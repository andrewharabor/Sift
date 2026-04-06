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


namespace Clownfish {

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
    CAPTURES,
    QUIET,
    CHECKS
};

class MoveGen {
public:
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

        Bitboard knightMask = Attacks::knight(kingSquare) & knights;
        mask |= knightMask;
        checks += knightMask.count();

        Bitboard bishopMask = Attacks::bishop(kingSquare, occupied) & (bishops | queens);
        while (bishopMask) {
            mask |= Attacks::between(kingSquare, static_cast<int>(bishopMask.pop()));
            checks++;
        }

        Bitboard rookMask = Attacks::rook(kingSquare, occupied) & (rooks | queens);
        while (rookMask) {
            mask |= Attacks::between(kingSquare, static_cast<int>(rookMask.pop()));
            checks++;
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

        Bitboard sliders = Attacks::slider<PT>(kingSquare, enemyOccupied) & (position.pieces(PT) | position.pieces(PieceType::QUEEN)) & enemyOccupied;
        Bitboard mask = Bitboard();
        while (sliders) {
            const Bitboard possiblePin = Attacks::between(kingSquare, static_cast<int>(sliders.pop()));
            if ((possiblePin & friendlyOccupied).count() == 1) {
                mask |= possiblePin;
            }
        }
        return mask;
    }

    template<Color::ColorEnum C>
    static Bitboard blockMask(const Position &position, Square enemyKingSquare, Bitboard friendlyOccupied, Bitboard enemyOccupied) {
        static_assert(C != Color::NONE);

        const Bitboard bishops = Attacks::bishop(enemyKingSquare, enemyOccupied) & (position.pieces(PieceType::BISHOP) | position.pieces(PieceType::QUEEN)) & friendlyOccupied;
        const Bitboard rooks = Attacks::rook(enemyKingSquare, enemyOccupied) & (position.pieces(PieceType::ROOK) | position.pieces(PieceType::QUEEN)) & friendlyOccupied;
        Bitboard snipers = bishops | rooks;
        Bitboard mask = Bitboard();
        while (snipers) {
            const Square sniperSquare = Square(static_cast<int>(snipers.pop()));
            const Bitboard possibleBlock = Attacks::between(enemyKingSquare, sniperSquare) ^ Bitboard(sniperSquare);
            if ((possibleBlock & friendlyOccupied).count() == 1) {
                mask |= possibleBlock;
            }
        }
        return mask;
    }

    template<Color::ColorEnum C>
    static Bitboard attacked(const Position &position, Square kingSquare, Square enemyKingSquare, Bitboard occupied) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        occupied ^= Bitboard(enemyKingSquare);
        const Bitboard pawns = position.pieces(PieceType::PAWN, COLOR);
        Bitboard knights = position.pieces(PieceType::KNIGHT, COLOR);
        Bitboard queens = position.pieces(PieceType::QUEEN, COLOR);
        Bitboard bishops = position.pieces(PieceType::BISHOP, COLOR) | queens;
        Bitboard rooks = position.pieces(PieceType::ROOK, COLOR) | queens;

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
        attackedSquares |= Attacks::king(kingSquare);

        return attackedSquares;
    }

    template<MoveGenType MGT = MoveGenType::ALL>
    static void legal(const Position &position, MoveList &moveList, PieceFlag pieces = PieceFlag::ALL) {
        moveList.clear();
        if (position.sideToMove() == Color::WHITE) {
            legal<Color::WHITE, MGT>(position, moveList, pieces);
        } else {
            legal<Color::BLACK, MGT>(position, moveList, pieces);
        }
    }

