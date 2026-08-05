#pragma once

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>

#include "color.hpp"
#include "types.hpp"


namespace Sift {

#define SQUARE_DECLARE_RANK(X)                                         \
    static constexpr SquareEnum A##X = SquareEnum::A##X; \
    static constexpr SquareEnum B##X = SquareEnum::B##X; \
    static constexpr SquareEnum C##X = SquareEnum::C##X; \
    static constexpr SquareEnum D##X = SquareEnum::D##X; \
    static constexpr SquareEnum E##X = SquareEnum::E##X; \
    static constexpr SquareEnum F##X = SquareEnum::F##X; \
    static constexpr SquareEnum G##X = SquareEnum::G##X; \
    static constexpr SquareEnum H##X = SquareEnum::H##X


class File {
public:
    enum class FileEnum : UInt8 {
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        NONE
    };

    static constexpr FileEnum A = FileEnum::A;
    static constexpr FileEnum B = FileEnum::B;
    static constexpr FileEnum C = FileEnum::C;
    static constexpr FileEnum D = FileEnum::D;
    static constexpr FileEnum E = FileEnum::E;
    static constexpr FileEnum F = FileEnum::F;
    static constexpr FileEnum G = FileEnum::G;
    static constexpr FileEnum H = FileEnum::H;
    static constexpr FileEnum NONE = FileEnum::NONE;

    constexpr File() noexcept : file_(FileEnum::NONE) {}
    constexpr File(FileEnum file) noexcept : file_(file) {}
    constexpr File(UInt8 file) noexcept : file_(static_cast<FileEnum>(file)) { assert(file < 8); }

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
    constexpr operator UInt8() const noexcept { return static_cast<UInt8>(file_); }
    constexpr explicit operator std::string() const noexcept { return std::string(1, static_cast<char>(file_) + 'a'); }

    constexpr FileEnum internal() const noexcept { return file_; }

private:
    FileEnum file_;
};

class Rank {
public:
    enum class RankEnum : UInt8 {
        FIRST,
        SECOND,
        THIRD,
        FOURTH,
        FIFTH,
        SIXTH,
        SEVENTH,
        EIGHTH,
        NONE
    };

    static constexpr RankEnum FIRST = RankEnum::FIRST;
    static constexpr RankEnum SECOND = RankEnum::SECOND;
    static constexpr RankEnum THIRD = RankEnum::THIRD;
    static constexpr RankEnum FOURTH = RankEnum::FOURTH;
    static constexpr RankEnum FIFTH = RankEnum::FIFTH;
    static constexpr RankEnum SIXTH = RankEnum::SIXTH;
    static constexpr RankEnum SEVENTH = RankEnum::SEVENTH;
    static constexpr RankEnum EIGHTH = RankEnum::EIGHTH;
    static constexpr RankEnum NONE = RankEnum::NONE;

    constexpr Rank() noexcept : rank_(RankEnum::NONE) {}
    constexpr Rank(RankEnum rank) noexcept : rank_(rank) {}
    constexpr Rank(UInt8 rank) noexcept : rank_(static_cast<RankEnum>(rank)) { assert(rank < 8); }

    constexpr Rank(RankEnum rank, Color color) noexcept : rank_(rank) {
        assert(color != Color::NONE);
        rank_ = static_cast<RankEnum>(static_cast<UInt8>(rank_) ^ (static_cast<UInt8>(color) * 7));
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
    constexpr operator UInt8() const noexcept { return static_cast<UInt8>(rank_); }
    constexpr explicit operator std::string() const noexcept { return std::string(1, static_cast<char>(rank_) + '1'); }

    constexpr bool backRank(Color color) const noexcept {
        assert(color != Color::NONE);
        return static_cast<UInt8>(rank_) == (static_cast<UInt8>(color) * 7);
    };

    constexpr RankEnum internal() const noexcept { return rank_; }

private:
    RankEnum rank_;
};

class Direction {
public:
    enum class DirectionEnum : Int8 {
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

    constexpr Direction(Int8 direction) noexcept : direction_(static_cast<DirectionEnum>(direction)) {
        assert(direction == 8 || direction == 1 || direction == -8 || direction == -1 ||
            direction == 9 || direction == -7 || direction == -9 || direction == 7 || direction == 0);
    }

