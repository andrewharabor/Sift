#pragma once

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>

#include "color.hpp"
#include "types.hpp"


namespace Clownfish {

#define SQUARE_DECLARE_RANK(X)                                         \
    static constexpr SquareEnum SQUARE_A##X = SquareEnum::SQUARE_A##X; \
    static constexpr SquareEnum SQUARE_B##X = SquareEnum::SQUARE_B##X; \
    static constexpr SquareEnum SQUARE_C##X = SquareEnum::SQUARE_C##X; \
    static constexpr SquareEnum SQUARE_D##X = SquareEnum::SQUARE_D##X; \
    static constexpr SquareEnum SQUARE_E##X = SquareEnum::SQUARE_E##X; \
    static constexpr SquareEnum SQUARE_F##X = SquareEnum::SQUARE_F##X; \
    static constexpr SquareEnum SQUARE_G##X = SquareEnum::SQUARE_G##X; \
    static constexpr SquareEnum SQUARE_H##X = SquareEnum::SQUARE_H##X


class File {
public:
    enum class FileEnum : U8 {
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

    static constexpr FileEnum FILE_A = FileEnum::FILE_A;
    static constexpr FileEnum FILE_B = FileEnum::FILE_B;
    static constexpr FileEnum FILE_C = FileEnum::FILE_C;
    static constexpr FileEnum FILE_D = FileEnum::FILE_D;
    static constexpr FileEnum FILE_E = FileEnum::FILE_E;
    static constexpr FileEnum FILE_F = FileEnum::FILE_F;
    static constexpr FileEnum FILE_G = FileEnum::FILE_G;
    static constexpr FileEnum FILE_H = FileEnum::FILE_H;
    static constexpr FileEnum NONE = FileEnum::NONE;

    constexpr File() noexcept : file_(FileEnum::NONE) {}
    constexpr File(FileEnum file) noexcept : file_(file) {}
    constexpr File(int file) noexcept : file_(static_cast<FileEnum>(file)) { assert(file >= 0 && file < 8); }

    constexpr File(std::string_view file) noexcept {
        assert(file.size() == 1);
        assert(file[0] >= 'a' && file[0] <= 'h');
        file_ = static_cast<FileEnum>(file[0] - 'a');
    }

    constexpr bool operator==(const File &other) const noexcept { return file_ == other.file_; }
    constexpr bool operator!=(const File &other) const noexcept { return file_ != other.file_; }
    constexpr bool operator<(const File &other) const noexcept { return file_ < other.file_; }
    constexpr bool operator>(const File &other) const noexcept { return file_ > other.file_; }
    constexpr bool operator<=(const File &other) const noexcept { return file_ <= other.file_; }
    constexpr bool operator>=(const File &other) const noexcept { return file_ >= other.file_; }
    constexpr operator int() const noexcept { return static_cast<int>(file_); }
    constexpr explicit operator std::string() const noexcept { return std::string(1, static_cast<char>(file_) + 'a'); }

    constexpr FileEnum internal() const noexcept { return file_; }

private:
    FileEnum file_;
};

class Rank {
public:
    enum class RankEnum : U8 {
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

    static constexpr RankEnum RANK_1 = RankEnum::RANK_1;
    static constexpr RankEnum RANK_2 = RankEnum::RANK_2;
    static constexpr RankEnum RANK_3 = RankEnum::RANK_3;
    static constexpr RankEnum RANK_4 = RankEnum::RANK_4;
    static constexpr RankEnum RANK_5 = RankEnum::RANK_5;
    static constexpr RankEnum RANK_6 = RankEnum::RANK_6;
    static constexpr RankEnum RANK_7 = RankEnum::RANK_7;
    static constexpr RankEnum RANK_8 = RankEnum::RANK_8;
    static constexpr RankEnum NONE = RankEnum::NONE;

    constexpr Rank() noexcept : rank_(RankEnum::NONE) {}
    constexpr Rank(RankEnum rank) noexcept : rank_(rank) {}
    constexpr Rank(int rank) noexcept : rank_(static_cast<RankEnum>(rank)) { assert(rank >= 0 && rank < 8); }

    constexpr Rank(RankEnum rank, Color color) noexcept : rank_(rank) {
        assert(color != Color::NONE);
        if (color == Color::BLACK) {
            rank_ = static_cast<RankEnum>(7 - static_cast<int>(rank_));
        }
    }

