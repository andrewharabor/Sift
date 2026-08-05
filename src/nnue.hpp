#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string_view>

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_SILENCE_BITCODE_WARNING

#include "incbin/incbin.h"

#undef INCBIN_ALIGNMENT
#define INCBIN_ALIGNMENT 64

#include "arch.hpp"
#include "bitboard.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "simd.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"


INCBIN(unsigned char, EMBEDDED_NETWORK, TOSTRING(NETWORK_FILE));

namespace Sift {

struct PSQFeature {
    Piece piece;
    Square square;

    constexpr PSQFeature() noexcept : piece(), square() {}

    constexpr PSQFeature(Piece piece, Square square) noexcept : piece(piece), square(square) {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
    }

    constexpr USize index(Color color, bool mirror) const noexcept {
        assert(color != Color::NONE);

        Square relativeSquare = (color == Color::WHITE) ? square : square.flipped();
        relativeSquare = (mirror) ? relativeSquare.mirrored() : relativeSquare;
        Piece relativePiece = (piece.color() == color) ? Piece(piece.type(), Color::WHITE) : Piece(piece.type(), Color::BLACK);
        relativePiece = (piece.type() == PieceType::KING) ? Piece::WHITE_KING : relativePiece;

        return static_cast<USize>(relativeSquare) + 64 * static_cast<USize>(relativePiece);
    }
};

struct RefreshEntry {
    alignas(64) std::array<Int16, Arch::L1_SIZE> data;

    constexpr RefreshEntry() noexcept : data(), piecesBitboards(), occupancyBitboards() {}

    constexpr void init(const std::array<Int16, Arch::L1_SIZE> &biases) noexcept { data = biases; }

    constexpr void update(const MultiArray<Int16, Arch::PSQ_SIZE, Arch::L1_SIZE> &weights, const Position &position, Color color, bool mirror) noexcept {
        std::array<USize, MAX_CHANGES> add;
        std::array<USize, MAX_CHANGES> sub;
        USize addSize = 0;
        USize subSize = 0;

        for (PieceType pieceType : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            for (Color pieceColor : {Color::WHITE, Color::BLACK}) {
                const Piece piece = Piece(pieceType, pieceColor);
                const Bitboard oldOccupancy = pieces(pieceType, pieceColor);
                const Bitboard newOccupancy = position.pieces(pieceType, pieceColor);

                Bitboard addOccupancy = newOccupancy & ~oldOccupancy;
                while (addOccupancy) {
                    const Square square = Square(addOccupancy.pop());

                    assert(addSize < MAX_CHANGES);
                    add[addSize++] = PSQFeature(piece, square).index(color, mirror);
                }

                Bitboard subOccupancy = oldOccupancy & ~newOccupancy;
                while (subOccupancy) {
                    const Square square = Square(subOccupancy.pop());

                    assert(subSize < MAX_CHANGES);
                    sub[subSize++] = PSQFeature(piece, square).index(color, mirror);
                }
            }
        }

        for (PieceType pieceType : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            piecesBitboards[static_cast<USize>(pieceType)] = position.pieces(pieceType);
        }

        for (Color pieceColor : {Color::WHITE, Color::BLACK}) {
            occupancyBitboards[static_cast<USize>(pieceColor)] = position.friendly(pieceColor);
        }

        while (addSize >= 2) {
            FusedUpdates::add2(data, weights[add[addSize - 1]], weights[add[addSize - 2]]);
            addSize -= 2;
        }

        while (addSize >= 1) {
            FusedUpdates::add1(data, weights[add[addSize - 1]]);
            addSize--;
        }

        while (subSize >= 2) {
            FusedUpdates::sub2(data, weights[sub[subSize - 1]], weights[sub[subSize - 2]]);
            subSize -= 2;
        }

        while (subSize >= 1) {
            FusedUpdates::sub1(data, weights[sub[subSize - 1]]);
            subSize--;
        }
    }

private:
    static constexpr USize MAX_CHANGES = 32;

    std::array<Bitboard, 6> piecesBitboards;
    std::array<Bitboard, 2> occupancyBitboards;

    constexpr Bitboard pieces(PieceType pieceType, Color color) const noexcept {
        assert(pieceType != PieceType::NONE && color != Color::NONE);
        return piecesBitboards[static_cast<USize>(pieceType)] & occupancyBitboards[static_cast<USize>(color)];
    }
};

class Accumulator {
public:
    enum class AccState : UInt8 {
        CLEAN,
        DIRTY,
        REFRESH
    };

