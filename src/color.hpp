
#pragma once

#include <cassert>
#include <cstdint>


namespace Clownfish {

class Color {
public:
    enum class Underlying : std::uint8_t {
        WHITE,
        BLACK,
        NONE
    };

    constexpr Color() : value(Underlying::NONE) {}
    constexpr Color(Underlying color) : value(color) {}
    constexpr Color(int color) : value(static_cast<Underlying>(color)) {}

    constexpr Color operator!() const {
        assert(value != Underlying::NONE);
        return Color(static_cast<Underlying>(static_cast<int>(value) ^ 1));
    }

    constexpr bool operator==(const Color &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const Color &other) const {
        return value != other.value;
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying WHITE = Underlying::WHITE;
    static constexpr Underlying BLACK = Underlying::BLACK;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    Underlying value;
};

}
