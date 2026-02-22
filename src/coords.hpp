
#pragma once

#include <cassert>
#include <cstdint>


namespace Clownfish {

#define CLOWNFISH_SQUARE_DECLARE_RANK(X)                               \
    static constexpr Underlying SQUARE_A##X = Underlying::SQUARE_A##X; \
    static constexpr Underlying SQUARE_B##X = Underlying::SQUARE_B##X; \
    static constexpr Underlying SQUARE_C##X = Underlying::SQUARE_C##X; \
    static constexpr Underlying SQUARE_D##X = Underlying::SQUARE_D##X; \
    static constexpr Underlying SQUARE_E##X = Underlying::SQUARE_E##X; \
    static constexpr Underlying SQUARE_F##X = Underlying::SQUARE_F##X; \
    static constexpr Underlying SQUARE_G##X = Underlying::SQUARE_G##X; \
    static constexpr Underlying SQUARE_H##X = Underlying::SQUARE_H##X


class File {
public:
    enum class Underlying : std::uint8_t {
        FILE_A,
        FILE_B,
        FILE_C,
        FILE_D,
        FILE_E,
        FILE_F,
        FILE_G,
        FILE_H,
        NONE
    };

    constexpr File() : value(Underlying::NONE) {}
    constexpr File(Underlying file) : value(file) {}
    constexpr File(int file) : value(isValid(file) ? static_cast<Underlying>(file) : Underlying::NONE) {}

    constexpr bool operator==(const File &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const File &other) const {
        return value != other.value;
    }

    constexpr bool operator<(const File &other) const {
        return static_cast<int>(value) < static_cast<int>(other.value);
    }

    constexpr bool operator>(const File &other) const {
        return static_cast<int>(value) > static_cast<int>(other.value);
    }

    constexpr bool operator<=(const File &other) const {
        return static_cast<int>(value) <= static_cast<int>(other.value);
    }

    constexpr bool operator>=(const File &other) const {
        return static_cast<int>(value) >= static_cast<int>(other.value);
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying FILE_A = Underlying::FILE_A;
    static constexpr Underlying FILE_B = Underlying::FILE_B;
    static constexpr Underlying FILE_C = Underlying::FILE_C;
    static constexpr Underlying FILE_D = Underlying::FILE_D;
    static constexpr Underlying FILE_E = Underlying::FILE_E;
    static constexpr Underlying FILE_F = Underlying::FILE_F;
    static constexpr Underlying FILE_G = Underlying::FILE_G;
    static constexpr Underlying FILE_H = Underlying::FILE_H;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    static constexpr bool isValid(int file) {
        return file >= 0 && file < 8;
    }

    Underlying value;
};

class Rank {
public:
    enum class Underlying : std::uint8_t {
        RANK_1,
        RANK_2,
        RANK_3,
        RANK_4,
        RANK_5,
        RANK_6,
        RANK_7,
        RANK_8,
        NONE
    };

    constexpr Rank() : value(Underlying::NONE) {}
    constexpr Rank(Underlying rank) : value(rank) {}
    constexpr Rank(int rank) : value(isValid(rank) ? static_cast<Underlying>(rank) : Underlying::NONE) {}

    constexpr bool operator==(const Rank &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const Rank &other) const {
        return value != other.value;
    }

    constexpr bool operator<(const Rank &other) const {
        return static_cast<int>(value) < static_cast<int>(other.value);
    }

    constexpr bool operator>(const Rank &other) const {
        return static_cast<int>(value) > static_cast<int>(other.value);
    }

    constexpr bool operator<=(const Rank &other) const {
        return static_cast<int>(value) <= static_cast<int>(other.value);
    }