    static constexpr AccState CLEAN = AccState::CLEAN;
    static constexpr AccState DIRTY = AccState::DIRTY;
    static constexpr AccState REFRESH = AccState::REFRESH;

    constexpr Accumulator() noexcept : data_(), states_({REFRESH, REFRESH}), add_(), sub_(), addSize_(0), subSize_(0) {}

    constexpr const MultiArray<Int16, 2, Arch::L1_SIZE> &data() const noexcept { return data_; }
    constexpr MultiArray<Int16, 2, Arch::L1_SIZE> &data() noexcept { return data_; }

    constexpr const std::array<Int16, Arch::L1_SIZE> &data(Color color) const noexcept {
        assert(color != Color::NONE);
        return data_[static_cast<USize>(color)];
    }

    constexpr std::array<Int16, Arch::L1_SIZE> &data(Color color) noexcept {
        assert(color != Color::NONE);
        return data_[static_cast<USize>(color)];
    }

    constexpr AccState state(Color color) const noexcept {
        assert(color != Color::NONE);
        return states_[static_cast<USize>(color)];
    }

    constexpr void mark(Color color, AccState newState) noexcept {
        assert(color != Color::NONE);
        states_[static_cast<USize>(color)] = newState;
    }

    constexpr void clearFeatures() noexcept {
        states_ = {DIRTY, DIRTY};
        addSize_ = 0;
        subSize_ = 0;
    }

    constexpr void addFeature(PSQFeature feature) noexcept {
        assert(addSize_ < 2);
        add_[addSize_++] = feature;
    }

    constexpr void subFeature(PSQFeature feature) noexcept {
        assert(subSize_ < 2);
        sub_[subSize_++] = feature;
    }

    constexpr void update(const MultiArray<Int16, Arch::PSQ_SIZE, Arch::L1_SIZE> &weights, const Accumulator &previous, Color color, bool mirror) noexcept {
        assert(state(color) == DIRTY);
        assert(previous.state(color) == CLEAN);
        assert(addSize_ >= 1);
        assert(subSize_ >= 1);
        assert(color != Color::NONE);

        data(color) = previous.data(color);

        const USize add1 = add_[0].index(color, mirror);
        const USize add2 = (addSize_ > 1) ? add_[1].index(color, mirror) : 0;
        const USize sub1 = sub_[0].index(color, mirror);
        const USize sub2 = (subSize_ > 1) ? sub_[1].index(color, mirror) : 0;

        if (addSize_ == 1 && subSize_ == 1) {
            FusedUpdates::add1Sub1(data(color), weights[add1], weights[sub1]);
        } else if (addSize_ == 1 && subSize_ == 2) {
            FusedUpdates::add1Sub2(data(color), weights[add1], weights[sub1], weights[sub2]);
        } else if (addSize_ == 2 && subSize_ == 2) {
            FusedUpdates::add2Sub2(data(color), weights[add1], weights[add2], weights[sub1], weights[sub2]);
        }

        mark(color, CLEAN);
    }

    constexpr void refresh(const RefreshEntry &refreshEntry, Color color) noexcept {
        assert(state(color) == REFRESH);
        assert(color != Color::NONE);

        data(color) = refreshEntry.data;
        mark(color, CLEAN);
    }

private:
    alignas(64) MultiArray<Int16, 2, Arch::L1_SIZE> data_;
    std::array<AccState, 2> states_;
    std::array<PSQFeature, 2> add_;
    std::array<PSQFeature, 2> sub_;
    USize addSize_;
    USize subSize_;
};

class SparsityIterator {
public:
#if defined(USE_SIMD)

    SparsityIterator() noexcept : indices_(), count_(0) { offset_ = SIMD::zeroInt16(); }