    constexpr Direction(DirectionEnum direction, Color color) noexcept : direction_(direction) {
        assert(color != Color::NONE);
        if (color == Color::BLACK) {
            direction_ = static_cast<DirectionEnum>(-static_cast<Int8>(direction_));
        }
    }

    constexpr bool operator==(const Direction &other) const noexcept { return direction_ == other.direction_; }
    constexpr bool operator!=(const Direction &other) const noexcept { return direction_ != other.direction_; }

    constexpr Direction operator-() const noexcept {
        assert(direction_ != DirectionEnum::NONE);
        return Direction(static_cast<DirectionEnum>(-static_cast<Int8>(direction_)));
    }

    constexpr operator Int8() const noexcept { return static_cast<Int8>(direction_); }

    constexpr DirectionEnum internal() const noexcept { return direction_; }

private:
    DirectionEnum direction_;
};

class Square {
public:
    enum class SquareEnum : UInt8 {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
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
    constexpr Square(UInt8 index) noexcept : square_(static_cast<SquareEnum>(index)) { assert(index < 64); }

    constexpr Square(File file, Rank rank) noexcept : square_(SquareEnum::NONE) {
        assert(file != File::NONE && rank != Rank::NONE);
        square_ = static_cast<SquareEnum>(rank * 8 + file);
    }

    constexpr Square(SquareEnum square, Color color) noexcept : square_(square) {
        assert(color != Color::NONE);
        square_ = static_cast<SquareEnum>(static_cast<UInt8>(square) ^ (static_cast<UInt8>(color) * 56));
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
    constexpr operator UInt8() const noexcept { return static_cast<UInt8>(square_); }

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
        const Int32 newIndex = static_cast<Int32>(index()) + static_cast<Int32>(direction);
        assert(newIndex >= 0 && newIndex < 64);
        return Square(static_cast<SquareEnum>(newIndex));
    }

    constexpr Square operator-(Direction direction) const noexcept {
        return *this + (-direction);
    }

    constexpr Square &flip() noexcept {
        assert(square_ != SquareEnum::NONE);
        square_ = static_cast<SquareEnum>(index() ^ 56);
        return *this;
    }

    constexpr Square flipped() const noexcept {
        assert(square_ != SquareEnum::NONE);
        return Square(static_cast<SquareEnum>(index() ^ 56));
    }

    constexpr Square &mirror() noexcept {
        assert(square_ != SquareEnum::NONE);
        square_ = static_cast<SquareEnum>(index() ^ 7);
        return *this;
    }

    constexpr Square mirrored() const noexcept {
        assert(square_ != SquareEnum::NONE);
        return Square(static_cast<SquareEnum>(index() ^ 7));
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
        return (color == Color::WHITE) ? (rank() == Rank::FIRST) : (rank() == Rank::EIGHTH);
    }

    static constexpr bool sameColor(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return ((9 * (square1 ^ square2).index()) & 8) == 0;
    }

    static constexpr UInt8 indexDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return static_cast<UInt8>(std::abs(square1.index() - square2.index()));
    }

    static constexpr Int32 rankDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        Int32 distance = static_cast<Int32>(square1.rank()) - static_cast<Int32>(square2.rank());
        if (distance < 0) {
            distance = -distance;
        }
        return distance;
    }

    static constexpr Int32 fileDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        Int32 distance = static_cast<Int32>(square1.file()) - static_cast<Int32>(square2.file());
        if (distance < 0) {
            distance = -distance;
        }
        return distance;
    }

    static constexpr Int32 chebyshevDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return std::max(rankDistance(square1, square2), fileDistance(square1, square2));
    }

    static constexpr Int32 manhattanDistance(Square square1, Square square2) noexcept {
        assert(square1 != Square::NONE && square2 != Square::NONE);
        return rankDistance(square1, square2) + fileDistance(square1, square2);
    }

    constexpr Square enPassant() const noexcept {
        assert(rank() == Rank::THIRD || rank() == Rank::FOURTH || rank() == Rank::FIFTH || rank() == Rank::SIXTH);
        return Square(index() ^ 8);
    }

    constexpr UInt8 index() const noexcept { return static_cast<UInt8>(square_); }

    constexpr SquareEnum internal() const noexcept { return square_; }

private:
    SquareEnum square_;
};

}
