
#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include "color.hpp"


namespace Clownfish {

class PieceType {
public:
    enum class Underlying : std::uint8_t {
        PAWN,
        KNIGHT,
        BISHOP,
        ROOK,
        QUEEN,
        KING,
        NONE
    };

    constexpr PieceType() : value(Underlying::NONE) {}
    constexpr PieceType(Underlying pieceType) : value(pieceType) {}
    constexpr PieceType(int pieceType) : value(static_cast<Underlying>(pieceType)) {}

    constexpr PieceType(std::string piece) : value(Underlying::NONE) {
        assert(piece.size() == 1);
        char c = piece[0];
        if (c == 'P' || c == 'p') {
            value = Underlying::PAWN;
        } else if (c == 'N' || c == 'n') {
            value = Underlying::KNIGHT;
        } else if (c == 'B' || c == 'b') {
            value = Underlying::BISHOP;
        } else if (c == 'R' || c == 'r') {
            value = Underlying::ROOK;
        } else if (c == 'Q' || c == 'q') {
            value = Underlying::QUEEN;
        } else if (c == 'K' || c == 'k') {
            value = Underlying::KING;
        } else {
            value = Underlying::NONE;
        }
    }

    constexpr bool operator==(const PieceType &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const PieceType &other) const {
        return value != other.value;
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying PAWN = Underlying::PAWN;
    static constexpr Underlying KNIGHT = Underlying::KNIGHT;
    static constexpr Underlying BISHOP = Underlying::BISHOP;
    static constexpr Underlying ROOK = Underlying::ROOK;
    static constexpr Underlying QUEEN = Underlying::QUEEN;
    static constexpr Underlying KING = Underlying::KING;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    Underlying value;
};

class Piece {
public:
    enum class Underlying : std::uint8_t {
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

    constexpr Piece() : value(Underlying::NONE) {}
    constexpr Piece(Underlying piece) : value(piece) {}
    constexpr Piece(int piece) : value(static_cast<Underlying>(piece)) {}

    constexpr Piece(std::string piece) : value(Underlying::NONE) {
        assert(piece.size() == 1);
        char c = piece[0];
        if (c == 'P') {
            value = Underlying::WHITE_PAWN;
        } else if (c == 'N') {
            value = Underlying::WHITE_KNIGHT;
        } else if (c == 'B') {
            value = Underlying::WHITE_BISHOP;
        } else if (c == 'R') {
            value = Underlying::WHITE_ROOK;
        } else if (c == 'Q') {
            value = Underlying::WHITE_QUEEN;
        } else if (c == 'K') {
            value = Underlying::WHITE_KING;
        } else if (c == 'p') {
            value = Underlying::BLACK_PAWN;
        } else if (c == 'n') {
            value = Underlying::BLACK_KNIGHT;
        } else if (c == 'b') {
            value = Underlying::BLACK_BISHOP;
        } else if (c == 'r') {
            value = Underlying::BLACK_ROOK;
        } else if (c == 'q') {
            value = Underlying::BLACK_QUEEN;
        } else if (c == 'k') {
            value = Underlying::BLACK_KING;
        } else {
            value = Underlying::NONE;
        }
    }

    constexpr Piece(PieceType pieceType, Color color) : value(Underlying::NONE) {
        if (pieceType == PieceType::NONE || color == Color::NONE) {
            value = Underlying::NONE;
            return;
        }
        value = static_cast<Underlying>(static_cast<int>(color.underlying()) * 6 + static_cast<int>(pieceType.underlying()));
    }

    constexpr bool operator==(const Piece &other) const {
        return value == other.value;
    }

    constexpr bool operator!=(const Piece &other) const {
        return value != other.value;
    }

    constexpr bool operator==(const PieceType &other) const {
        return type() == other;
    }

    constexpr bool operator!=(const PieceType &other) const {
        return type() != other;
    }

    constexpr bool operator==(const Color &other) const {
        return color() == other;
    }

    constexpr bool operator!=(const Color &other) const {
        return color() != other;
    }

    constexpr PieceType type() const {
        if (value == Underlying::NONE) {
            return PieceType::NONE;
        }
        return PieceType(static_cast<int>(value) % 6);
    }

    constexpr Color color() const {
        if (value == Underlying::NONE) {
            return Color::NONE;
        }
        return Color(static_cast<int>(value) / 6);
    }

    constexpr Underlying underlying() const {
        return value;
    }

    static constexpr Underlying WHITE_PAWN = Underlying::WHITE_PAWN;
    static constexpr Underlying WHITE_KNIGHT = Underlying::WHITE_KNIGHT;
    static constexpr Underlying WHITE_BISHOP = Underlying::WHITE_BISHOP;
    static constexpr Underlying WHITE_ROOK = Underlying::WHITE_ROOK;
    static constexpr Underlying WHITE_QUEEN = Underlying::WHITE_QUEEN;
    static constexpr Underlying WHITE_KING = Underlying::WHITE_KING;
    static constexpr Underlying BLACK_PAWN = Underlying::BLACK_PAWN;
    static constexpr Underlying BLACK_KNIGHT = Underlying::BLACK_KNIGHT;
    static constexpr Underlying BLACK_BISHOP = Underlying::BLACK_BISHOP;
    static constexpr Underlying BLACK_ROOK = Underlying::BLACK_ROOK;
    static constexpr Underlying BLACK_QUEEN = Underlying::BLACK_QUEEN;
    static constexpr Underlying BLACK_KING = Underlying::BLACK_KING;
    static constexpr Underlying NONE = Underlying::NONE;

private:
    Underlying value;
};

}