    constexpr bool operator>=(const Rank &other) const {
        return static_cast<int>(value) >= static_cast<int>(other.value);
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying RANK_1 = Underlying::RANK_1;
    static constexpr Underlying RANK_2 = Underlying::RANK_2;
    static constexpr Underlying RANK_3 = Underlying::RANK_3;
    static constexpr Underlying RANK_4 = Underlying::RANK_4;
    static constexpr Underlying RANK_5 = Underlying::RANK_5;
    static constexpr Underlying RANK_6 = Underlying::RANK_6;
    static constexpr Underlying RANK_7 = Underlying::RANK_7;
    static constexpr Underlying RANK_8 = Underlying::RANK_8;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    static constexpr bool isValid(int rank) {
        return rank >= 0 && rank < 8;
    }

    Underlying value;

};

class Direction {
public:
    enum class Underlying : std::int8_t {
        NORTH = 8,
        EAST = 1,
        SOUTH = -8,
        WEST = -1,
        NORTH_EAST = 9,
        SOUTH_EAST = -7,
        SOUTH_WEST = -9,
        NORTH_WEST = 7,
        NONE = 0
    };

    constexpr Direction() : value(Underlying::NONE) {}
    constexpr Direction(Underlying direction) : value(direction) {}

    constexpr bool operator==(const Direction &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const Direction &other) const {
        return value != other.value;
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying NORTH = Underlying::NORTH;
    static constexpr Underlying EAST = Underlying::EAST;
    static constexpr Underlying SOUTH = Underlying::SOUTH;
    static constexpr Underlying WEST = Underlying::WEST;
    static constexpr Underlying NORTH_EAST = Underlying::NORTH_EAST;
    static constexpr Underlying SOUTH_EAST = Underlying::SOUTH_EAST;
    static constexpr Underlying SOUTH_WEST = Underlying::SOUTH_WEST;
    static constexpr Underlying NORTH_WEST = Underlying::NORTH_WEST;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    Underlying value;

};

class Square {
public:
    enum class Underlying {
        SQUARE_A1, SQUARE_B1, SQUARE_C1, SQUARE_D1, SQUARE_E1, SQUARE_F1, SQUARE_G1, SQUARE_H1,
        SQUARE_A2, SQUARE_B2, SQUARE_C2, SQUARE_D2, SQUARE_E2, SQUARE_F2, SQUARE_G2, SQUARE_H2,
        SQUARE_A3, SQUARE_B3, SQUARE_C3, SQUARE_D3, SQUARE_E3, SQUARE_F3, SQUARE_G3, SQUARE_H3,
        SQUARE_A4, SQUARE_B4, SQUARE_C4, SQUARE_D4, SQUARE_E4, SQUARE_F4, SQUARE_G4, SQUARE_H4,
        SQUARE_A5, SQUARE_B5, SQUARE_C5, SQUARE_D5, SQUARE_E5, SQUARE_F5, SQUARE_G5, SQUARE_H5,
        SQUARE_A6, SQUARE_B6, SQUARE_C6, SQUARE_D6, SQUARE_E6, SQUARE_F6, SQUARE_G6, SQUARE_H6,
        SQUARE_A7, SQUARE_B7, SQUARE_C7, SQUARE_D7, SQUARE_E7, SQUARE_F7, SQUARE_G7, SQUARE_H7,
        SQUARE_A8, SQUARE_B8, SQUARE_C8, SQUARE_D8, SQUARE_E8, SQUARE_F8, SQUARE_G8, SQUARE_H8,
        NONE
    };

    constexpr Square() : value(Underlying::NONE) {}
    constexpr Square(Underlying square) : value(square) {}
    constexpr Square(int square) : value((isValid(square)) ? static_cast<Underlying>(square) : Underlying::NONE) {}

    constexpr Square(File file, Rank rank) : value(Underlying::NONE) {
        if (file == File::NONE || rank == Rank::NONE) {
            value = Underlying::NONE;
            return;
        }
        value = static_cast<Underlying>(static_cast<int>(rank.underlying()) * 8 + static_cast<int>(file.underlying()));
    }

    constexpr bool operator==(const Square &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const Square &other) const {
        return value != other.value;
    }

    constexpr bool operator<(const Square &other) const {
        return static_cast<int>(value) < static_cast<int>(other.value);
    }

