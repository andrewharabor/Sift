#pragma once

#include <cstdint>
#include <functional>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "magic.hpp"
#include "piece.hpp"


namespace Clownfish {

template<Direction::DirectionEnum D>
inline constexpr Bitboard Attacks::shift(const Bitboard bitboard) noexcept {
    static_assert(D != Direction::NONE);
    if constexpr (D == Direction::NORTH) {
        return bitboard << 8;
    } else if constexpr (D == Direction::EAST) {
        return (bitboard & ~FILE_MASKS[7]) << 1;
    } else if constexpr (D == Direction::SOUTH) {
        return bitboard >> 8;
    } else if constexpr (D == Direction::WEST) {
        return (bitboard & ~FILE_MASKS[0]) >> 1;
    } else if constexpr (D == Direction::NORTH_EAST) {
        return (bitboard & ~FILE_MASKS[7]) << 9;
    } else if constexpr (D == Direction::SOUTH_EAST) {
        return (bitboard & ~FILE_MASKS[7]) >> 7;
    } else if constexpr (D == Direction::SOUTH_WEST) {
        return (bitboard & ~FILE_MASKS[0]) >> 9;
    } else if constexpr (D == Direction::NORTH_WEST) {
        return (bitboard & ~FILE_MASKS[0]) << 7;
    } else {
        static_assert(false);
        return Bitboard();
    }
}

inline void Attacks::init() {
    BISHOP_TABLE[0].attacks = BISHOP_ATTACKS;
    ROOK_TABLE[0].attacks = ROOK_ATTACKS;
    for (int i = 0; i < 64; i++) {
        initSliders(Square(i), BISHOP_TABLE, BISHOP_MAGICS[i], sliderSlow<PieceType::BISHOP>);
        initSliders(Square(i), ROOK_TABLE, ROOK_MAGICS[i], sliderSlow<PieceType::ROOK>);
    }
}

inline void Attacks::initSliders(Square square, Magic table[], std::uint64_t magic, const std::function<Bitboard(Square, Bitboard)> &attacks) {
    assert(square != Square::NONE);
    const Bitboard edges = ((Bitboard(Rank::RANK_1) | Bitboard(Rank::RANK_8)) & ~Bitboard(square.rank())) | ((Bitboard(File::FILE_A) | Bitboard(File::FILE_H)) & ~Bitboard(square.file()));
    std::uint64_t occupied = 0ULL;
    Magic &entry = table[square.index()];
    entry.mask = (attacks(square, occupied) & ~edges).bits();
    entry.magic = magic;
    entry.shift = 64 - static_cast<std::uint64_t>(Bitboard(entry.mask).count());

    if (square.index() < 63) {
        table[square.index() + 1].attacks = entry.attacks + (1ULL << Bitboard(entry.mask).count());
    }

    do {
        entry.attacks[entry.hashIndex(occupied)] = attacks(square, occupied);
        occupied = (occupied - entry.mask) & entry.mask;
    } while (occupied);
}

template<PieceType::PieceTypeEnum PT>
inline Bitboard Attacks::sliderSlow(Square square, Bitboard occupied) {
    static_assert(PT == PieceType::BISHOP || PT == PieceType::ROOK);
    assert(square != Square::NONE);
    static constexpr int directions[2][4][2] = {
        { { 1, 1 }, { 1, -1 }, { -1, -1 }, { -1, 1 } },
        { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } }
    };
    const bool isRook = (PT == PieceType::ROOK);
    Bitboard attacks = 0ULL;

    int file = static_cast<int>(square.file().internal());
    int rank = static_cast<int>(square.rank().internal());
    for (int i = 0; i < 4; i++) {
        int fileOffset = directions[isRook][i][0];
        int rankOffset = directions[isRook][i][1];
        int currentFile;
        int currentRank;
        for (currentFile = file + fileOffset, currentRank = rank + rankOffset; currentFile >= 0 && currentFile < 8 && currentRank >= 0 && currentRank < 8; currentFile += fileOffset, currentRank += rankOffset) {
            const int index = Square(File(currentFile), Rank(currentRank)).index();
            attacks.set(index);
            if (occupied.get(index)) {
                break;
            }
        }
    }

    return attacks;
}

template<Color::ColorEnum C>
inline Bitboard Attacks::pawnLeftAttacks(const Bitboard pawns) noexcept {
    static_assert(C != Color::NONE);
    if constexpr (C == Color::WHITE) {
        return shift<Direction::NORTH_WEST>(pawns);
    } else if constexpr (C == Color::BLACK) {
        return shift<Direction::SOUTH_EAST>(pawns);
    } else {
        static_assert(false);
        return Bitboard();
    }
}

template<Color::ColorEnum C>
inline Bitboard Attacks::pawnRightAttacks(const Bitboard pawns) noexcept {
    static_assert(C != Color::NONE);
    if constexpr (C == Color::WHITE) {
        return shift<Direction::NORTH_EAST>(pawns);
    } else if constexpr (C == Color::BLACK) {
        return shift<Direction::SOUTH_WEST>(pawns);
    } else {
        static_assert(false);
        return Bitboard();
    }
}

inline Bitboard Attacks::pawn(Color color, Square square) noexcept {
    assert(color != Color::NONE);
    assert(square != Square::NONE);
    return PAWN_ATTACKS[static_cast<int>(color.internal())][square.index()];
}

inline Bitboard Attacks::knight(Square square) noexcept {
    assert(square != Square::NONE);
    return KNIGHT_ATTACKS[square.index()];
}

inline Bitboard Attacks::bishop(Square square, Bitboard occupied) noexcept {
    assert(square != Square::NONE);
    return BISHOP_TABLE[square.index()].attacks[BISHOP_TABLE[square.index()].hashIndex(occupied)];
}

inline Bitboard Attacks::rook(Square square, Bitboard occupied) noexcept {
    assert(square != Square::NONE);
    return ROOK_TABLE[square.index()].attacks[ROOK_TABLE[square.index()].hashIndex(occupied)];
}

inline Bitboard Attacks::queen(Square square, Bitboard occupied) noexcept {
    assert(square != Square::NONE);
    return bishop(square, occupied) | rook(square, occupied);
}

inline Bitboard Attacks::king(Square square) noexcept {
    assert(square != Square::NONE);
    return KING_ATTACKS[square.index()];
}

template<PieceType::PieceTypeEnum PT>
inline Bitboard Attacks::slider(Square square, Bitboard occupied) noexcept {
    static_assert(PT == PieceType::BISHOP || PT == PieceType::ROOK || PT == PieceType::QUEEN);
    assert(square != Square::NONE);
    if constexpr (PT == PieceType::BISHOP) {
        return bishop(square, occupied);
    } else if constexpr (PT == PieceType::ROOK) {
        return rook(square, occupied);
    } else if constexpr (PT == PieceType::QUEEN) {
        return queen(square, occupied);
    } else {
        static_assert(false);
        return Bitboard();
    }
}

// inline Bitboard Attacks::attackers(const Board &board, Color color, Square square) noexcept {
//     // TODO
// }

}  // namespace Clownfish
