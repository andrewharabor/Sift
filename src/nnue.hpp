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
    constexpr Accumulator() : data_{}, addSize_(0), subSize_(0), refresh_(false) {}

    constexpr VecInt16 &data() noexcept { return data_; }
    constexpr const VecInt16 &data() const noexcept { return data_; }

    constexpr void update(const Accumulator &previous, const std::array<VecInt16, NNUEArch::INPUT_SIZE> &weights, Color color) {
        assert(dirty());
        assert(!previous.dirty());
        assert(!previous.refresh());
        assert(color != Color::NONE);

        USize addIndex1 = add_[0].index(color);
        USize addIndex2 = (addSize_ > 1) ? add_[1].index(color) : 0;
        USize subIndex1 = sub_[0].index(color);
        USize subIndex2 = (subSize_ > 1) ? sub_[1].index(color) : 0;

        if (addSize_ == 1 && subSize_ == 1) {
            SIMD::addSub(data_, weights[addIndex1], weights[subIndex1]);
        } else if (addSize_ == 1 && subSize_ == 2) {
            SIMD::addSub2(data_, weights[addIndex1], weights[subIndex1], weights[subIndex2]);
        } else if (addSize_ == 2 && subSize_ == 2) {
            SIMD::add2Sub2(data_, weights[addIndex1], weights[addIndex2], weights[subIndex1], weights[subIndex2]);
        } else {
            assert(addSize_ == 0 && subSize_ == 0);
        }

        resetFeatures();
    }

    constexpr void addFeature(const InputFeature &feature) noexcept {
        assert(addSize_ < 2);
        add_[addSize_++] = feature;
    }

    constexpr void subFeature(const InputFeature &feature) noexcept {
        assert(subSize_ < 2);
        sub_[subSize_++] = feature;
    }

    constexpr void resetFeatures() noexcept {
        addSize_ = 0;
        subSize_ = 0;
    }

    constexpr bool dirty() const noexcept {
        assert((addSize_ > 0) == (subSize_ > 0));
        return addSize_ > 0 && subSize_ > 0;
    }

    constexpr bool refresh() const noexcept { return refresh_; }
    constexpr void setRefresh(bool refresh) noexcept { refresh_ = refresh; }

private:
    VecInt16 data_;
    InputFeature add_[2];
    InputFeature sub_[2];
    USize addSize_;
    USize subSize_;
    bool refresh_;
};

}
