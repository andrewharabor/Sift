#pragma once

#include <cassert>

#include "arch.hpp"
#include "coords.hpp"
#include "piece.hpp"
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

    constexpr void clear() noexcept {
        states_ = {DIRTY, DIRTY};
        addSize_ = 0;
        subSize_ = 0;
    }

    constexpr const LayerMultiArray &data() const noexcept { return data_; }
    constexpr LayerMultiArray &data() noexcept { return data_; }

    constexpr FeatureState state(Color color) const noexcept {
        assert(color != Color::NONE);
        return states_[static_cast<USize>(color)];
    }

    constexpr void mark(Color color, FeatureState newState) noexcept {
        assert(color != Color::NONE);
        states_[static_cast<USize>(color)] = newState;
    }

    constexpr void addFeature(InputFeature feature) noexcept {
        assert(addSize_ < 2);
        add_[addSize_++] = feature;
    }

    constexpr void subFeature(InputFeature feature) noexcept {
        assert(subSize_ < 2);
        sub_[subSize_++] = feature;
    }

    constexpr void update(const Accumulator &previous, const LayerMultiArray &weights, Color color) noexcept {
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
            add1sub1(weights[add1], weights[sub1], color);
        } else if (addSize_ == 1 && subSize_ == 2) {
            add1sub2(weights[add1], weights[sub1], weights[sub2], color);
        } else if (addSize_ == 2 && subSize_ == 2) {
            add2sub2(weights[add1], weights[add2], weights[sub1], weights[sub2], color);
        } else {
            assert(false);
        }

        mark(color, CLEAN);
    }

private:
    alignas(64) LayerMultiArray data_;
    std::array<FeatureState, 2> states_;
    std::array<InputFeature, 2> add_;
    std::array<InputFeature, 2> sub_;
    USize addSize_;
    USize subSize_;

    constexpr void add1sub1(const LayerArray &add1, const LayerArray &sub1, Color color) noexcept {
        LayerArray &dataVec = data_[static_cast<USize>(color)];
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVec[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 3]);
            dataReg1 = SIMD::addInt16(dataReg1, SIMD::loadInt16(&add1[i]));
            dataReg2 = SIMD::addInt16(dataReg2, SIMD::loadInt16(&add1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::addInt16(dataReg3, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::addInt16(dataReg4, SIMD::loadInt16(&add1[i + SIMD::WIDTH * 3]));
            dataReg1 = SIMD::subInt16(dataReg1, SIMD::loadInt16(&sub1[i]));
            dataReg2 = SIMD::subInt16(dataReg2, SIMD::loadInt16(&sub1[i + SIMD::WIDTH]));
            dataReg3 = SIMD::subInt16(dataReg3, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 2]));
            dataReg4 = SIMD::subInt16(dataReg4, SIMD::loadInt16(&sub1[i + SIMD::WIDTH * 3]));
            SIMD::storeInt16(&dataVec[i], dataReg1);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVec[i] += add1[i] - sub1[i];
        }
#endif
    }

    constexpr void add1sub2(const LayerArray &add1, const LayerArray &sub1, const LayerArray &sub2, Color color) noexcept {
        LayerArray &dataVec = data_[static_cast<USize>(color)];
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVec[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 3]);
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
            SIMD::storeInt16(&dataVec[i], dataReg1);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVec[i] += add1[i] - sub1[i] - sub2[i];
        }
#endif
    }

    constexpr void add2sub2(const LayerArray &add1, const LayerArray &add2, const LayerArray &sub1, const LayerArray &sub2, Color color) noexcept {
        LayerArray &dataVec = data_[static_cast<USize>(color)];
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (SIMD::WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += SIMD::WIDTH * 4) {
            RegInt16 dataReg1 = SIMD::loadInt16(&dataVec[i]);
            RegInt16 dataReg2 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH]);
            RegInt16 dataReg3 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 2]);
            RegInt16 dataReg4 = SIMD::loadInt16(&dataVec[i + SIMD::WIDTH * 3]);
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
            SIMD::storeInt16(&dataVec[i], dataReg1);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH], dataReg2);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 2], dataReg3);
            SIMD::storeInt16(&dataVec[i + SIMD::WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            dataVec[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
#endif
    }
};

}
