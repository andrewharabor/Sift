#pragma once

#include <cassert>

#include "types.hpp"


namespace Clownfish {

class Color {
public:
    enum class ColorEnum : UInt8 {
        WHITE,
        BLACK,
        NONE
    };

    static constexpr ColorEnum WHITE = ColorEnum::WHITE;
    static constexpr ColorEnum BLACK = ColorEnum::BLACK;
    static constexpr ColorEnum NONE = ColorEnum::NONE;

    constexpr Color() noexcept : color_(ColorEnum::NONE) {}
    constexpr Color(ColorEnum color) noexcept : color_(color) {}
    constexpr Color(UInt8 color) noexcept : color_(static_cast<ColorEnum>(color)) { assert(valid(color)); }

    constexpr Color operator~() const noexcept {
        assert(color_ != ColorEnum::NONE);
        return Color(1 - static_cast<UInt8>(color_));
    }

    constexpr bool operator==(const Color &other) const noexcept { return color_ == other.color_; }
    constexpr bool operator!=(const Color &other) const noexcept { return color_ != other.color_; }
    constexpr operator UInt8() const noexcept { return static_cast<UInt8>(color_); }

    constexpr ColorEnum internal() const noexcept { return color_; }

private:
    static constexpr bool valid(UInt8 color) noexcept { return color < 3; }

    ColorEnum color_;
};

}