    constexpr bool operator>(const Square &other) const {
        return static_cast<int>(value) > static_cast<int>(other.value);
    }

    constexpr bool operator<=(const Square &other) const {
        return static_cast<int>(value) <= static_cast<int>(other.value);
    }

    constexpr bool operator>=(const Square &other) const {
        return static_cast<int>(value) >= static_cast<int>(other.value);
    }

    constexpr Square &operator++() {
        if (value == Underlying::NONE) {
            return *this;
        }
        if (index() >= 63) {
            value = Underlying::NONE;

        } else {
            value = static_cast<Underlying>(index() + 1);
        }
        return *this;
    }

    constexpr Square operator++(int) {
        if (value == Underlying::NONE) {
            return *this;
        }
        Square tmp = *this;
        if (index() >= 63) {
            value = Underlying::NONE;
        } else {
            value = static_cast<Underlying>(index() + 1);
        }
        return tmp;
    }

    constexpr Square &operator--() {
        if (value == Underlying::NONE) {
            return *this;
        }
        if (index() <= 0) {
            value = Underlying::NONE;
        } else {
            value = static_cast<Underlying>(index() - 1);
        }
        return *this;
    }

    constexpr Square operator--(int) {
        if (value == Underlying::NONE) {
            return *this;
        }
        Square tmp = *this;
        if (index() <= 0) {
            value = Underlying::NONE;
        } else {
            value = static_cast<Underlying>(index() - 1);
        }
        return tmp;
    }

    constexpr Square operator+(Direction direction) const {
        if (value == Underlying::NONE || direction == Direction::NONE) {
            return *this;
        }

        const File currentFile = file();
        const Rank currentRank = rank();
        if (currentFile == File::NONE || currentRank == Rank::NONE) {
            return Square::NONE;
        }

        if ((direction == Direction::EAST || direction == Direction::NORTH_EAST || direction == Direction::SOUTH_EAST)
            && currentFile == File::FILE_H) {
            return Square::NONE;
        }

        if ((direction == Direction::WEST || direction == Direction::NORTH_WEST || direction == Direction::SOUTH_WEST)
            && currentFile == File::FILE_A) {
            return Square::NONE;
        }

        if ((direction == Direction::NORTH || direction == Direction::NORTH_EAST || direction == Direction::NORTH_WEST)
            && currentRank == Rank::RANK_8) {
            return Square::NONE;
        }

        if ((direction == Direction::SOUTH || direction == Direction::SOUTH_EAST || direction == Direction::SOUTH_WEST)
            && currentRank == Rank::RANK_1) {
            return Square::NONE;
        }

        const int newIndex = index() + static_cast<int>(direction.underlying());
        if (!isValid(newIndex)) {
            return Square::NONE;
        }
        return Square(static_cast<Underlying>(newIndex));
    }

    constexpr File file() const {
        if (value == Underlying::NONE || !isValid(index())) {
            return File::NONE;
        }
        return File(index() % 8);
    }

    constexpr Rank rank() const {
        if (value == Underlying::NONE || !isValid(index())) {
            return Rank::NONE;
        }
        return Rank(index() / 8);
    }

    constexpr int index() const {
        return static_cast<int>(value);
    }

    constexpr Underlying underlying() const {
        return value;
    }

    CLOWNFISH_SQUARE_DECLARE_RANK(1);
    CLOWNFISH_SQUARE_DECLARE_RANK(2);
    CLOWNFISH_SQUARE_DECLARE_RANK(3);
    CLOWNFISH_SQUARE_DECLARE_RANK(4);
    CLOWNFISH_SQUARE_DECLARE_RANK(5);
    CLOWNFISH_SQUARE_DECLARE_RANK(6);
    CLOWNFISH_SQUARE_DECLARE_RANK(7);
    CLOWNFISH_SQUARE_DECLARE_RANK(8);
    static constexpr Underlying NONE = Underlying::NONE;

private:
    static constexpr bool isValid(int square) {
        return square >= 0 && square < 64;
    }

    Underlying value;
};

}
