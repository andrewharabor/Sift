#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>

#include "color.hpp"


namespace Clownfish {

class PieceType {
public:
    enum class PieceTypeEnum : std::uint8_t {
        PAWN,
        KNIGHT,
        BISHOP,
        ROOK,
        QUEEN,
        KING,
        NONE
    };

    static constexpr PieceTypeEnum PAWN = PieceTypeEnum::PAWN;
    static constexpr PieceTypeEnum KNIGHT = PieceTypeEnum::KNIGHT;
    static constexpr PieceTypeEnum BISHOP = PieceTypeEnum::BISHOP;
    static constexpr PieceTypeEnum ROOK = PieceTypeEnum::ROOK;
    static constexpr PieceTypeEnum QUEEN = PieceTypeEnum::QUEEN;
    static constexpr PieceTypeEnum KING = PieceTypeEnum::KING;
    static constexpr PieceTypeEnum NONE = PieceTypeEnum::NONE;

    constexpr PieceType() noexcept : pieceType_(PieceTypeEnum::NONE) {}
    constexpr PieceType(PieceTypeEnum pieceType) noexcept : pieceType_(pieceType) {}
    constexpr PieceType(int pieceType) noexcept : pieceType_(static_cast<PieceTypeEnum>(pieceType)) { assert(isValid(pieceType)); }

    constexpr PieceType(std::string_view piece) noexcept : pieceType_(PieceTypeEnum::NONE) {
        assert(piece.size() == 1);
        char c = piece[0];
        if (c == 'P' || c == 'p') {
            pieceType_ = PieceTypeEnum::PAWN;
        } else if (c == 'N' || c == 'n') {
            pieceType_ = PieceTypeEnum::KNIGHT;
        } else if (c == 'B' || c == 'b') {
            pieceType_ = PieceTypeEnum::BISHOP;
        } else if (c == 'R' || c == 'r') {
            pieceType_ = PieceTypeEnum::ROOK;
        } else if (c == 'Q' || c == 'q') {
            pieceType_ = PieceTypeEnum::QUEEN;
        } else if (c == 'K' || c == 'k') {
            pieceType_ = PieceTypeEnum::KING;
        } else {
            pieceType_ = PieceTypeEnum::NONE;
        }
    }

    constexpr bool operator==(const PieceType &other) const noexcept { return pieceType_ == other.pieceType_; }
    constexpr bool operator!=(const PieceType &other) const noexcept { return pieceType_ != other.pieceType_; }
    constexpr bool operator<(const PieceType &other) const noexcept { return static_cast<int>(pieceType_) < static_cast<int>(other.pieceType_); }
    constexpr bool operator>(const PieceType &other) const noexcept { return static_cast<int>(pieceType_) > static_cast<int>(other.pieceType_); }
    constexpr bool operator<=(const PieceType &other) const noexcept { return static_cast<int>(pieceType_) <= static_cast<int>(other.pieceType_); }
    constexpr bool operator>=(const PieceType &other) const noexcept { return static_cast<int>(pieceType_) >= static_cast<int>(other.pieceType_); }

    constexpr PieceTypeEnum internal() const noexcept { return pieceType_; }

private:
    constexpr bool isValid(int pieceType) const noexcept { return pieceType >= 0 && pieceType < 7; }

    PieceTypeEnum pieceType_;
};

class Piece {
public:
    enum class PieceEnum : std::uint8_t {
        WHITE_PAWN,
        WHITE_KNIGHT,
        WHITE_BISHOP,
        WHITE_ROOK,
        WHITE_QUEEN,
        WHITE_KING,
        BLACK_PAWN,
        BLACK_KNIGHT,
        BLACK_BISHOP,
        BLACK_ROOK,
        BLACK_QUEEN,
        BLACK_KING,
        NONE
    };

    static constexpr PieceEnum WHITE_PAWN = PieceEnum::WHITE_PAWN;
    static constexpr PieceEnum WHITE_KNIGHT = PieceEnum::WHITE_KNIGHT;
    static constexpr PieceEnum WHITE_BISHOP = PieceEnum::WHITE_BISHOP;
    static constexpr PieceEnum WHITE_ROOK = PieceEnum::WHITE_ROOK;
    static constexpr PieceEnum WHITE_QUEEN = PieceEnum::WHITE_QUEEN;
    static constexpr PieceEnum WHITE_KING = PieceEnum::WHITE_KING;
    static constexpr PieceEnum BLACK_PAWN = PieceEnum::BLACK_PAWN;
    static constexpr PieceEnum BLACK_KNIGHT = PieceEnum::BLACK_KNIGHT;
    static constexpr PieceEnum BLACK_BISHOP = PieceEnum::BLACK_BISHOP;
    static constexpr PieceEnum BLACK_ROOK = PieceEnum::BLACK_ROOK;
    static constexpr PieceEnum BLACK_QUEEN = PieceEnum::BLACK_QUEEN;
    static constexpr PieceEnum BLACK_KING = PieceEnum::BLACK_KING;
    static constexpr PieceEnum NONE = PieceEnum::NONE;

