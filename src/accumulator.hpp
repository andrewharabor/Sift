#pragma once

#include <array>

#include "bitboard.hpp"
#include "color.hpp"
#include "piece.hpp"
#include "simd.hpp"


namespace Clownfish {

class Accumulator {
public:
    constexpr Accumulator() : accumulators_{} {}

    constexpr AlignedVector &data(Color color) noexcept {
        assert(color != Color::NONE);
        return accumulators_[static_cast<std::size_t>(color)];
    }

    constexpr const AlignedVector &data(Color color) const noexcept {
        assert(color != Color::NONE);
        return accumulators_[static_cast<std::size_t>(color)];
    }

private:
    std::array<AlignedVector, 2> accumulators_;
};

}
