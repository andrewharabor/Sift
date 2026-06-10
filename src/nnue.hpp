#pragma once

#include <algorithm>
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
#include "coords.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "simd.hpp"
#include "tunable.hpp"
#include "types.hpp"
#include "utils.hpp"


INCBIN(unsigned char, INTERNAL_EVAL_FILE, TOSTRING(EVAL_FILE));

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
            add1Sub1(weights[add1], weights[sub1], color);
        } else if (addSize_ == 1 && subSize_ == 2) {
            add1Sub2(weights[add1], weights[sub1], weights[sub2], color);
        } else if (addSize_ == 2 && subSize_ == 2) {
            add2Sub2(weights[add1], weights[add2], weights[sub1], weights[sub2], color);
        }

        mark(color, CLEAN);
    }

    constexpr void refresh(const FeatureMatrix &weights, const LayerVector &biases, const Position &position, Color color, bool mirror) noexcept {
        assert(state(color) == REFRESH);
        assert(color != Color::NONE);

        data(color) = biases;
        Bitboard occupied = position.occupied();
        while (occupied) {
            const Square square = static_cast<Square>(occupied.pop());
            const Piece piece = position.pieceAt(square);
            const USize featureIndex = InputFeature(piece, square).index(color, mirror);
            add1(weights[featureIndex], color);
        }

        mark(color, CLEAN);
    }

