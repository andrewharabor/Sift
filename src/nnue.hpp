#pragma once

#include <algorithm>
#include <cassert>

#include "arch.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "simd.hpp"
#include "types.hpp"


namespace Syft {

struct InputFeature {
    Piece piece;
    Square square;

    constexpr InputFeature() noexcept : piece(), square() {}

    constexpr InputFeature(Piece piece, Square square) noexcept : piece(piece), square(square) {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
    }

    constexpr USize index(Color color) const noexcept {
        assert(color != Color::NONE);

        const USize colorIndex = static_cast<USize>(color);
        const USize pieceTypeIndex = static_cast<USize>(piece.type());
        const USize pieceColorIndex = static_cast<USize>(piece.color());
        const USize squareIndex = (color == Color::BLACK) ? static_cast<USize>(square.flipped().index()) : static_cast<USize>(square.index());

        return squareIndex + (pieceTypeIndex + ((pieceColorIndex ^ colorIndex) * 6)) * 64;
    }
};

class Accumulator {
public:
    enum class FeatureState : UInt8 {
        CLEAN,
        DIRTY,
        REFRESH
    };

    static constexpr FeatureState CLEAN = FeatureState::CLEAN;
    static constexpr FeatureState DIRTY = FeatureState::DIRTY;
    static constexpr FeatureState REFRESH = FeatureState::REFRESH;

    constexpr Accumulator() noexcept : data_(), states_(), add_(), sub_(), addSize_(0), subSize_(0) {}

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

    constexpr FeatureState state(Color color) const noexcept {
        assert(color != Color::NONE);
        return states_[static_cast<USize>(color)];
    }

    constexpr void mark(Color color, FeatureState newState) noexcept {
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

    constexpr void update(const InputMatrix &weights, const Accumulator &previous, Color color) noexcept {
        assert(state(color) == DIRTY);
        assert(previous.state(color) == CLEAN);
        assert(addSize_ >= 1);
        assert(subSize_ >= 1);
        assert(color != Color::NONE);

        const USize add1 = add_[0].index(color);
        const USize add2 = (addSize_ > 1) ? add_[1].index(color) : 0;
        const USize sub1 = sub_[0].index(color);
        const USize sub2 = (subSize_ > 1) ? sub_[1].index(color) : 0;

        if (addSize_ == 1 && subSize_ == 1) {
            add1Sub1(weights[add1], weights[sub1], color);
        } else if (addSize_ == 1 && subSize_ == 2) {
            add1Sub2(weights[add1], weights[sub1], weights[sub2], color);
        } else if (addSize_ == 2 && subSize_ == 2) {
            add2Sub2(weights[add1], weights[add2], weights[sub1], weights[sub2], color);
        } else {
            assert(false);
        }

        mark(color, CLEAN);
    }

    constexpr void refresh(const InputMatrix &weights, const LayerVector &biases, const Position &position, Color color) noexcept {
        assert(state(color) == REFRESH);
        assert(color != Color::NONE);

        data(color) = biases;
        Bitboard occupied = position.occupied();
        while (occupied) {
            const Square square = static_cast<Square>(occupied.pop());
            const Piece piece = position.pieceAt(square);
            const USize featureIndex = InputFeature(piece, square).index(color);
            add1(weights[featureIndex], color);
        }

        mark(color, CLEAN);
    }

private:
    alignas(64) DualLayerVector data_;
    std::array<FeatureState, 2> states_;
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

    NNUE() noexcept : inputWeights_(), inputBiases_(), layerWeights_(), layerBias_(), accumulators_(), ply_(0), lastClean_() {}

    constexpr void reset(const Position &position) noexcept {
        ply_ = 0;
        lastClean_ = {0, 0};
        accumulators_[ply_].refresh(inputWeights_, inputBiases_, position, Color::WHITE);
        accumulators_[ply_].refresh(inputWeights_, inputBiases_, position, Color::BLACK);
    }

    constexpr void update(Color color) noexcept {
        USize &cleanIdx = lastClean_[static_cast<USize>(color)];
        while (cleanIdx < ply_) {
            cleanIdx++;
            accumulators_[cleanIdx].update(inputWeights_, accumulators_[cleanIdx - 1], color);
        }
    }

    constexpr Int32 evaluate(const Position &position) const noexcept {
        const LayerVector &friendlyAcc = accumulators_[ply_].data(position.sideToMove());
        const LayerVector &enemyAcc = accumulators_[ply_].data(~position.sideToMove());
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
            const RegInt16 friendlyWeightReg1 = SIMD::loadInt16(&layerWeights_[0][i]);
            const RegInt16 friendlyWeightReg2 = SIMD::loadInt16(&layerWeights_[0][i + SIMD::WIDTH]);
            const RegInt16 friendlyWeightReg3 = SIMD::loadInt16(&layerWeights_[0][i + SIMD::WIDTH * 2]);
            const RegInt16 friendlyWeightReg4 = SIMD::loadInt16(&layerWeights_[0][i + SIMD::WIDTH * 3]);
            const RegInt16 enemyWeightReg1 = SIMD::loadInt16(&layerWeights_[1][i]);
            const RegInt16 enemyWeightReg2 = SIMD::loadInt16(&layerWeights_[1][i + SIMD::WIDTH]);
            const RegInt16 enemyWeightReg3 = SIMD::loadInt16(&layerWeights_[1][i + SIMD::WIDTH * 2]);
            const RegInt16 enemyWeightReg4 = SIMD::loadInt16(&layerWeights_[1][i + SIMD::WIDTH * 3]);
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
            score += friendlyClamp1 * friendlyClamp1 * static_cast<Int32>(layerWeights_[0][i]);
            score += enemyClamp1 * enemyClamp1 * static_cast<Int32>(layerWeights_[1][i]);
        }
#endif
        score /= Arch::QUANT_A;
        score += static_cast<Int32>(layerBias_);
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
        const Piece capturedPiece = position.pieceAt(move.to());
        const PieceType pieceType = position.pieceAt(move.from()).type();

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

        if (accumulators_[ply_ - 1].state(color) == Accumulator::REFRESH) {
            acc.mark(color, Accumulator::REFRESH);
        }
    }

    constexpr void unmakeMove() noexcept {
        assert(ply_ > 0);
        ply_--;
    }

private:
    alignas(64) InputMatrix inputWeights_;
    alignas(64) LayerVector inputBiases_;
    alignas(64) DualLayerVector layerWeights_;
    alignas(64) Int16 layerBias_;

    Accumulator accumulators_[MAX_PLY + 1];
    USize ply_;
    std::array<USize, 2> lastClean_;

private:

};

}
