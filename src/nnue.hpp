#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
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

namespace Syft {

struct InputFeature {
    Piece piece;
    Square square;

    constexpr InputFeature() noexcept : piece(), square() {}

    constexpr InputFeature(Piece piece, Square square) noexcept : piece(piece), square(square) {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
    }

    constexpr USize index(Color color, bool mirror) const noexcept {
        assert(color != Color::NONE);

        Square relativeSquare = (color == Color::WHITE) ? square : square.flipped();
        relativeSquare = (mirror) ? relativeSquare.mirrored() : relativeSquare;
        Piece relativePiece = (piece.color() == color) ? Piece(piece.type(), Color::WHITE) : Piece(piece.type(), Color::BLACK);
        // TODO: king-plane merging
        // relativePiece = (piece.type() == PieceType::KING) ? Piece::WHITE_KING : relativePiece;

        return static_cast<USize>(relativeSquare) + 64 * static_cast<USize>(relativePiece);
    }
};

struct RefreshEntry {
    alignas(64) LayerVector data;

    constexpr RefreshEntry() noexcept : data(), piecesBitboards(), occupancyBitboards() {}

    constexpr void init(const LayerVector &biases) noexcept { data = biases; }

    constexpr void update(const FeatureMatrix &weights, const Position &position, Color color, bool mirror) noexcept {
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
                    add[addSize++] = InputFeature(piece, square).index(color, mirror);
                }

                Bitboard subOccupancy = oldOccupancy & ~newOccupancy;
                while (subOccupancy) {
                    const Square square = Square(subOccupancy.pop());

                    assert(subSize < MAX_CHANGES);
                    sub[subSize++] = InputFeature(piece, square).index(color, mirror);
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
            SIMD::add2(data, weights[add[addSize - 1]], weights[add[addSize - 2]]);
            addSize -= 2;
        }

        while (addSize >= 1) {
            SIMD::add1(data, weights[add[addSize - 1]]);
            addSize--;
        }

        while (subSize >= 2) {
            SIMD::sub2(data, weights[sub[subSize - 1]], weights[sub[subSize - 2]]);
            subSize -= 2;
        }

