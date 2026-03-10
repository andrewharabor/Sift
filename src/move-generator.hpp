#pragma once

#include <array>
#include <cstdint>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coordinates.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"


namespace Clownfish {

enum PieceFlag : std::uint8_t {
    PAWN = 1 << 0,
    KNIGHT = 1 << 1,
    BISHOP = 1 << 2,
    ROOK = 1 << 3,
    QUEEN = 1 << 4,
    KING = 1 << 5,
    ALL = PAWN | KNIGHT | BISHOP | ROOK | QUEEN | KING
};

class MoveGenerator {
public:
    enum class MoveGenerationType {
        ALL,
        CAPTURES,
        QUIET,
    };

    template<Color::ColorEnum C>
    static std::pair<Bitboard, int> checkMask(const Position &position, Square kingSquare, Bitboard occupied) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);
        const Bitboard pawns = position.pieces(PieceType::PAWN, ~COLOR);
        const Bitboard knights = position.pieces(PieceType::KNIGHT, ~COLOR);
        const Bitboard bishops = position.pieces(PieceType::BISHOP, ~COLOR);
        const Bitboard rooks = position.pieces(PieceType::ROOK, ~COLOR);
        const Bitboard queens = position.pieces(PieceType::QUEEN, ~COLOR);

        Bitboard mask = Bitboard();
        int checks = 0;

        Bitboard pawnMask = Attacks::pawn(kingSquare, COLOR) & pawns;
        mask |= pawnMask;
        checks += pawnMask.count();
        if (checks > 1) {
            return {mask, 2};
        }

        Bitboard knightMask = Attacks::knight(kingSquare) & knights;
        mask |= knightMask;
        checks += knightMask.count();
        if (checks > 1) {
            return {mask, 2};
        }

        Bitboard bishopMask = Attacks::bishop(kingSquare, occupied) & (bishops | queens);
        if (bishopMask) {
            if (bishopMask.count() > 1) {
                return {mask, 2};
            }

            mask |= Attacks::between(kingSquare, bishopMask.lsb());
            checks++;
            if (checks > 1) {
                return {mask, 2};
            }
        }

        Bitboard rookMask = Attacks::rook(kingSquare, occupied) & (rooks | queens);
        if (rookMask) {
            if (rookMask.count() > 1) {
                return {mask, 2};
            }

            mask |= Attacks::between(kingSquare, rookMask.lsb());
            checks++;
            if (checks > 1) {
                return {mask, 2};
            }
        }

        if (!mask) {
            mask = Bitboard(0xFFFFFFFFFFFFFFFFULL);
        }

        return {mask, checks};
    }

    template<Color::ColorEnum C, PieceType::PieceTypeEnum PT>
    static Bitboard pinMask(const Position &position, Square kingSquare, Bitboard friendlyOccupied, Bitboard enemyOccupied) {
        static_assert(C != Color::NONE);
        static_assert(PT == PieceType::BISHOP || PT == PieceType::ROOK);

        const Bitboard sliders = (position.pieces(PT) | position.pieces(PieceType::QUEEN)) & enemyOccupied;
        Bitboard attacks = Attacks::slider<PT>(kingSquare, enemyOccupied) & sliders;

        Bitboard pinMask = Bitboard();
        while (attacks) {
            const Bitboard possiblePin = Attacks::between(kingSquare, static_cast<int>(attacks.pop()));
            if ((possiblePin & friendlyOccupied).count() == 1) {
                pinMask |= possiblePin;
            }
        }
        return pinMask;
    }

    template<Color::ColorEnum C>
    static Bitboard attacked(const Position &position, Square enemyKingSquare, Bitboard occupied) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        occupied ^= Bitboard(enemyKingSquare);
        const Bitboard pawns = position.pieces(PieceType::PAWN, COLOR);
        Bitboard knights = position.pieces(PieceType::KNIGHT, COLOR);
        Bitboard bishops = position.pieces(PieceType::BISHOP, COLOR);
        Bitboard rooks = position.pieces(PieceType::ROOK, COLOR);
        Bitboard queens = position.pieces(PieceType::QUEEN, COLOR);

        Bitboard attackedSquares = Bitboard();

        attackedSquares |= Attacks::allPawns<C>(pawns);
        while (knights) {
            attackedSquares |= Attacks::knight(static_cast<int>(knights.pop()));
        }
        while (bishops) {
            attackedSquares |= Attacks::bishop(static_cast<int>(bishops.pop()), occupied);
        }
        while (rooks) {
            attackedSquares |= Attacks::rook(static_cast<int>(rooks.pop()), occupied);
        }
        while (queens) {
            attackedSquares |= Attacks::queen(static_cast<int>(queens.pop()), occupied);
        }

        return attackedSquares;
    }

    template<MoveGenerationType MGT = MoveGenerationType::ALL>
    static void legal(const Position &position, MoveList &moveList, std::uint8_t pieces = PieceFlag::ALL) {
        moveList.clear();
        if (position.sideToMove() == Color::WHITE) {
            legal<Color::WHITE, MGT>(position, moveList, pieces);
        } else {
            legal<Color::BLACK, MGT>(position, moveList, pieces);
        }
    }

