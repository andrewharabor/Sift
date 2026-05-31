#pragma once

#include <array>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Syft {

enum class PieceFlag : UInt8 {
    PAWN = 1 << 0,
    KNIGHT = 1 << 1,
    BISHOP = 1 << 2,
    ROOK = 1 << 3,
    QUEEN = 1 << 4,
    KING = 1 << 5,
    ALL = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5)
};

constexpr bool operator&(PieceFlag left, PieceFlag right) noexcept {
    return (static_cast<UInt8>(left) & static_cast<UInt8>(right)) != 0;
}

constexpr PieceFlag operator|(PieceFlag left, PieceFlag right) noexcept {
    return static_cast<PieceFlag>(static_cast<UInt8>(left) | static_cast<UInt8>(right));
}

enum class MoveGenType : UInt8 {
    ALL,
    NOISY,
    QUIET,
};

class MoveGen {
public:
    template<MoveGenType MOVE_GEN_TYPE = MoveGenType::ALL>
    static void legal(const Position &position, MoveList &moveList, PieceFlag pieces = PieceFlag::ALL) {
        if (position.sideToMove() == Color::WHITE) {
            legal<Color::WHITE, MOVE_GEN_TYPE>(position, moveList, pieces);
        } else {
            legal<Color::BLACK, MOVE_GEN_TYPE>(position, moveList, pieces);
        }
    }

private:
    template<Color::ColorEnum COLOR_ENUM, MoveGenType MOVE_GEN_TYPE>
    static void pawn(const Position &position, MoveList &moveList, Square kingSquare, Bitboard occupied, Bitboard enemyOccupied, Bitboard diagonalPins, Bitboard orthogonalPins, Bitboard checkMask) {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        constexpr Direction UP = Direction(Direction::NORTH, COLOR);
        constexpr Direction DOWN = Direction(Direction::SOUTH, COLOR);
        constexpr Direction DOWN_LEFT = Direction(Direction::SOUTH_WEST, COLOR);
        constexpr Direction DOWN_RIGHT = Direction(Direction::SOUTH_EAST, COLOR);
        constexpr Direction UP_LEFT = Direction(Direction::NORTH_WEST, COLOR);
        constexpr Direction UP_RIGHT = Direction(Direction::NORTH_EAST, COLOR);

        constexpr Bitboard PROMOTION_RANK = Rank(Rank::RANK_8, COLOR);
        constexpr Bitboard BEFORE_PROMOTION_RANK = Rank(Rank::RANK_7, COLOR);
        constexpr Bitboard FIRST_PUSH_RANK = Rank(Rank::RANK_3, COLOR);

        const Bitboard pawns = position.pieces(PieceType::PAWN, COLOR);
        const Bitboard diagonalPawns = pawns & ~orthogonalPins;
        const Bitboard unpinnedDiagonal = diagonalPawns & ~diagonalPins;
        const Bitboard pinnedDiagonal = diagonalPawns & diagonalPins;

        Bitboard leftAttacks = Attacks::shift<UP_LEFT.internal()>(unpinnedDiagonal) | (Attacks::shift<UP_LEFT.internal()>(pinnedDiagonal) & diagonalPins);
        Bitboard rightAttacks = Attacks::shift<UP_RIGHT.internal()>(unpinnedDiagonal) | (Attacks::shift<UP_RIGHT.internal()>(pinnedDiagonal) & diagonalPins);
        leftAttacks &= enemyOccupied & checkMask;
        rightAttacks &= enemyOccupied & checkMask;

        const Bitboard orthogonalPawns = pawns & ~diagonalPins;
        const Bitboard unpinnedOrthogonal = orthogonalPawns & ~orthogonalPins;
        const Bitboard pinnedOrthogonal = orthogonalPawns & orthogonalPins;

        const Bitboard unpinnedSinglePush = Attacks::shift<UP.internal()>(unpinnedOrthogonal) & ~occupied;
        const Bitboard pinnedSinglePush = Attacks::shift<UP.internal()>(pinnedOrthogonal) & ~occupied & orthogonalPins;

        Bitboard singlePush = (unpinnedSinglePush | pinnedSinglePush) & checkMask;
        Bitboard doublePush = ((Attacks::shift<UP.internal()>(unpinnedSinglePush & FIRST_PUSH_RANK) & ~occupied) | (Attacks::shift<UP.internal()>(pinnedSinglePush & FIRST_PUSH_RANK) & ~occupied & orthogonalPins)) & checkMask;

        if (pawns & BEFORE_PROMOTION_RANK) {
            Bitboard leftPromotions = leftAttacks & PROMOTION_RANK;
            Bitboard rightPromotions = rightAttacks & PROMOTION_RANK;
            Bitboard pushPromotion = singlePush & PROMOTION_RANK;

            if constexpr (MOVE_GEN_TYPE != MoveGenType::QUIET) {
                while (leftPromotions) {
                    const Square to = Square(leftPromotions.pop());
                    const Square from = to + DOWN_RIGHT;
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                }

                while (rightPromotions) {
                    const Square to = Square(rightPromotions.pop());
                    const Square from = to + DOWN_LEFT;
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                }

                while (pushPromotion) {
                    const Square to = Square(pushPromotion.pop());
                    const Square from = to + DOWN;
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                }
            }
        }

        singlePush &= ~PROMOTION_RANK;
        leftAttacks &= ~PROMOTION_RANK;
        rightAttacks &= ~PROMOTION_RANK;

        if constexpr (MOVE_GEN_TYPE != MoveGenType::QUIET) {
            while (leftAttacks) {
                const Square to = Square(leftAttacks.pop());
                const Square from = to + DOWN_RIGHT;
                moveList.add(Move(from, to));
            }

            while (rightAttacks) {
                const Square to = Square(rightAttacks.pop());
                const Square from = to + DOWN_LEFT;
                moveList.add(Move(from, to));
            }
        }

        if constexpr (MOVE_GEN_TYPE != MoveGenType::NOISY) {
            while (singlePush) {
                const Square to = Square(singlePush.pop());
                const Square from = to + DOWN;
                moveList.add(Move(from, to));
            }

            while (doublePush) {
                const Square to = Square(doublePush.pop());
                const Square from = to + DOWN + DOWN;
                moveList.add(Move(from, to));
            }
        }

        if constexpr (MOVE_GEN_TYPE != MoveGenType::QUIET) {
            const Square enPassantSquare = position.enPassantSquare();
            if (enPassantSquare != Square::NONE) {
                enPassant<COLOR_ENUM>(position, moveList, kingSquare, occupied, diagonalPins, checkMask, diagonalPawns, enPassantSquare);
            }
        }
    }