private:
    alignas(64) DualLayerVector data_;
    std::array<AccState, 2> states_;
    std::array<InputFeature, 2> add_;
    std::array<InputFeature, 2> sub_;
    USize addSize_;
    USize subSize_;

    constexpr void add1(const LayerVector &add1, Color color) noexcept {
        LayerVector &dataVector = data(color);
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVector[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add1[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVector[i], dataReg1);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVector[i] += add1[i];
        }
#endif
    }

    constexpr void sub1(const LayerVector &sub1, Color color) noexcept {
        LayerVector &dataVector = data(color);
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVector[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub1[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVector[i], dataReg1);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVector[i] -= sub1[i];
        }
#endif
    }

    constexpr void add1Sub1(const LayerVector &add1, const LayerVector &sub1, Color color) noexcept {
        LayerVector &dataVector = data(color);
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVector[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add1[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub1[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVector[i], dataReg1);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVector[i] += add1[i] - sub1[i];
        }
#endif
    }

    constexpr void add1Sub2(const LayerVector &add1, const LayerVector &sub1, const LayerVector &sub2, Color color) noexcept {
        LayerVector &dataVector = data(color);
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVector[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add1[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub1[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub2[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub2[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub2[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub2[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVector[i], dataReg1);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVector[i] += add1[i] - sub1[i] - sub2[i];
        }
#endif
    }

    constexpr void add2Sub2(const LayerVector &add1, const LayerVector &add2, const LayerVector &sub1, const LayerVector &sub2, Color color) noexcept {
        LayerVector &dataVector = data(color);
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVector[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVector[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add1[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add2[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add2[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add2[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add2[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub1[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub2[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub2[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub2[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub2[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVector[i], dataReg1);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVector[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVector[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
#endif
    }
};

class NNUE {
public:
    static constexpr USize MAX_PLY = static_cast<USize>(Score::MAX_PLY);

    NNUE() noexcept : params_(), accumulators_(), ply_(0) { loadInternal(); }

    void loadInternal() noexcept {
        assert(64 * ((sizeof(Params) + 63) / 64) == INTERNAL_EVAL_FILE_size);
        assert(reinterpret_cast<uintptr_t>(INTERNAL_EVAL_FILE_data) % alignof(Params) == 0);

        params_ = reinterpret_cast<const Params *>(INTERNAL_EVAL_FILE_data);
    }

    void load(std::string_view path) noexcept {
        std::ifstream file = std::ifstream(path.data(), std::ios::binary);

        assert(file.is_open());

#if !defined(NDEBUG)
        file.seekg(0, std::ios::end);
        const std::streamsize fileSize = file.tellg();
        assert(64 * ((sizeof(Params) + 63) / 64) == fileSize);
        file.seekg(0, std::ios::beg);
#endif

        file.read(reinterpret_cast<char *>(&loadedParams_), sizeof(Params));
        params_ = &loadedParams_;
    }

    constexpr void set(const Position &position) noexcept {
        ply_ = 0;
        for (Color color : {Color::WHITE, Color::BLACK}) {
            const bool mirr = mirror(position.kingSquare(color));
            const USize kBucket = kingBucket(position.kingSquare(color), color);
            accumulators_[0].mark(color, Accumulator::REFRESH);
            accumulators_[0].refresh(params_->featureWeights[kBucket], params_->featureBiases, position, color, mirr);
        }
    }

    constexpr void update(const Position &position, Color color) noexcept {
        const bool mirr = mirror(position.kingSquare(color));
        const USize kBucket = kingBucket(position.kingSquare(color), color);

        if (accumulators_[ply_].state(color) == Accumulator::REFRESH) {
            accumulators_[ply_].refresh(params_->featureWeights[kBucket], params_->featureBiases, position, color, mirr);
        } else {
            USize idx_ = ply_;
            while (accumulators_[idx_].state(color) == Accumulator::DIRTY) {
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

        const LayerVector &friendlyAcc = accumulators_[ply_].data(color);
        const LayerVector &enemyAcc = accumulators_[ply_].data(~color);

        const USize bucketIndex = (position.occupied().count() - 2) / Arch::OUTPUT_BUCKET_DIV;

        Int32 score = 0;
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        const RegInt16 zero = SIMD::zeroInt16();
        const RegInt16 quantA = SIMD::setInt16(static_cast<Int16>(Arch::QUANT_A));
        RegInt32 sum = SIMD::zeroInt32();
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            const RegInt16 friendlyClampReg1 = SIMD::clampInt16(SIMD::loadInt16(&friendlyAcc[i]), zero, quantA);
            const RegInt16 friendlyClampReg2 = SIMD::clampInt16(SIMD::loadInt16(&friendlyAcc[i + SIMD::WIDTH]), zero, quantA);
            const RegInt16 friendlyClampReg3 = SIMD::clampInt16(SIMD::loadInt16(&friendlyAcc[i + SIMD::WIDTH * 2]), zero, quantA);
            const RegInt16 friendlyClampReg4 = SIMD::clampInt16(SIMD::loadInt16(&friendlyAcc[i + SIMD::WIDTH * 3]), zero, quantA);
            const RegInt16 enemyClampReg1 = SIMD::clampInt16(SIMD::loadInt16(&enemyAcc[i]), zero, quantA);
            const RegInt16 enemyClampReg2 = SIMD::clampInt16(SIMD::loadInt16(&enemyAcc[i + SIMD::WIDTH]), zero, quantA);
            const RegInt16 enemyClampReg3 = SIMD::clampInt16(SIMD::loadInt16(&enemyAcc[i + SIMD::WIDTH * 2]), zero, quantA);
            const RegInt16 enemyClampReg4 = SIMD::clampInt16(SIMD::loadInt16(&enemyAcc[i + SIMD::WIDTH * 3]), zero, quantA);
            const RegInt16 friendlyWeightReg1 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][0][i]);
            const RegInt16 friendlyWeightReg2 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][0][i + SIMD::WIDTH]);
            const RegInt16 friendlyWeightReg3 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][0][i + SIMD::WIDTH * 2]);
            const RegInt16 friendlyWeightReg4 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][0][i + SIMD::WIDTH * 3]);
            const RegInt16 enemyWeightReg1 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][1][i]);
            const RegInt16 enemyWeightReg2 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][1][i + SIMD::WIDTH]);
            const RegInt16 enemyWeightReg3 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][1][i + SIMD::WIDTH * 2]);
            const RegInt16 enemyWeightReg4 = SIMD::loadInt16(&params_->layerWeights[bucketIndex][1][i + SIMD::WIDTH * 3]);
            const RegInt32 friendlyProdReg1 = SIMD::mulAddInt16(friendlyClampReg1, SIMD::mulLoInt16(friendlyClampReg1, friendlyWeightReg1));
            const RegInt32 friendlyProdReg2 = SIMD::mulAddInt16(friendlyClampReg2, SIMD::mulLoInt16(friendlyClampReg2, friendlyWeightReg2));
            const RegInt32 friendlyProdReg3 = SIMD::mulAddInt16(friendlyClampReg3, SIMD::mulLoInt16(friendlyClampReg3, friendlyWeightReg3));
            const RegInt32 friendlyProdReg4 = SIMD::mulAddInt16(friendlyClampReg4, SIMD::mulLoInt16(friendlyClampReg4, friendlyWeightReg4));
            const RegInt32 enemyProdReg1 = SIMD::mulAddInt16(enemyClampReg1, SIMD::mulLoInt16(enemyClampReg1, enemyWeightReg1));
            const RegInt32 enemyProdReg2 = SIMD::mulAddInt16(enemyClampReg2, SIMD::mulLoInt16(enemyClampReg2, enemyWeightReg2));
            const RegInt32 enemyProdReg3 = SIMD::mulAddInt16(enemyClampReg3, SIMD::mulLoInt16(enemyClampReg3, enemyWeightReg3));
            const RegInt32 enemyProdReg4 = SIMD::mulAddInt16(enemyClampReg4, SIMD::mulLoInt16(enemyClampReg4, enemyWeightReg4));
            sum = SIMD::addInt32(sum, friendlyProdReg1);
            sum = SIMD::addInt32(sum, friendlyProdReg2);
            sum = SIMD::addInt32(sum, friendlyProdReg3);
            sum = SIMD::addInt32(sum, friendlyProdReg4);
            sum = SIMD::addInt32(sum, enemyProdReg1);
            sum = SIMD::addInt32(sum, enemyProdReg2);
            sum = SIMD::addInt32(sum, enemyProdReg3);
            sum = SIMD::addInt32(sum, enemyProdReg4);
        }
        score += SIMD::horizAddInt32(sum);
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            const Int32 friendlyClamp1 = std::clamp(static_cast<Int32>(friendlyAcc[i]), 0, Arch::QUANT_A);
            const Int32 enemyClamp1 = std::clamp(static_cast<Int32>(enemyAcc[i]), 0, Arch::QUANT_A);
            score += friendlyClamp1 * friendlyClamp1 * static_cast<Int32>(params_->layerWeights[bucketIndex][0][i]);
            score += enemyClamp1 * enemyClamp1 * static_cast<Int32>(params_->layerWeights[bucketIndex][1][i]);
        }
#endif
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
    Params loadedParams_;

    Accumulator accumulators_[MAX_PLY + 1];
    USize ply_;

    constexpr bool mirror(Square kingSquare) const noexcept { return kingSquare.file() > File::D; }

    constexpr USize kingBucket(Square kingSquare, Color color) const noexcept {
        const bool mirr = mirror(kingSquare);
        Square relativeSquare = (mirr) ? kingSquare.mirrored() : kingSquare;
        relativeSquare = (color == Color::WHITE) ? relativeSquare : relativeSquare.flipped();
        return Arch::KING_BUCKET_LAYOUT[4 * static_cast<USize>(relativeSquare.rank()) + static_cast<USize>(relativeSquare.file())];
    }
};

}