    constexpr Piece() noexcept : piece_(PieceEnum::NONE) {}
    constexpr Piece(PieceEnum piece) noexcept : piece_(piece) {}
    constexpr Piece(int piece) noexcept : piece_(static_cast<PieceEnum>(piece)) { assert(isValid(piece)); }

    constexpr Piece(std::string_view piece) noexcept : piece_(PieceEnum::NONE) {
        assert(piece.size() == 1);
        char c = piece[0];
        if (c == 'P') {
            piece_ = PieceEnum::WHITE_PAWN;
        } else if (c == 'N') {
            piece_ = PieceEnum::WHITE_KNIGHT;
        } else if (c == 'B') {
            piece_ = PieceEnum::WHITE_BISHOP;
        } else if (c == 'R') {
            piece_ = PieceEnum::WHITE_ROOK;
        } else if (c == 'Q') {
            piece_ = PieceEnum::WHITE_QUEEN;
        } else if (c == 'K') {
            piece_ = PieceEnum::WHITE_KING;
        } else if (c == 'p') {
            piece_ = PieceEnum::BLACK_PAWN;
        } else if (c == 'n') {
            piece_ = PieceEnum::BLACK_KNIGHT;
        } else if (c == 'b') {
            piece_ = PieceEnum::BLACK_BISHOP;
        } else if (c == 'r') {
            piece_ = PieceEnum::BLACK_ROOK;
        } else if (c == 'q') {
            piece_ = PieceEnum::BLACK_QUEEN;
        } else if (c == 'k') {
            piece_ = PieceEnum::BLACK_KING;
        } else {
            piece_ = PieceEnum::NONE;
        }
    }

    constexpr Piece(PieceType pieceType, Color color) noexcept : piece_(PieceEnum::NONE) {
        if (pieceType == PieceType::NONE || color == Color::NONE) {
            piece_ = PieceEnum::NONE;
            return;
        }
        piece_ = static_cast<PieceEnum>(static_cast<int>(color.internal()) * 6 + static_cast<int>(pieceType.internal()));
    }

    constexpr bool operator==(const Piece &other) const noexcept { return piece_ == other.piece_; }
    constexpr bool operator!=(const Piece &other) const noexcept { return piece_ != other.piece_; }
    constexpr bool operator<(const Piece &other) const noexcept { return static_cast<int>(piece_) < static_cast<int>(other.piece_); }
    constexpr bool operator>(const Piece &other) const noexcept { return static_cast<int>(piece_) > static_cast<int>(other.piece_); }
    constexpr bool operator<=(const Piece &other) const noexcept { return static_cast<int>(piece_) <= static_cast<int>(other.piece_); }
    constexpr bool operator>=(const Piece &other) const noexcept { return static_cast<int>(piece_) >= static_cast<int>(other.piece_); }

    constexpr bool operator==(const PieceType &other) const noexcept { return type() == other; }
    constexpr bool operator!=(const PieceType &other) const noexcept { return type() != other; }
    constexpr bool operator<(const PieceType &other) const noexcept { return type() < other; }
    constexpr bool operator>(const PieceType &other) const noexcept { return type() > other; }
    constexpr bool operator<=(const PieceType &other) const noexcept { return type() <= other; }
    constexpr bool operator>=(const PieceType &other) const noexcept { return type() >= other; }

    constexpr bool operator==(const Color &other) const noexcept { return color() == other; }
    constexpr bool operator!=(const Color &other) const noexcept { return color() != other; }

    constexpr PieceType type() const noexcept {
        if (piece_ == PieceEnum::NONE) {
            return PieceType::NONE;
        }
        return PieceType(static_cast<int>(piece_) % 6);
    }

    constexpr Color color() const noexcept {
        if (piece_ == PieceEnum::NONE) {
            return Color::NONE;
        }
        return Color(static_cast<int>(piece_) / 6);
    }

    constexpr PieceEnum internal() const noexcept { return piece_; }

private:
    constexpr bool isValid(int piece) const noexcept { return piece >= 0 && piece < 13; }

    PieceEnum piece_;
};

}