    inline void addNonzeros(VecUInt8 vecA, VecUInt8 vecB) noexcept {
#if defined(USE_AVX512)
        alignas(64) static constexpr std::array<Int16, 32> INDEX_TABLE = [] {
            std::array<Int16, 32> table = {};
            for (Int16 i = 0; i < 32; i++) {
                table[i] = i;
            }
            return table;
        }();
        UInt32 mask = SIMD::nonzeroMaskUInt8(vecA) | (SIMD::nonzeroMaskUInt8(vecB) << 16);
        VecInt16 indexTable = SIMD::loadInt16(INDEX_TABLE.data());
        indexTable = SIMD::addInt16(indexTable, offset_);
        assert(count_ + std::popcount(mask) <= indices_.size());
        _mm512_mask_compressstoreu_epi16(&indices_[count_], mask, indexTable);
        count_ += std::popcount(mask);
        offset_ = SIMD::addInt16(offset_, SIMD::setInt16(32));
#elif defined(USE_AVX2) || defined(USE_SSE4)
#if defined(USE_AVX2)
        UInt16 mask = static_cast<UInt16>(SIMD::nonzeroMaskUInt8(vecA) | (SIMD::nonzeroMaskUInt8(vecB) << 8));
        __m128i offset = _mm256_castsi256_si128(offset_);
#elif defined(USE_SSE4)
        UInt8 mask = static_cast<UInt8>(SIMD::nonzeroMaskUInt8(vecA) | (SIMD::nonzeroMaskUInt8(vecB) << 4));
        __m128i offset = offset_;
#endif
        for (USize i = 0; i < sizeof(mask); i++) {
            UInt8 byteMask = (mask >> (i * 8)) & 0xFF;
            const TableEntry &entry = SPARSE_INDEX_TABLE.entries[byteMask];
            __m128i indexTable = _mm_load_si128(reinterpret_cast<const __m128i *>(entry.indices.data()));
            indexTable = _mm_add_epi16(indexTable, offset);
            assert(count_ + entry.count <= indices_.size());
            __m128i *end = reinterpret_cast<__m128i *>(&indices_[count_]);
            _mm_storeu_si128(end, indexTable);
            count_ += entry.count;
            offset = _mm_add_epi16(offset, _mm_set1_epi16(8));
        }
#if defined(USE_AVX2)
        offset_ = _mm256_castsi128_si256(offset);
#elif defined(USE_SSE4)
        offset_ = offset;
#endif
#elif defined(USE_NEON)
        UInt8 mask = static_cast<UInt8>(SIMD::nonzeroMaskUInt8(vecA) | (SIMD::nonzeroMaskUInt8(vecB) << 4));
        const TableEntry &entry = SPARSE_INDEX_TABLE.entries[mask];
        VecInt16 indexTable = SIMD::loadInt16(entry.indices.data());
        indexTable = SIMD::addInt16(indexTable, offset_);
        assert(count_ + entry.count <= indices_.size());
        SIMD::storeInt16(&indices_[count_], indexTable);
        count_ += entry.count;
        offset_ = SIMD::addInt16(offset_, SIMD::setInt16(8));
#endif
    }

    constexpr USize index(USize i) const noexcept {
        assert(i < count_);
        return static_cast<USize>(indices_[i]);
    }

    constexpr USize count() const noexcept { return count_; }

private:
    struct TableEntry {
        alignas(16) std::array<Int16, 8> indices;
        USize count;
    };

    struct IndexTable {
        constexpr IndexTable() noexcept {
            for (UInt64 i = 0; i < 256; i++) {
                UInt64 bits = i;
                USize idx = 0;
                while (bits) {
                    Int16 lsb = static_cast<Int16>(std::countr_zero(bits));
                    bits &= bits - 1;
                    entries[i].indices[idx++] = lsb;
                }
                entries[i].count = idx;
            }
        }

        std::array<TableEntry, 256> entries;
    };

    static inline IndexTable SPARSE_INDEX_TABLE = IndexTable();

    std::array<Int16, Arch::L1_SIZE / 4> indices_;
    USize count_;

    VecInt16 offset_;

#endif
};

class NNUEState {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    NNUEState(const MultiArray<Int16, Arch::KING_BUCKETS, Arch::PSQ_SIZE, Arch::L1_SIZE> &ftWeights, const std::array<Int16, Arch::L1_SIZE> &ftBiases) :ftWeights_(ftWeights), accumulators_(), ply_(0), refreshTable_() {
        for (Color color : {Color::WHITE, Color::BLACK}) {
            for (bool mirror : {false, true}) {
                for (USize kBucket = 0; kBucket < Arch::KING_BUCKETS; kBucket++) {
                    refreshTable_[static_cast<USize>(color)][mirror][kBucket].init(ftBiases);
                }
            }
        }
    }