    constexpr Rank(std::string_view rank) noexcept {
        assert(rank.size() == 1);
        assert(rank[0] >= '1' && rank[0] <= '8');
        rank_ = static_cast<RankEnum>(rank[0] - '1');
    }

    constexpr bool operator==(const Rank &other) const noexcept { return rank_ == other.rank_; }
    constexpr bool operator!=(const Rank &other) const noexcept { return rank_ != other.rank_; }
    constexpr bool operator<(const Rank &other) const noexcept { return rank_ < other.rank_; }
    constexpr bool operator>(const Rank &other) const noexcept { return rank_ > other.rank_; }
    constexpr bool operator<=(const Rank &other) const noexcept { return rank_ <= other.rank_; }
    constexpr bool operator>=(const Rank &other) const noexcept { return rank_ >= other.rank_; }
    constexpr operator int() const noexcept { return static_cast<int>(rank_); }
    constexpr explicit operator std::string() const noexcept { return std::string(1, static_cast<char>(rank_) + '1'); }

    constexpr bool backRank(Color color) const noexcept {
        assert(color != Color::NONE);
        return static_cast<int>(rank_) == (color * 7);
    };

    constexpr RankEnum internal() const noexcept { return rank_; }

private:
    RankEnum rank_;
};

class Direction {
public:
    enum class DirectionEnum : std::int8_t {
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

    static constexpr DirectionEnum NORTH = DirectionEnum::NORTH;
    static constexpr DirectionEnum EAST = DirectionEnum::EAST;
    static constexpr DirectionEnum SOUTH = DirectionEnum::SOUTH;
    static constexpr DirectionEnum WEST = DirectionEnum::WEST;
    static constexpr DirectionEnum NORTH_EAST = DirectionEnum::NORTH_EAST;
    static constexpr DirectionEnum SOUTH_EAST = DirectionEnum::SOUTH_EAST;
    static constexpr DirectionEnum SOUTH_WEST = DirectionEnum::SOUTH_WEST;
    static constexpr DirectionEnum NORTH_WEST = DirectionEnum::NORTH_WEST;
    static constexpr DirectionEnum NONE = DirectionEnum::NONE;

    constexpr Direction() noexcept : direction_(DirectionEnum::NONE) {}
    constexpr Direction(DirectionEnum direction) noexcept : direction_(direction) {}

    constexpr Direction(int direction) noexcept : direction_(static_cast<DirectionEnum>(direction)) {
        assert(direction == 8 || direction == 1 || direction == -8 || direction == -1 ||
            direction == 9 || direction == -7 || direction == -9 || direction == 7 || direction == 0);
    }

    constexpr Direction(DirectionEnum direction, Color color) noexcept : direction_(direction) {
        assert(color != Color::NONE);
        if (color == Color::BLACK) {
            direction_ = static_cast<DirectionEnum>(-static_cast<int>(direction_));
        }
    }

    constexpr bool operator==(const Direction &other) const noexcept { return direction_ == other.direction_; }
    constexpr bool operator!=(const Direction &other) const noexcept { return direction_ != other.direction_; }

    constexpr Direction operator-() const noexcept {
        assert(direction_ != DirectionEnum::NONE);
        return Direction(static_cast<DirectionEnum>(-static_cast<int>(direction_)));
    }

    constexpr operator int() const noexcept { return static_cast<int>(direction_); }

    constexpr DirectionEnum internal() const noexcept { return direction_; }

private:
    DirectionEnum direction_;
};

class Square {
public:
    enum class SquareEnum : U8 {
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

    SQUARE_DECLARE_RANK(1);
    SQUARE_DECLARE_RANK(2);
    SQUARE_DECLARE_RANK(3);
    SQUARE_DECLARE_RANK(4);
    SQUARE_DECLARE_RANK(5);
    SQUARE_DECLARE_RANK(6);
    SQUARE_DECLARE_RANK(7);
    SQUARE_DECLARE_RANK(8);
    static constexpr SquareEnum NONE = SquareEnum::NONE;

    constexpr Square() noexcept : square_(SquareEnum::NONE) {}
    constexpr Square(SquareEnum square) noexcept : square_(square) {}
    constexpr Square(int index) noexcept : square_(static_cast<SquareEnum>(index)) { assert(index >= 0 && index < 64); }