private:
    template<typename F>
    static void whileBitboard(MoveList &moveList, Bitboard bitboard, F movesFunc) {
        while (bitboard) {
            const Square from = Square(static_cast<int>(bitboard.pop()));
            Bitboard moves = movesFunc(from);
            while (moves) {
                const Square to = Square(static_cast<int>(moves.pop()));
                moveList.add(Move(from, to));
            }
        }
    }


    template<Color::ColorEnum C, MoveGenerationType MGT>
    static void pawn(const Position &position, MoveList &moveList, Square kingSquare, Bitboard occupied, Bitboard enemyOccupied, Bitboard diagonalPins, Bitboard orthogonalPins, Bitboard checkMask) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

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

            if constexpr (MGT == MoveGenerationType::ALL || MGT == MoveGenerationType::CAPTURES) {
                while (leftPromotions) {
                    const Square to = Square(static_cast<int>(leftPromotions.pop()));
                    const Square from = to + DOWN_RIGHT;
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::KNIGHT));
                }

                while (rightPromotions) {
                    const Square to = Square(static_cast<int>(rightPromotions.pop()));
                    const Square from = to + DOWN_LEFT;
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::KNIGHT));
                }
            }

            if constexpr (MGT == MoveGenerationType::ALL || MGT == MoveGenerationType::QUIET) {
                while (pushPromotion) {
                    const Square to = Square(static_cast<int>(pushPromotion.pop()));
                    const Square from = to + DOWN;
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, Move::MoveType::PROMOTION, PieceType::KNIGHT));
                }
            }
        }

        singlePush &= ~PROMOTION_RANK;
        leftAttacks &= ~PROMOTION_RANK;
        rightAttacks &= ~PROMOTION_RANK;

        if constexpr (MGT == MoveGenerationType::ALL || MGT == MoveGenerationType::CAPTURES) {
            while (leftAttacks) {
                const Square to = Square(static_cast<int>(leftAttacks.pop()));
                const Square from = to + DOWN_RIGHT;
                moveList.add(Move(from, to));
            }

            while (rightAttacks) {
                const Square to = Square(static_cast<int>(rightAttacks.pop()));
                const Square from = to + DOWN_LEFT;
                moveList.add(Move(from, to));
            }
        }

        if constexpr (MGT == MoveGenerationType::ALL || MGT == MoveGenerationType::QUIET) {
            while (singlePush) {
                const Square to = Square(static_cast<int>(singlePush.pop()));
                const Square from = to + DOWN;
                moveList.add(Move(from, to));
            }

            while (doublePush) {
                const Square to = Square(static_cast<int>(doublePush.pop()));
                const Square from = to + DOWN + DOWN;
                moveList.add(Move(from, to));
            }
        }

        if constexpr (MGT == MoveGenerationType::QUIET) {
            return;
        }

        const Square enPassantSquare = position.enPassantSquare();
        if (enPassantSquare != Square::NONE) {
            enPassant<C>(position, moveList, kingSquare, occupied, diagonalPins, checkMask, diagonalPawns, enPassantSquare);
        }
    }

    template<Color::ColorEnum C>
    static void enPassant(const Position &position, MoveList &moveList, Square kingSquare, Bitboard occupied, Bitboard diagonalPins, Bitboard checkMask, Bitboard attackingPawns, Square enPassantSquare) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

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
            const Square from = Square(static_cast<int>(enPassantAttackers.pop()));
            const Square to = enPassantSquare;

            if (bool(Bitboard(from) & diagonalPins) && !(Bitboard(to) & diagonalPins)) {
                continue;
            }

            const Bitboard enPassantPawns = Bitboard(enPassantTarget) | Bitboard(from);
            const bool possiblePin = kingMask && enemyRooksQueens;
            if (possiblePin && (Attacks::rook(kingSquare, occupied ^ enPassantPawns) & enemyRooksQueens)) {
                break;
            }

            moveList.add(Move(from, to, Move::MoveType::EN_PASSANT));
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

    static Bitboard king(Square square, Bitboard attacked) {
        return Attacks::king(square) & ~attacked;
    }

    template<Color::ColorEnum C>
    static Bitboard castling(const Position &position, Square kingSquare, Bitboard occupied, Bitboard attacked) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        if (!kingSquare.backRank(COLOR) || !position.castlingRights().get(COLOR)) {
            return Bitboard();
        }

        constexpr std::array<Position::CastlingRights::CastlingSide, 2> castlingSides = [COLOR] {
            if constexpr (COLOR == Color::WHITE) {
                return std::array<Position::CastlingRights::CastlingSide, 2>{Position::CastlingRights::WHITE_KINGSIDE, Position::CastlingRights::WHITE_QUEENSIDE};
            } else  if constexpr (COLOR == Color::BLACK) {
                return std::array<Position::CastlingRights::CastlingSide, 2>{Position::CastlingRights::BLACK_KINGSIDE, Position::CastlingRights::BLACK_QUEENSIDE};
            } else {
                static_assert(false);
                return std::array<Position::CastlingRights::CastlingSide, 2>{};
            }
        }();

        const Position::CastlingRights castlingRights = position.castlingRights();
        Bitboard moves = Bitboard();
        for (const Position::CastlingRights::CastlingSide castlingSide : castlingSides) {
            if (!castlingRights.get(castlingSide)) {
                continue;
            }

            if (occupied & position.castlingPath(castlingSide)) {
                continue;
            }

            const bool kingside = Position::CastlingRights::kingside(castlingSide);
            const Square kingTo = Square::kingCastlingSquare(COLOR, kingside);
            if (Attacks::between(kingSquare, kingTo) & attacked) {
                continue;
            }

            const Square rookFrom = Square(Position::CastlingRights::rookFile(castlingSide), Rank(Rank::RANK_1, COLOR));
            moves |= Bitboard(rookFrom);
        }

        return moves;
    }

    template<Color::ColorEnum C, MoveGenerationType MGT>
    static void legal(const Position &position, MoveList &moveList, std::uint8_t pieces = PieceFlag::ALL) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        const Square kingSquare = position.kingSquare(COLOR);
        const Bitboard occupied = position.occupied();
        const Bitboard friendlyOccupied = position.friendly(COLOR);
        const Bitboard enemyOccupied = position.enemy(COLOR);

        const auto [checkMaskSquares, checks] = checkMask<C>(position, kingSquare, occupied);
        assert(checks <= 2);

        const Bitboard diagonalPins = pinMask<C, PieceType::BISHOP>(position, kingSquare, friendlyOccupied, enemyOccupied);
        const Bitboard orthogonalPins = pinMask<C, PieceType::ROOK>(position, kingSquare, friendlyOccupied, enemyOccupied);

        Bitboard movable = Bitboard();
        if constexpr (MGT == MoveGenerationType::ALL) {
            movable = ~friendlyOccupied;
        } else if constexpr (MGT == MoveGenerationType::CAPTURES) {
            movable = enemyOccupied;
        } else if constexpr (MGT == MoveGenerationType::QUIET) {
            movable = ~occupied;
        } else {
            static_assert(false);
        }

        if (pieces & PieceFlag::KING) {
            Bitboard attackedSquares = attacked<(~COLOR).internal()>(position, kingSquare, occupied);
            auto genKing = [&](Square square) { return king(square, attackedSquares) & movable; };
            whileBitboard(moveList, Bitboard(kingSquare), genKing);

            if constexpr (MGT == MoveGenerationType::ALL || MGT == MoveGenerationType::QUIET) {
                if (checks == 0) {
                    Bitboard castlingMoves = castling<C>(position, kingSquare, occupied, attackedSquares);
                    while (castlingMoves) {
                        const Square to = Square(static_cast<int>(castlingMoves.pop()));
                        moveList.add(Move(kingSquare, to, Move::MoveType::CASTLING));
                    }
                }
            }
        }

        if (checks >= 2) {
            return;
        }

        movable &= checkMaskSquares;

        if (pieces & PieceFlag::PAWN) {
            pawn<C, MGT>(position, moveList, kingSquare, occupied, enemyOccupied, diagonalPins, orthogonalPins, checkMaskSquares);
        }

        if (pieces & PieceFlag::KNIGHT) {
            Bitboard knights = position.pieces(PieceType::KNIGHT, COLOR) & ~(diagonalPins | orthogonalPins);
            auto genKnight = [&](Square square) { return knight(square) & movable; };
            whileBitboard(moveList, knights, genKnight);
        }

        if (pieces & PieceFlag::BISHOP) {
            Bitboard bishops = position.pieces(PieceType::BISHOP, COLOR) & ~orthogonalPins;
            auto genBishop = [&](Square square) { return bishop(square, occupied, diagonalPins) & movable; };
            whileBitboard(moveList, bishops, genBishop);
        }

        if (pieces & PieceFlag::ROOK) {
            Bitboard rooks = position.pieces(PieceType::ROOK, COLOR) & ~diagonalPins;
            auto genRook = [&](Square square) { return rook(square, occupied, orthogonalPins) & movable; };
            whileBitboard(moveList, rooks, genRook);
        }

        if (pieces & PieceFlag::QUEEN) {
            Bitboard queens = position.pieces(PieceType::QUEEN, COLOR) & ~(diagonalPins & orthogonalPins);
            auto genQueen = [&](Square square) { return queen(square, occupied, diagonalPins, orthogonalPins) & movable; };
            whileBitboard(moveList, queens, genQueen);
        }
    }
};

}