    constexpr void set(const Position &position) noexcept {
        ply_ = 0;
        for (Color color : {Color::WHITE, Color::BLACK}) {
            const bool mirr = mirror(position.kingSquare(color));
            const USize kBucket = kingBucket(position.kingSquare(color), color);
            accumulators_[0].mark(color, Accumulator::REFRESH);
            RefreshEntry &refreshEntry = refreshTable_[static_cast<USize>(color)][mirr][kBucket];
            refreshEntry.update(ftWeights_[kBucket], position, color, mirr);
            accumulators_[0].refresh(refreshEntry, color);
        }
    }

    constexpr void update(const Position &position, Color color) noexcept {
        const bool mirr = mirror(position.kingSquare(color));
        const USize kBucket = kingBucket(position.kingSquare(color), color);

        if (accumulators_[ply_].state(color) == Accumulator::REFRESH) {
            RefreshEntry &refreshEntry = refreshTable_[static_cast<USize>(color)][mirr][kBucket];
            refreshEntry.update(ftWeights_[kBucket], position, color, mirr);
            accumulators_[ply_].refresh(refreshEntry, color);
        } else {
            USize idx_ = ply_;
            while (idx_ > 0 && accumulators_[idx_].state(color) == Accumulator::DIRTY) {
                idx_--;
            }

            while (idx_ < ply_) {
                idx_++;
                accumulators_[idx_].update(ftWeights_[kBucket], accumulators_[idx_ - 1], color, mirr);
            }
        }
    }

    constexpr void makeMove(const Position &position, Move move) noexcept {
        assert(ply_ < MAX_PLY);
        ply_++;

        Accumulator &acc = accumulators_[ply_];
        acc.clearFeatures();

        const Color color = position.sideToMove();
        const Piece movedPiece = position.pieceAt(move.from());
        const Piece capturedPiece = position.pieceAt(move.to());

        acc.subFeature(PSQFeature(position.pieceAt(move.from()), move.from()));

        if (move.type() == MoveType::PROMOTION) {
            const Piece promotion = Piece(move.promotion(), color);
            acc.addFeature(PSQFeature(promotion, move.to()));
        } else if (move.type() == MoveType::CASTLING) {
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), color);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            acc.addFeature(PSQFeature(Piece(PieceType::ROOK, color), rookTo));
            acc.addFeature(PSQFeature(Piece(PieceType::KING, color), kingTo));
        } else {
            acc.addFeature(PSQFeature(position.pieceAt(move.from()), move.to()));
        }

        if (capturedPiece != Piece::NONE) {
            acc.subFeature(PSQFeature(capturedPiece, move.to()));
        } else if (move.type() == MoveType::EN_PASSANT) {
            const Square enPassantSquare = move.to().enPassant();
            acc.subFeature(PSQFeature(Piece(PieceType::PAWN, ~color), enPassantSquare));
        }

        for (Color col : {Color::WHITE, Color::BLACK}) {
            if (accumulators_[ply_ - 1].state(col) == Accumulator::REFRESH) {
                acc.mark(col, Accumulator::REFRESH);
            }
        }

        if (movedPiece.type() == PieceType::KING && ((mirror(move.to()) != mirror(move.from())) || (kingBucket(move.to(), color) != kingBucket(move.from(), color)))) {
            acc.mark(color, Accumulator::REFRESH);
        }
    }

    constexpr void unmakeMove() noexcept {
        assert(ply_ > 0);
        ply_--;
    }

    const Accumulator &topAccumulator(const Position &position) noexcept {
        update(position, Color::WHITE);
        update(position, Color::BLACK);
        assert(accumulators_[ply_].state(Color::WHITE) == Accumulator::CLEAN);
        assert(accumulators_[ply_].state(Color::BLACK) == Accumulator::CLEAN);
        return accumulators_[ply_];
    }

private:
    const MultiArray<Int16, Arch::KING_BUCKETS, Arch::PSQ_SIZE, Arch::L1_SIZE> &ftWeights_;

    Accumulator accumulators_[MAX_PLY + 1];
    USize ply_;

    MultiArray<RefreshEntry, 2, 2, Arch::KING_BUCKETS> refreshTable_;

    constexpr bool mirror(Square kingSquare) const noexcept { return kingSquare.file() > File::D; }

    constexpr USize kingBucket(Square kingSquare, Color color) const noexcept {
        const bool mirr = mirror(kingSquare);
        Square relativeSquare = (mirr) ? kingSquare.mirrored() : kingSquare;
        relativeSquare = (color == Color::WHITE) ? relativeSquare : relativeSquare.flipped();
        return Arch::KING_BUCKET_LAYOUT[4 * static_cast<USize>(relativeSquare.rank()) + static_cast<USize>(relativeSquare.file())];
    }
};

