#pragma once

#include <cassert>
#include <cstdint>


namespace Clownfish {

class Color {
public:
    enum class ColorEnum : std::uint8_t {
        WHITE,
        BLACK,
        NONE
    };

    static constexpr ColorEnum WHITE = ColorEnum::WHITE;
    static constexpr ColorEnum BLACK = ColorEnum::BLACK;
    static constexpr ColorEnum NONE = ColorEnum::NONE;

    constexpr Color() noexcept : color_(ColorEnum::NONE) {}
    constexpr Color(ColorEnum color) noexcept : color_(color) {}
    constexpr Color(int color) noexcept : color_(static_cast<ColorEnum>(color)) { assert(valid(color)); }

    constexpr Color operator~() const noexcept {
        assert(color_ != ColorEnum::NONE);
        return Color(static_cast<ColorEnum>(1 - static_cast<int>(color_)));
    }

    constexpr bool operator==(const Color &other) const noexcept { return color_ == other.color_; }
    constexpr bool operator!=(const Color &other) const noexcept { return color_ != other.color_; }
    constexpr operator int() const noexcept { return static_cast<int>(color_); }

    constexpr ColorEnum internal() const noexcept { return color_; }

private:
    static constexpr bool valid(int color) noexcept { return color >= 0 && color < 3; }

    ColorEnum color_;
};

}