        while (subSize >= 1) {
            SIMD::sub1(data, weights[sub[subSize - 1]]);
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

    constexpr const DualLayerVector &data() const noexcept { return data_; }
    constexpr DualLayerVector &data() noexcept { return data_; }

    constexpr const LayerVector &data(Color color) const noexcept {
        assert(color != Color::NONE);
        return data_[static_cast<USize>(color)];
    }

    constexpr LayerVector &data(Color color) noexcept {
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

    constexpr void addFeature(InputFeature feature) noexcept {
        assert(addSize_ < 2);
        add_[addSize_++] = feature;
    }

    constexpr void subFeature(InputFeature feature) noexcept {
        assert(subSize_ < 2);
        sub_[subSize_++] = feature;
    }

    constexpr void update(const FeatureMatrix &weights, const Accumulator &previous, Color color, bool mirror) noexcept {
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
            SIMD::add1Sub1(data(color), weights[add1], weights[sub1]);
        } else if (addSize_ == 1 && subSize_ == 2) {
            SIMD::add1Sub2(data(color), weights[add1], weights[sub1], weights[sub2]);
        } else if (addSize_ == 2 && subSize_ == 2) {
            SIMD::add2Sub2(data(color), weights[add1], weights[add2], weights[sub1], weights[sub2]);
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
    alignas(64) DualLayerVector data_;
    std::array<AccState, 2> states_;
    std::array<InputFeature, 2> add_;
    std::array<InputFeature, 2> sub_;
    USize addSize_;
    USize subSize_;
};

class NNUE {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    NNUE() noexcept : params_(), accumulators_(), ply_(0), refreshTable_() {
        assert(64 * ((sizeof(Params) + 63) / 64) == EMBEDDED_NETWORK_size);
        assert(reinterpret_cast<uintptr_t>(EMBEDDED_NETWORK_data) % alignof(Params) == 0);

        params_ = reinterpret_cast<const Params *>(EMBEDDED_NETWORK_data);

        for (Color color : {Color::WHITE, Color::BLACK}) {
            for (bool mirror : {false, true}) {
                for (USize kBucket = 0; kBucket < Arch::KING_BUCKETS; kBucket++) {
                    refreshTable_[static_cast<USize>(color)][mirror][kBucket].init(params_->featureBiases);
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
            refreshEntry.update(params_->featureWeights[kBucket], position, color, mirr);
            accumulators_[0].refresh(refreshEntry, color);
        }
    }

    constexpr void update(const Position &position, Color color) noexcept {
        const bool mirr = mirror(position.kingSquare(color));
        const USize kBucket = kingBucket(position.kingSquare(color), color);

        if (accumulators_[ply_].state(color) == Accumulator::REFRESH) {
            RefreshEntry &refreshEntry = refreshTable_[static_cast<USize>(color)][mirr][kBucket];
            refreshEntry.update(params_->featureWeights[kBucket], position, color, mirr);
            accumulators_[ply_].refresh(refreshEntry, color);
        } else {
            USize idx_ = ply_;
            while (idx_ > 0 && accumulators_[idx_].state(color) == Accumulator::DIRTY) {
                idx_--;
            }

            while (idx_ < ply_) {
                idx_++;
                accumulators_[idx_].update(params_->featureWeights[kBucket], accumulators_[idx_ - 1], color, mirr);
            }
        }
    }

    inline Int32 evaluate(const Position &position) noexcept {
        Int32 score = forward(position);

        const Int32 materialAdjust = EVAL_ADJUST_PAWN_SCALE * position.pieces(PieceType::PAWN).count() + EVAL_ADJUST_KNIGHT_SCALE * position.pieces(PieceType::KNIGHT).count() + EVAL_ADJUST_BISHOP_SCALE * position.pieces(PieceType::BISHOP).count() + EVAL_ADJUST_ROOK_SCALE * position.pieces(PieceType::ROOK).count() + EVAL_ADJUST_QUEEN_SCALE * position.pieces(PieceType::QUEEN).count();

        score = score * (EVAL_ADJUST_MATERIAL_BASE + materialAdjust) / EVAL_ADJUST_MATERIAL_DIVISOR;
        score = score * (EVAL_ADJUST_HALF_MOVE_SCALE - position.halfmoveClock()) / EVAL_ADJUST_HALF_MOVE_SCALE;
        score = std::clamp(score, Score::LOSS + 1, Score::WIN - 1);

        return score;
    }

    inline Int32 forward(const Position &position) noexcept {
        update(position, Color::WHITE);
        update(position, Color::BLACK);

        const Color color = position.sideToMove();

        assert(accumulators_[ply_].state(color) == Accumulator::CLEAN);
        assert(accumulators_[ply_].state(~color) == Accumulator::CLEAN);

        const USize bucketIndex = (position.occupied().count() - 2) / Arch::OUTPUT_BUCKET_DIV;

        Int32 score = SIMD::activate(accumulators_[ply_].data(color), accumulators_[ply_].data(~color), params_->layerWeights[bucketIndex]);
        score /= Arch::QUANT_A;
        score += static_cast<Int32>(params_->layerBiases[bucketIndex]);
        score *= Arch::SCALE;
        score /= (Arch::QUANT_A * Arch::QUANT_B);
        return score;
    }

    constexpr void makeMove(const Position &position, Move move) noexcept {
        assert(ply_ < MAX_PLY);
        ply_++;

        Accumulator &acc = accumulators_[ply_];
        acc.clearFeatures();

        const Color color = position.sideToMove();
        const Piece movedPiece = position.pieceAt(move.from());
        const Piece capturedPiece = position.pieceAt(move.to());

        acc.subFeature(InputFeature(position.pieceAt(move.from()), move.from()));

        if (move.type() == MoveType::PROMOTION) {
            const Piece promotion = Piece(move.promotion(), color);
            acc.addFeature(InputFeature(promotion, move.to()));
        } else if (move.type() == MoveType::CASTLING) {
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), color);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            acc.addFeature(InputFeature(Piece(PieceType::ROOK, color), rookTo));
            acc.addFeature(InputFeature(Piece(PieceType::KING, color), kingTo));
        } else {
            acc.addFeature(InputFeature(position.pieceAt(move.from()), move.to()));
        }

        if (capturedPiece != Piece::NONE) {
            acc.subFeature(InputFeature(capturedPiece, move.to()));
        } else if (move.type() == MoveType::EN_PASSANT) {
            const Square enPassantSquare = move.to().enPassant();
            acc.subFeature(InputFeature(Piece(PieceType::PAWN, ~color), enPassantSquare));
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

private:
    struct Params {
        alignas(64) std::array<FeatureMatrix, Arch::KING_BUCKETS> featureWeights;
        alignas(64) LayerVector featureBiases;
        alignas(64) std::array<DualLayerVector, Arch::OUTPUT_BUCKETS> layerWeights;
        alignas(64) std::array<Int16, Arch::OUTPUT_BUCKETS> layerBiases;
    };

    const Params *params_;

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

}