class NNUE {
public:
    NNUE() : params_(loadParams()), state_(params_->ftWeights, params_->ftBiases) {}

    constexpr NNUEState &state() noexcept { return state_; }

    inline Int32 forward(const Position &position) noexcept {
        const Color color = position.sideToMove();
        const Accumulator &acc = state_.topAccumulator(position);
        const std::array<Int16, Arch::L1_SIZE> &friendlyAcc = acc.data(color);
        const std::array<Int16, Arch::L1_SIZE> &enemyAcc = acc.data(~color);

        static constexpr USize OUTPUT_BUCKET_DIV = (32 + Arch::OUTPUT_BUCKETS - 1) / Arch::OUTPUT_BUCKETS;
        const USize outputBucket = (position.occupied().count() - 2) / OUTPUT_BUCKET_DIV;

        SparsityIterator sparsityIter = SparsityIterator();

        alignas(64) std::array<UInt8, Arch::L1_SIZE> l0Out;
        alignas(64) std::array<Int32, Arch::L2_SIZE> l1Out;

        forwardL0Half(l0Out, 0, friendlyAcc, sparsityIter);
        forwardL0Half(l0Out, Arch::L1_SIZE / 2, enemyAcc, sparsityIter);
        forwardL1(l1Out, l0Out, outputBucket, sparsityIter);
        Int64 score = forwardL2L3(l1Out, outputBucket);

#if defined(MEASURE_SPARSITY)
        addFTActs(l0Out);
#endif

        score *= static_cast<Int64>(Arch::SCALE);
        score /= static_cast<Int64>(Arch::QUANT_C * Arch::QUANT_C * Arch::QUANT_C * Arch::QUANT_C);
        return static_cast<Int32>(score);
    }

    inline Int32 evaluate(const Position &position) noexcept {
        Int32 score = forward(position);

        const Int32 materialAdjust = EVAL_ADJUST_PAWN_SCALE * position.pieces(PieceType::PAWN).count() + EVAL_ADJUST_KNIGHT_SCALE * position.pieces(PieceType::KNIGHT).count() + EVAL_ADJUST_BISHOP_SCALE * position.pieces(PieceType::BISHOP).count() + EVAL_ADJUST_ROOK_SCALE * position.pieces(PieceType::ROOK).count() + EVAL_ADJUST_QUEEN_SCALE * position.pieces(PieceType::QUEEN).count();

        score = score * (EVAL_ADJUST_MATERIAL_BASE + materialAdjust) / EVAL_ADJUST_MATERIAL_DIVISOR;
        score = score * (EVAL_ADJUST_HALF_MOVE_SCALE - position.halfmoveClock()) / EVAL_ADJUST_HALF_MOVE_SCALE;
        score = std::clamp(score, Score::LOSS + 1, Score::WIN - 1);

        return score;
    }

    Int32 scale(std::string_view path) noexcept {
        static constexpr Float64 TARGET_AVG_ABS_EVAL = 553.065;

        std::ifstream file = std::ifstream(path.data());

        UInt64 total = 0;
        UInt64 count = 0;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream stream = std::istringstream(line);
            std::string fen;
            for (USize i = 0; i < 6; i++) {
                if (i > 0) {
                    fen += " ";
                }
                std::string token;
                stream >> token;
                fen += token;
            }

            Position position = Position(fen);
            state_.set(position);
            total += static_cast<UInt64>(std::abs(forward(position)));
            count++;
        }

        file.close();

        Float64 avgAbsEval = static_cast<Float64>(total) / static_cast<Float64>(count);
        Float64 scale = TARGET_AVG_ABS_EVAL * Arch::SCALE / avgAbsEval;
        return static_cast<Int32>(std::round(scale));
    }