private:
    struct KingSquares {
        Square friendly;
        Square enemy;
    };

    struct Occupied {
        Bitboard all;
        Bitboard friendly;
        Bitboard enemy;
    };

    struct Masks {
        Bitboard check;
        Bitboard diagonalPins;
        Bitboard orthogonalPins;
        Bitboard blockers;
        Bitboard attacked;
    };

    struct CheckSquares {
        Bitboard pawn;
        Bitboard knight;
        Bitboard bishop;
        Bitboard rook;
        Bitboard queen;
    };

    template<Color::ColorEnum C, MoveGenType MGT>
    static void pawn(const Position &position, MoveList &moveList, const KingSquares &kingSquares, const Occupied &occupied, const Masks &masks, const CheckSquares &checkSquares) {
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
        const Bitboard diagonalPawns = pawns & ~masks.orthogonalPins;
        const Bitboard unpinnedDiagonal = diagonalPawns & ~masks.diagonalPins;
        const Bitboard pinnedDiagonal = diagonalPawns & masks.diagonalPins;

        Bitboard leftAttacks = Attacks::shift<UP_LEFT.internal()>(unpinnedDiagonal) | (Attacks::shift<UP_LEFT.internal()>(pinnedDiagonal) & masks.diagonalPins);
        Bitboard rightAttacks = Attacks::shift<UP_RIGHT.internal()>(unpinnedDiagonal) | (Attacks::shift<UP_RIGHT.internal()>(pinnedDiagonal) & masks.diagonalPins);
        leftAttacks &= occupied.enemy & masks.check;
        rightAttacks &= occupied.enemy & masks.check;

        const Bitboard orthogonalPawns = pawns & ~masks.diagonalPins;
        const Bitboard unpinnedOrthogonal = orthogonalPawns & ~masks.orthogonalPins;
        const Bitboard pinnedOrthogonal = orthogonalPawns & masks.orthogonalPins;

        const Bitboard unpinnedSinglePush = Attacks::shift<UP.internal()>(unpinnedOrthogonal) & ~occupied.all;
        const Bitboard pinnedSinglePush = Attacks::shift<UP.internal()>(pinnedOrthogonal) & ~occupied.all & masks.orthogonalPins;

        Bitboard singlePush = (unpinnedSinglePush | pinnedSinglePush) & masks.check;
        Bitboard doublePush = ((Attacks::shift<UP.internal()>(unpinnedSinglePush & FIRST_PUSH_RANK) & ~occupied.all) | (Attacks::shift<UP.internal()>(pinnedSinglePush & FIRST_PUSH_RANK) & ~occupied.all & masks.orthogonalPins)) & masks.check;

        if (pawns & BEFORE_PROMOTION_RANK) {
            Bitboard leftPromotions = leftAttacks & PROMOTION_RANK;
            Bitboard rightPromotions = rightAttacks & PROMOTION_RANK;
            Bitboard pushPromotion = singlePush & PROMOTION_RANK;

            auto addPromotions = [&](Square from, Square to) {
                if constexpr (MGT != MoveGenType::CHECKS) {
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                } else if (bool(Bitboard(from) & masks.blockers) && !(Attacks::between(kingSquares.enemy, from) & Attacks::between(kingSquares.enemy, to))) {
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                } else {
                    if (Bitboard(to) & checkSquares.knight) {
                        moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::KNIGHT));
                    }
                    if ((Bitboard(to) & checkSquares.bishop) || (Attacks::between(kingSquares.enemy, to) & Bitboard(from) & checkSquares.bishop)) {
                        moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::BISHOP));
                    }
                    if ((Bitboard(to) & checkSquares.rook) || (Attacks::between(kingSquares.enemy, to) & Bitboard(from) & checkSquares.rook)) {
                        moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::ROOK));
                    }
                    if ((Bitboard(to) & checkSquares.queen) || (Attacks::between(kingSquares.enemy, to) & Bitboard(from) & checkSquares.queen)) {
                        moveList.add(Move(from, to, MoveType::PROMOTION, PieceType::QUEEN));
                    }
                }
            };

            if constexpr (MGT == MoveGenType::ALL || MGT == MoveGenType::CAPTURES || MGT == MoveGenType::CHECKS) {
                while (leftPromotions) {
                    const Square to = Square(static_cast<int>(leftPromotions.pop()));
                    const Square from = to + DOWN_RIGHT;
                    addPromotions(from, to);
                }

                while (rightPromotions) {
                    const Square to = Square(static_cast<int>(rightPromotions.pop()));
                    const Square from = to + DOWN_LEFT;
                    addPromotions(from, to);
                }
            }

            if constexpr (MGT == MoveGenType::ALL || MGT == MoveGenType::QUIET || MGT == MoveGenType::CHECKS) {
                while (pushPromotion) {
                    const Square to = Square(static_cast<int>(pushPromotion.pop()));
                    const Square from = to + DOWN;
                    addPromotions(from, to);
                }
            }
        }

        singlePush &= ~PROMOTION_RANK;
        leftAttacks &= ~PROMOTION_RANK;
        rightAttacks &= ~PROMOTION_RANK;

        auto addMove = [&](Square from, Square to) {
            if constexpr (MGT != MoveGenType::CHECKS) {
                moveList.add(Move(from, to));
            } else if ((bool(Bitboard(from) & masks.blockers) && !(Attacks::between(kingSquares.enemy, from) & Attacks::between(kingSquares.enemy, to))) || (Bitboard(to) & checkSquares.pawn)) {
                moveList.add(Move(from, to));
            }
        };

        if constexpr (MGT == MoveGenType::ALL || MGT == MoveGenType::CAPTURES || MGT == MoveGenType::CHECKS) {
            while (leftAttacks) {
                const Square to = Square(static_cast<int>(leftAttacks.pop()));
                const Square from = to + DOWN_RIGHT;
                addMove(from, to);
            }

            while (rightAttacks) {
                const Square to = Square(static_cast<int>(rightAttacks.pop()));
                const Square from = to + DOWN_LEFT;
                addMove(from, to);
            }
        }

        if constexpr (MGT == MoveGenType::ALL || MGT == MoveGenType::QUIET || MGT == MoveGenType::CHECKS) {
            while (singlePush) {
                const Square to = Square(static_cast<int>(singlePush.pop()));
                const Square from = to + DOWN;
                addMove(from, to);
            }

            while (doublePush) {
                const Square to = Square(static_cast<int>(doublePush.pop()));
                const Square from = to + DOWN + DOWN;
                addMove(from, to);
            }
        }

        const Square enPassantSquare = position.enPassantSquare();
        if (enPassantSquare != Square::NONE) {
            enPassant<C, MGT>(position, moveList, kingSquares, occupied, masks, checkSquares, diagonalPawns, enPassantSquare);
        }
    }

    template<Color::ColorEnum C, MoveGenType MGT>
    static void enPassant(const Position &position, MoveList &moveList, const KingSquares &kingSquares, const Occupied &occupied, const Masks &masks, const CheckSquares &checkSquares, Bitboard attackingPawns, Square enPassantSquare) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        assert(enPassantSquare != Square::NONE);
        assert((enPassantSquare.rank() == Rank::RANK_3 && COLOR == Color::BLACK) || (enPassantSquare.rank() == Rank::RANK_6 && COLOR == Color::WHITE));

        if constexpr (MGT == MoveGenType::QUIET) {
            return;
        }

        constexpr Direction DOWN = Direction(Direction::SOUTH, COLOR);
        const Square enPassantTarget = enPassantSquare + DOWN;

        if (((Bitboard(enPassantTarget) | Bitboard(enPassantSquare)) & masks.check).empty()) {
            return;
        }

        const Bitboard kingMask = Bitboard(kingSquares.friendly) & Bitboard(enPassantTarget.rank());
        const Bitboard enemyRooksQueens = position.pieces(PieceType::ROOK, ~COLOR) | position.pieces(PieceType::QUEEN, ~COLOR);

        const Bitboard enemyKingMask = Bitboard(kingSquares.enemy) & Bitboard(enPassantTarget.rank());
        const Bitboard friendlyRooksQueens = position.pieces(PieceType::ROOK, COLOR) | position.pieces(PieceType::QUEEN, COLOR);
        const Bitboard enemyDiagonalPinned = pinMask<(~COLOR).internal(), PieceType::BISHOP>(position, kingSquares.enemy, occupied.enemy, occupied.friendly);

        Bitboard enPassantAttackers = Attacks::pawn(enPassantSquare, ~COLOR) & attackingPawns;
        while (enPassantAttackers) {
            const Square from = Square(static_cast<int>(enPassantAttackers.pop()));
            const Square to = enPassantSquare;

            if (bool(Bitboard(from) & masks.diagonalPins) && !(Bitboard(to) & masks.diagonalPins)) {
                continue;
            }

            const Bitboard enPassantPawns = Bitboard(enPassantTarget) | Bitboard(from);
            const bool possiblePin = kingMask && enemyRooksQueens;
            if (possiblePin && (Attacks::rook(kingSquares.friendly, occupied.all ^ enPassantPawns) & enemyRooksQueens)) {
                break;
            }

            if constexpr (MGT != MoveGenType::CHECKS) {
                moveList.add(Move(from, to, MoveType::EN_PASSANT));
            } else {
                const bool possibleBlocker = enemyKingMask && friendlyRooksQueens;
                if ((bool(Bitboard(from) & masks.blockers) && !(Attacks::between(kingSquares.enemy, from) & Attacks::between(kingSquares.enemy, to))) || (Bitboard(to) & checkSquares.pawn) || (Bitboard(enPassantTarget) & enemyDiagonalPinned) || (possibleBlocker && (Attacks::rook(kingSquares.enemy, occupied.all ^ enPassantPawns) & friendlyRooksQueens))) {
                    moveList.add(Move(from, to, MoveType::EN_PASSANT));
                }
            }
        }
    }

    static Bitboard knight(Square square) { return Attacks::knight(square); }

    static Bitboard bishop(Square square, const Occupied &occupied, const Masks &masks) {
        if (masks.diagonalPins & Bitboard(square)) {
            return Attacks::bishop(square, occupied.all) & masks.diagonalPins;
        }
        return Attacks::bishop(square, occupied.all);
    }

    static Bitboard rook(Square square, const Occupied &occupied, const Masks &masks) {
        if (masks.orthogonalPins & Bitboard(square)) {
            return Attacks::rook(square, occupied.all) & masks.orthogonalPins;
        }
        return Attacks::rook(square, occupied.all);
    }

    static Bitboard queen(Square square, const Occupied &occupied, const Masks &masks) {
        if (masks.diagonalPins & Bitboard(square)) {
            return Attacks::bishop(square, occupied.all) & masks.diagonalPins;
        }
        if (masks.orthogonalPins & Bitboard(square)) {
            return Attacks::rook(square, occupied.all) & masks.orthogonalPins;
        }
        return Attacks::queen(square, occupied.all);
    }

    static Bitboard king(Square square, Bitboard attacked) {
        return Attacks::king(square) & ~attacked;
    }

    template<Color::ColorEnum C>
    static Bitboard castling(const Position &position, const KingSquares &kingSquares, const Occupied &occupied, const Masks &masks) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        if (!kingSquares.friendly.backRank(COLOR) || !position.castlingRights().get(COLOR)) {
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

            if (occupied.all & position.castlingPath(castlingSide)) {
                continue;
            }

            const Square kingTo = Position::CastlingRights::kingTo(castlingSide);
            if (Attacks::between(kingSquares.friendly, kingTo) & masks.attacked) {
                continue;
            }

            const Square rookFrom = Position::CastlingRights::rookFrom(castlingSide);
            moves |= Bitboard(rookFrom);
        }

        return moves;
    }

    template<Color::ColorEnum C, MoveGenType MGT>
    static void legal(const Position &position, MoveList &moveList, PieceFlag pieces = PieceFlag::ALL) {
        static_assert(C != Color::NONE);
        constexpr Color COLOR = Color(C);

        const KingSquares kingSquares = {
            position.kingSquare(COLOR),
            position.kingSquare(~COLOR)
        };

        const Occupied occupied = {
            position.occupied(),
            position.friendly(COLOR),
            position.enemy(COLOR)
        };

        const auto [checkMaskSquares, checks] = checkMask<C>(position, kingSquares.friendly, occupied.all);

        const Masks masks = {
            checkMaskSquares,
            pinMask<C, PieceType::BISHOP>(position, kingSquares.friendly, occupied.friendly, occupied.enemy),
            pinMask<C, PieceType::ROOK>(position, kingSquares.friendly, occupied.friendly, occupied.enemy),
            blockMask<C>(position, kingSquares.enemy, occupied.friendly, occupied.enemy),
            attacked<(~COLOR).internal()>(position, kingSquares.enemy, kingSquares.friendly, occupied.all)
        };

        const CheckSquares checkSquares = {
            Attacks::pawn(kingSquares.enemy, ~COLOR),
            Attacks::knight(kingSquares.enemy),
            Attacks::bishop(kingSquares.enemy, occupied.all),
            Attacks::rook(kingSquares.enemy, occupied.all),
            Attacks::queen(kingSquares.enemy, occupied.all),
        };

        Bitboard movable = Bitboard();
        if constexpr (MGT == MoveGenType::ALL || MGT == MoveGenType::CHECKS) {
            movable = ~occupied.friendly;
        } else if constexpr (MGT == MoveGenType::CAPTURES) {
            movable = occupied.enemy;
        } else if constexpr (MGT == MoveGenType::QUIET) {
            movable = ~occupied.all;
        } else {
            static_assert(false);
        }

        if (pieces & PieceFlag::KING) {
            auto genKing = [&](Square square) { return king(square, masks.attacked) & movable; };
            whileBitboardAddMoves<MGT>(moveList, Bitboard(kingSquares.friendly), genKing, kingSquares.enemy, masks.blockers, Bitboard());

            if constexpr (MGT != MoveGenType::CAPTURES) {
                if (checks == 0) {
                    Bitboard castlingMoves = castling<C>(position, kingSquares, occupied, masks);
                    while (castlingMoves) {
                        const Square kingTo = Square(static_cast<int>(castlingMoves.pop()));
                        const Position::CastlingRights::CastlingSide castlingSide = Position::CastlingRights::closestSide(kingTo, kingSquares.friendly, COLOR);
                        const Square rookTo = Position::CastlingRights::rookTo(castlingSide);
                        if constexpr (MGT != MoveGenType::CHECKS) {
                            moveList.add(Move(kingSquares.friendly, kingTo, MoveType::CASTLING));
                        } else if (Bitboard(rookTo) & checkSquares.rook) {
                            moveList.add(Move(kingSquares.friendly, kingTo, MoveType::CASTLING));
                        }
                    }
                }
            }
        }

        if (checks >= 2) {
            return;
        }

        movable &= masks.check;

        if (pieces & PieceFlag::PAWN) {
            pawn<C, MGT>(position, moveList, kingSquares, occupied, masks, checkSquares);
        }

        if (pieces & PieceFlag::KNIGHT) {
            Bitboard knights = position.pieces(PieceType::KNIGHT, COLOR) & ~(masks.diagonalPins | masks.orthogonalPins);
            auto genKnight = [&](Square square) { return knight(square) & movable; };
            whileBitboardAddMoves<MGT>(moveList, knights, genKnight, kingSquares.enemy, masks.blockers, checkSquares.knight);
        }

        if (pieces & PieceFlag::BISHOP) {
            Bitboard bishops = position.pieces(PieceType::BISHOP, COLOR) & ~masks.orthogonalPins;
            auto genBishop = [&](Square square) { return bishop(square, occupied, masks) & movable; };
            whileBitboardAddMoves<MGT>(moveList, bishops, genBishop, kingSquares.enemy, masks.blockers, checkSquares.bishop);
        }

        if (pieces & PieceFlag::ROOK) {
            Bitboard rooks = position.pieces(PieceType::ROOK, COLOR) & ~masks.diagonalPins;
            auto genRook = [&](Square square) { return rook(square, occupied, masks) & movable; };
            whileBitboardAddMoves<MGT>(moveList, rooks, genRook, kingSquares.enemy, masks.blockers, checkSquares.rook);
        }

        if (pieces & PieceFlag::QUEEN) {
            Bitboard queens = position.pieces(PieceType::QUEEN, COLOR) & ~(masks.diagonalPins & masks.orthogonalPins);
            auto genQueen = [&](Square square) { return queen(square, occupied, masks) & movable; };
            whileBitboardAddMoves<MGT>(moveList, queens, genQueen, kingSquares.enemy, masks.blockers, checkSquares.queen);
        }
    }

    template<MoveGenType MGT, typename F>
    static void whileBitboardAddMoves(MoveList &moveList, Bitboard bitboard, F movesFunc, Square enemyKingSquare, Bitboard blockers, Bitboard checkSquares) {
        while (bitboard) {
            const Square from = Square(static_cast<int>(bitboard.pop()));
            Bitboard moves = movesFunc(from);
            while (moves) {
                const Square to = Square(static_cast<int>(moves.pop()));
                if constexpr (MGT != MoveGenType::CHECKS) {
                    moveList.add(Move(from, to));
                } else if ((bool(Bitboard(from) & blockers) && !(Attacks::between(enemyKingSquare, from) & Attacks::between(enemyKingSquare, to))) || (Bitboard(to) & checkSquares)) {
                    moveList.add(Move(from, to));
                }
            }
        }
    }
};

}