    template<Color::ColorEnum COLOR_ENUM>
    static void enPassant(const Position &position, MoveList &moveList, Square kingSquare, Bitboard occupied, Bitboard diagonalPins, Bitboard checkMask, Bitboard attackingPawns, Square enPassantSquare) {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        assert(enPassantSquare != Square::NONE);
        assert((enPassantSquare.rank() == Rank::RANK_3 && COLOR == Color::BLACK) || (enPassantSquare.rank() == Rank::RANK_6 && COLOR == Color::WHITE));

        constexpr Direction DOWN = Direction(Direction::SOUTH, COLOR);
        const Square enPassantTarget = enPassantSquare + DOWN;

        if (((Bitboard(enPassantTarget) | Bitboard(enPassantSquare)) & checkMask).empty()) {
            return;
        }

        const Bitboard kingMask = Bitboard(kingSquare) & Bitboard(enPassantTarget.rank());
        const Bitboard enemyRooksQueens = position.pieces(PieceType::ROOK, ~COLOR) | position.pieces(PieceType::QUEEN, ~COLOR);
        Bitboard enPassantAttackers = Attacks::pawn(enPassantSquare, ~COLOR) & attackingPawns;

        while (enPassantAttackers) {
            const Square from = Square(enPassantAttackers.pop());
            const Square to = enPassantSquare;

            if (bool(Bitboard(from) & diagonalPins) && !(Bitboard(to) & diagonalPins)) {
                continue;
            }

            const Bitboard enPassantPawns = Bitboard(enPassantTarget) | Bitboard(from);
            const bool possiblePin = kingMask && enemyRooksQueens;
            if (possiblePin && (Attacks::rook(kingSquare, occupied ^ enPassantPawns) & enemyRooksQueens)) {
                break;
            }

            moveList.add(Move(from, to, MoveType::EN_PASSANT));
        }
    }

    static Bitboard knight(Square square) { return Attacks::knight(square); }

    static Bitboard bishop(Square square, Bitboard occupied, Bitboard diagonalPins) {
        if (diagonalPins & Bitboard(square)) {
            return Attacks::bishop(square, occupied) & diagonalPins;
        }
        return Attacks::bishop(square, occupied);
    }

    static Bitboard rook(Square square, Bitboard occupied, Bitboard orthogonalPins) {
        if (orthogonalPins & Bitboard(square)) {
            return Attacks::rook(square, occupied) & orthogonalPins;
        }
        return Attacks::rook(square, occupied);
    }

    static Bitboard queen(Square square, Bitboard occupied, Bitboard diagonalPins, Bitboard orthogonalPins) {
        if (diagonalPins & Bitboard(square)) {
            return Attacks::bishop(square, occupied) & diagonalPins;
        }
        if (orthogonalPins & Bitboard(square)) {
            return Attacks::rook(square, occupied) & orthogonalPins;
        }
        return Attacks::queen(square, occupied);
    }

    static Bitboard king(Square square, Bitboard attackMask) {
        return Attacks::king(square) & ~attackMask;
    }