#if defined(MEASURE_SPARSITY)

    static void addFTActs(const std::array<UInt8, Arch::L1_SIZE> &l0Out) noexcept {
        for (USize i = 0; i < Arch::L1_SIZE / 2; i++) {
            if (l0Out[i]) {
                ftActs[i]++;
            }
        }

        for (USize i = 0; i < Arch::L1_SIZE / 2; i += 4) {
            bool nonzero = false;
            for (USize j = 0; j < 4; j++) {
                if (l0Out[i + j]) {
                    nonzero = true;
                    break;
                }
            }

            if (nonzero) {
                nonzeroActs++;
            }
        }

        totalCalls++;
    }

    static UInt64 saveFTActs(std::string_view path) noexcept {
        std::ofstream file = std::ofstream(path.data());
        assert(file.is_open());

        file << "[ ";
        for (USize i = 0; i < Arch::L1_SIZE / 2; i++) {
            if (i > 0) {
                file << ", ";
            }
            file << ftActs[i];
        }
        file << " ]" << std::endl;
        file.close();

        assert(totalCalls > 0);
        return nonzeroActs / totalCalls;
    }

#endif

private:
    const NetParams *params_;
    NNUEState state_;

    const NetParams *loadParams() noexcept {
        assert(64 * ((sizeof(NetParams) + 63) / 64) == EMBEDDED_NETWORK_size);
        assert(reinterpret_cast<uintptr_t>(EMBEDDED_NETWORK_data) % alignof(NetParams) == 0);
        return reinterpret_cast<const NetParams *>(EMBEDDED_NETWORK_data);
    }

#if defined(MEASURE_SPARSITY)
    static inline std::array<UInt64, Arch::L1_SIZE / 2> ftActs = {};
    static inline UInt64 nonzeroActs = 0;
    static inline UInt64 totalCalls = 0;