    constexpr Square(File file, Rank rank) noexcept : square_(SquareEnum::NONE) {
        assert(file != File::NONE && rank != Rank::NONE);
        square_ = static_cast<SquareEnum>(rank * 8 + file);
    }

    constexpr Square(SquareEnum square, Color color) noexcept : square_(square) {
        assert(color != Color::NONE);
        if (color == Color::BLACK) {
            square_ = static_cast<SquareEnum>(static_cast<int>(square) ^ 56);
        }
    }

    constexpr Square(std::string_view square) noexcept : square_(SquareEnum::NONE) {
        assert(square.size() == 2);
        const char fileChar = square[0];
        const char rankChar = square[1];
        assert(fileChar >= 'a' && fileChar <= 'h');
        assert(rankChar >= '1' && rankChar <= '8');
        square_ = static_cast<SquareEnum>((rankChar - '1') * 8 + (fileChar - 'a'));
    }

    constexpr bool operator==(const Square &other) const noexcept { return square_ == other.square_; }
    constexpr bool operator!=(const Square &other) const noexcept { return square_ != other.square_; }
    constexpr bool operator<(const Square &other) const noexcept { return square_ < other.square_; }
    constexpr bool operator>(const Square &other) const noexcept { return square_ > other.square_; }
    constexpr bool operator<=(const Square &other) const noexcept { return square_ <= other.square_; }
    constexpr bool operator>=(const Square &other) const noexcept { return square_ >= other.square_; }
    constexpr operator int() const noexcept { return static_cast<int>(square_); }

    constexpr explicit operator std::string() const noexcept {
        if (square_ == SquareEnum::NONE) {
            return "";
        }
        const char fileChar = static_cast<char>(file()) + 'a';
        const char rankChar = static_cast<char>(rank()) + '1';
        return std::string{fileChar, rankChar};
    }

    constexpr Square operator^(const Square &other) const noexcept { return Square(static_cast<SquareEnum>(index() ^ other.index())); }

    constexpr Square operator+(Direction direction) const noexcept {
        assert(square_ != SquareEnum::NONE);
        assert(direction != Direction::NONE);
        const int newIndex = index() + direction;
        assert(newIndex >= 0 && newIndex < 64);
        return Square(static_cast<SquareEnum>(newIndex));
    }

    constexpr Square operator-(Direction direction) const noexcept {
        return *this + (-direction);
    }

    constexpr Square &mirror() noexcept {
        assert(square_ != SquareEnum::NONE);
        square_ = static_cast<SquareEnum>(index() ^ 56);
        return *this;
    }

    constexpr File file() const noexcept {
        assert(square_ != SquareEnum::NONE);
        return File(index() % 8);
    }

    constexpr Rank rank() const noexcept {
        assert(square_ != SquareEnum::NONE);
        return Rank(index() / 8);
    }

    constexpr bool backRank(Color color) const noexcept {
        assert(color != Color::NONE);
        if (color == Color::WHITE) {
            return rank() == Rank::RANK_1;
        }
        return rank() == Rank::RANK_8;
    }

    static constexpr bool sameColor(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return ((9 * (square1 ^ square2).index()) & 8) == 0;
    }

    static constexpr int indexDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return std::abs(square1.index() - square2.index());
    }

    static constexpr I32 rankDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        I32 distance = static_cast<I32>(square1.rank()) - static_cast<I32>(square2.rank());
        if (distance < 0) {
            distance = -distance;
        }
        return distance;
    }

    static constexpr I32 fileDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        I32 distance = static_cast<I32>(square1.file()) - static_cast<I32>(square2.file());
        if (distance < 0) {
            distance = -distance;
        }
        return distance;
    }

    static constexpr I32 chebyshevDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return std::max(rankDistance(square1, square2), fileDistance(square1, square2));
    }

    static constexpr I32 manhattanDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return rankDistance(square1, square2) + fileDistance(square1, square2);
    }

    constexpr Square enPassantSquare() const noexcept {
        assert(rank() == Rank::RANK_3 || rank() == Rank::RANK_4 || rank() == Rank::RANK_5 || rank() == Rank::RANK_6);
        return Square(index() ^ 8);
    }

    constexpr int index() const noexcept { return static_cast<int>(square_); }

    constexpr SquareEnum internal() const noexcept { return square_; }

private:
    SquareEnum square_;
};

}