    template<Color::ColorEnum COLOR_ENUM>
    static Bitboard castling(const Position &position, Square kingSquare, Bitboard occupied, Bitboard attackMask) {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        if (!kingSquare.backRank(COLOR) || !position.castlingRights().get(COLOR)) {
            return Bitboard();
        }

        constexpr std::array<CastlingRights::Side, 2> castlingSides = [COLOR] {
            if constexpr (COLOR == Color::WHITE) {
                return std::array<CastlingRights::Side, 2>{CastlingRights::WHITE_KINGSIDE, CastlingRights::WHITE_QUEENSIDE};
            } else  if constexpr (COLOR == Color::BLACK) {
                return std::array<CastlingRights::Side, 2>{CastlingRights::BLACK_KINGSIDE, CastlingRights::BLACK_QUEENSIDE};
            } else {
                static_assert(false);
                return std::array<CastlingRights::Side, 2>{};
            }
        }();

        const CastlingRights castlingRights = position.castlingRights();
        Bitboard moves = Bitboard();
        for (const CastlingRights::Side castlingSide : castlingSides) {
            if (!castlingRights.get(castlingSide)) {
                continue;
            }

            if (occupied & position.castlingPath(castlingSide)) {
                continue;
            }

            const Square kingTo = CastlingRights::kingTo(castlingSide);
            if (Attacks::between(kingSquare, kingTo) & attackMask) {
                continue;
            }

            const Square rookFrom = CastlingRights::rookFrom(castlingSide);
            moves |= Bitboard(rookFrom);
        }

        return moves;
    }

    template<Color::ColorEnum COLOR_ENUM, MoveGenType MOVE_GEN_TYPE>
    static void legal(const Position &position, MoveList &moveList, PieceFlag pieces = PieceFlag::ALL) {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        const Square kingSquare = position.kingSquare(COLOR);

        const Bitboard occupied = position.occupied();
        const Bitboard friendlyOccupied = position.friendly(COLOR);
        const Bitboard enemyOccupied = position.enemy(COLOR);

        const UInt8 checks = position.checks();
        const Bitboard checkMask = position.checkMask();
        const Bitboard threats = position.threats();

        const Bitboard diagonalPins = position.diagonalPinMask();
        const Bitboard orthogonalPins = position.orthogonalPinMask();

        Bitboard movable = Bitboard();
        if constexpr (MOVE_GEN_TYPE == MoveGenType::ALL) {
            movable = ~friendlyOccupied;
        } else if constexpr (MOVE_GEN_TYPE == MoveGenType::NOISY) {
            movable = enemyOccupied;
        } else if constexpr (MOVE_GEN_TYPE == MoveGenType::QUIET) {
            movable = ~occupied;
        } else {
            static_assert(false);
        }

        if (pieces & PieceFlag::KING) {
            auto genKing = [&](Square square) { return king(square, threats) & movable; };
            whileBitboardAddMoves(moveList, Bitboard(kingSquare), genKing);

            if constexpr (MOVE_GEN_TYPE != MoveGenType::NOISY) {
                if (checks == 0) {
                    Bitboard castlingMoves = castling<COLOR_ENUM>(position, kingSquare, occupied, threats);
                    while (castlingMoves) {
                        const Square to = Square(castlingMoves.pop());
                        moveList.add(Move(kingSquare, to, MoveType::CASTLING));
                    }
                }
            }
        }

        if (checks >= 2) {
            return;
        }

        movable &= checkMask;

        if (pieces & PieceFlag::PAWN) {
            pawn<COLOR_ENUM, MOVE_GEN_TYPE>(position, moveList, kingSquare, occupied, enemyOccupied, diagonalPins, orthogonalPins, checkMask);
        }

        if (pieces & PieceFlag::KNIGHT) {
            Bitboard knights = position.pieces(PieceType::KNIGHT, COLOR) & ~(diagonalPins | orthogonalPins);
            auto genKnight = [&](Square square) { return knight(square) & movable; };
            whileBitboardAddMoves(moveList, knights, genKnight);
        }

        if (pieces & PieceFlag::BISHOP) {
            Bitboard bishops = position.pieces(PieceType::BISHOP, COLOR) & ~orthogonalPins;
            auto genBishop = [&](Square square) { return bishop(square, occupied, diagonalPins) & movable; };
            whileBitboardAddMoves(moveList, bishops, genBishop);
        }

        if (pieces & PieceFlag::ROOK) {
            Bitboard rooks = position.pieces(PieceType::ROOK, COLOR) & ~diagonalPins;
            auto genRook = [&](Square square) { return rook(square, occupied, orthogonalPins) & movable; };
            whileBitboardAddMoves(moveList, rooks, genRook);
        }

        if (pieces & PieceFlag::QUEEN) {
            Bitboard queens = position.pieces(PieceType::QUEEN, COLOR) & ~(diagonalPins & orthogonalPins);
            auto genQueen = [&](Square square) { return queen(square, occupied, diagonalPins, orthogonalPins) & movable; };
            whileBitboardAddMoves(moveList, queens, genQueen);
        }
    }

    template<typename Function>
    static void whileBitboardAddMoves(MoveList &moveList, Bitboard bitboard, Function movesFunc) {
        while (bitboard) {
            const Square from = Square(bitboard.pop());
            Bitboard moves = movesFunc(from);
            while (moves) {
                const Square to = Square(moves.pop());
                moveList.add(Move(from, to));
            }
        }
    }
};

}