#endif

    inline void forwardL0Half(std::array<UInt8, Arch::L1_SIZE> &l0Out, USize offset, const std::array<Int16, Arch::L1_SIZE> &acc, [[maybe_unused]] SparsityIterator &sparsityIter) noexcept {
#if defined(USE_SIMD)
        static_assert((Arch::L1_SIZE / 2) % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = (Arch::L1_SIZE / 2) / SIMD::WIDTH16;

        const VecInt16 zero = SIMD::zeroInt16();
        const VecInt16 quant = SIMD::setInt16(static_cast<Int16>(Arch::QUANT_A));
        for (USize i = 0; i < ITERS4; i += 4) {
            const VecInt16 clamp0Vec0 = SIMD::clampInt16(SIMD::loadInt16(&acc[(i + 0) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp0Vec1 = SIMD::clampInt16(SIMD::loadInt16(&acc[(i + 1) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp0Vec2 = SIMD::clampInt16(SIMD::loadInt16(&acc[(i + 2) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp0Vec3 = SIMD::clampInt16(SIMD::loadInt16(&acc[(i + 3) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp1Vec0 = SIMD::clampInt16(SIMD::loadInt16(&acc[Arch::L1_SIZE / 2 + (i + 0) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp1Vec1 = SIMD::clampInt16(SIMD::loadInt16(&acc[Arch::L1_SIZE / 2 + (i + 1) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp1Vec2 = SIMD::clampInt16(SIMD::loadInt16(&acc[Arch::L1_SIZE / 2 + (i + 2) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 clamp1Vec3 = SIMD::clampInt16(SIMD::loadInt16(&acc[Arch::L1_SIZE / 2 + (i + 3) * SIMD::WIDTH16]), zero, quant);
            const VecInt16 prodVec0 = SIMD::lShiftMulHiInt16(clamp0Vec0, clamp1Vec0, 7);
            const VecInt16 prodVec1 = SIMD::lShiftMulHiInt16(clamp0Vec1, clamp1Vec1, 7);
            const VecInt16 prodVec2 = SIMD::lShiftMulHiInt16(clamp0Vec2, clamp1Vec2, 7);
            const VecInt16 prodVec3 = SIMD::lShiftMulHiInt16(clamp0Vec3, clamp1Vec3, 7);
            const VecUInt8 packedVec0 = SIMD::packUsInt16(prodVec0, prodVec1);
            const VecUInt8 packedVec1 = SIMD::packUsInt16(prodVec2, prodVec3);
            SIMD::storeUInt8(&l0Out[offset + (i + 0) * SIMD::WIDTH16], packedVec0);
            SIMD::storeUInt8(&l0Out[offset + (i + 2) * SIMD::WIDTH16], packedVec1);
            sparsityIter.addNonzeros(packedVec0, packedVec1);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE / 2; i++) {
            const Int32 clamp0 = std::clamp(static_cast<Int32>(acc[i]), 0, Arch::QUANT_A);
            const Int32 clamp1 = std::clamp(static_cast<Int32>(acc[i + Arch::L1_SIZE / 2]), 0, Arch::QUANT_A);
            l0Out[offset + i] = static_cast<UInt8>(((clamp0 << 7) * clamp1) >> 16);
        }
#endif
    }

    inline void forwardL1(std::array<Int32, Arch::L2_SIZE> &l1Out, const std::array<UInt8, Arch::L1_SIZE> &l0Out, USize outputBucket, [[maybe_unused]] SparsityIterator &sparsityIter) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % 16 == 0);
        static_assert(Arch::L2_SIZE % SIMD::WIDTH32 == 0);
        static constexpr USize ITERS = Arch::L2_SIZE / SIMD::WIDTH32;

        const USize nonzeros = sparsityIter.count();
        const USize nonzeros4 = (nonzeros / 4) * 4;

        MultiArray<VecInt32, ITERS, 4> matMul;
        for (USize i = 0; i < ITERS; i++) {
            matMul[i][0] = SIMD::zeroInt32();
            matMul[i][1] = SIMD::zeroInt32();
            matMul[i][2] = SIMD::zeroInt32();
            matMul[i][3] = SIMD::zeroInt32();
        }

        for (USize j = 0; j < nonzeros4; j += 4) {
            const USize nonzeroIdx0 = sparsityIter.index(j + 0);
            const USize nonzeroIdx1 = sparsityIter.index(j + 1);
            const USize nonzeroIdx2 = sparsityIter.index(j + 2);
            const USize nonzeroIdx3 = sparsityIter.index(j + 3);
            const VecUInt8 inputVec0 = SIMD::tileUInt8(&l0Out[nonzeroIdx0 * 4]);
            const VecUInt8 inputVec1 = SIMD::tileUInt8(&l0Out[nonzeroIdx1 * 4]);
            const VecUInt8 inputVec2 = SIMD::tileUInt8(&l0Out[nonzeroIdx2 * 4]);
            const VecUInt8 inputVec3 = SIMD::tileUInt8(&l0Out[nonzeroIdx3 * 4]);

            for (USize i = 0; i < ITERS; i++) {
                const VecInt8 weightsVec0 = SIMD::loadInt8(&params_->l1Weights[outputBucket][nonzeroIdx0][i * SIMD::WIDTH8]);
                const VecInt8 weightsVec1 = SIMD::loadInt8(&params_->l1Weights[outputBucket][nonzeroIdx1][i * SIMD::WIDTH8]);
                const VecInt8 weightsVec2 = SIMD::loadInt8(&params_->l1Weights[outputBucket][nonzeroIdx2][i * SIMD::WIDTH8]);
                const VecInt8 weightsVec3 = SIMD::loadInt8(&params_->l1Weights[outputBucket][nonzeroIdx3][i * SIMD::WIDTH8]);
                matMul[i][0] = SIMD::dotProdUInt8Int8(matMul[i][0], inputVec0, weightsVec0);
                matMul[i][1] = SIMD::dotProdUInt8Int8(matMul[i][1], inputVec1, weightsVec1);
                matMul[i][2] = SIMD::dotProdUInt8Int8(matMul[i][2], inputVec2, weightsVec2);
                matMul[i][3] = SIMD::dotProdUInt8Int8(matMul[i][3], inputVec3, weightsVec3);
            }
        }

        for (USize j = nonzeros4; j < nonzeros; j++) {
            const USize nonzeroIdx = sparsityIter.index(j);
            const VecUInt8 inputVec = SIMD::tileUInt8(&l0Out[nonzeroIdx * 4]);
            for (USize i = 0; i < ITERS; i++) {
                const VecInt8 weightsVec = SIMD::loadInt8(&params_->l1Weights[outputBucket][nonzeroIdx][i * SIMD::WIDTH8]);
                matMul[i][0] = SIMD::dotProdUInt8Int8(matMul[i][0], inputVec, weightsVec);
            }
        }

        const VecInt32 zero = SIMD::zeroInt32();
        const VecInt32 quant = SIMD::setInt32(Arch::QUANT_C << Arch::SHIFT);
        for (USize i = 0; i < ITERS; i++) {
            const VecInt32 sumVec0 = SIMD::addInt32(matMul[i][0], matMul[i][1]);
            const VecInt32 sumVec1 = SIMD::addInt32(matMul[i][2], matMul[i][3]);
            const VecInt32 preVec = SIMD::addInt32(SIMD::addInt32(sumVec0, sumVec1), SIMD::loadInt32(&params_->l1Biases[outputBucket][i * SIMD::WIDTH32]));
            const VecInt32 creluVec = SIMD::clampInt32(preVec, zero, quant);
            const VecInt32 screluVec = SIMD::rShiftInt32(SIMD::mulLoInt32(creluVec, creluVec), 2 * Arch::SHIFT);
            SIMD::storeInt32(&l1Out[i * SIMD::WIDTH32], screluVec);
        }
#else
        constexpr USize TILES = Arch::L1_SIZE / 4;
        std::array<Int32, Arch::L2_SIZE> matMul = {0};

        for (USize i = 0; i < Arch::L2_SIZE; i++) {
            for (USize j = 0; j < TILES; j++) {
                for (USize k = 0; k < 4; k++) {
                    matMul[i] += static_cast<Int32>(l0Out[4 * j + k]) * static_cast<Int32>(params_->l1Weights[outputBucket][j][4 * i + k]);
                }
            }
        }

        for (USize i = 0; i < Arch::L2_SIZE; i++) {
            const Int32 pre = matMul[i] + params_->l1Biases[outputBucket][i];
            const Int32 crelu = std::clamp(pre, 0, Arch::QUANT_C << Arch::SHIFT);
            const Int32 screlu = (crelu * crelu) >> (2 * Arch::SHIFT);
            l1Out[i] = screlu;
        }
#endif
    }

    inline Int64 forwardL2L3(const std::array<Int32, Arch::L2_SIZE> &l1Out, USize outputBucket) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L3_SIZE % SIMD::WIDTH32 == 0);
        static constexpr USize ITERS = Arch::L3_SIZE / SIMD::WIDTH32;

        std::array<VecInt32, ITERS> l2Pre;
        for (USize i = 0; i < ITERS; i++) {
            l2Pre[i] = SIMD::loadInt32(&params_->l2Biases[outputBucket][i * SIMD::WIDTH32]);
        }

        for (USize j = 0; j < Arch::L2_SIZE; j++) {
            const VecInt32 inputVec = SIMD::setInt32(l1Out[j]);
            for (USize i = 0; i < ITERS; i++) {
                const VecInt32 weightsVec = SIMD::loadInt32(&params_->l2Weights[outputBucket][j][i * SIMD::WIDTH32]);
                l2Pre[i] = SIMD::addInt32(l2Pre[i], SIMD::mulLoInt32(inputVec, weightsVec));
            }
        }

        Int64 l3Out = static_cast<Int64>(params_->l3Biases[outputBucket]);

        VecInt32 sumVec = SIMD::zeroInt32();
        const VecInt32 zero = SIMD::zeroInt32();
        const VecInt32 quant = SIMD::setInt32(Arch::QUANT_C * Arch::QUANT_C * Arch::QUANT_C);
        for (USize i = 0; i < ITERS; i++) {
            const VecInt32 creluVec = SIMD::clampInt32(l2Pre[i], zero, quant);
            const VecInt32 weightsVec = SIMD::loadInt32(&params_->l3Weights[outputBucket][i * SIMD::WIDTH32]);
            sumVec = SIMD::addInt32(sumVec, SIMD::mulLoInt32(creluVec, weightsVec));
        }

        l3Out += static_cast<Int64>(SIMD::horizAddInt32(sumVec));
        return l3Out;
#else
        std::array<Int32, Arch::L3_SIZE> l2Pre = params_->l2Biases[outputBucket];
        for (USize i = 0; i < Arch::L2_SIZE; i++) {
            for (USize j = 0; j < Arch::L3_SIZE; j++) {
                l2Pre[j] += l1Out[i] * params_->l2Weights[outputBucket][i][j];
            }
        }

        Int64 l3Out = static_cast<Int64>(params_->l3Biases[outputBucket]);
        for (USize i = 0; i < Arch::L3_SIZE; i++) {
            const Int32 crelu = std::clamp(l2Pre[i], 0, Arch::QUANT_C * Arch::QUANT_C * Arch::QUANT_C);
            l3Out += static_cast<Int64>(crelu) * static_cast<Int64>(params_->l3Weights[outputBucket][i]);
        }

        return l3Out;
#endif
    }
};

}
