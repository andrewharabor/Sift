
#include <iostream>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "move.hpp"
#include "piece.hpp"


using namespace std;
using namespace Clownfish;

void testColor() {
    Color defaultColor;
    Color whiteColor(Color::WHITE);
    Color blackColor(Color::BLACK);
    Color whiteFromInt(0);
    Color blackFromInt(1);
    Color noneFromInt(2);

    assert(defaultColor == Color::NONE);
    assert(whiteColor != blackColor);
    assert(!whiteColor == Color::BLACK);
    assert(!blackColor == Color::WHITE);
    assert(Color::WHITE != Color::BLACK);
    assert(defaultColor == Color::NONE);
    assert(static_cast<int>(Color::WHITE) == 0);
    assert(static_cast<int>(Color::BLACK) == 1);
    assert(static_cast<int>(Color::NONE) == 2);
    assert(whiteFromInt == Color::WHITE);
    assert(blackFromInt == Color::BLACK);
    assert(noneFromInt == Color::NONE);
}

void testPieceType() {
    PieceType defaultPieceType;
    PieceType pawnType(PieceType::PAWN);
    PieceType knightType(PieceType::KNIGHT);
    PieceType bishopType(PieceType::BISHOP);
    PieceType rookType(PieceType::ROOK);
    PieceType queenType(PieceType::QUEEN);
    PieceType kingType(PieceType::KING);
    assert(defaultPieceType == PieceType::NONE);
    assert(pawnType != knightType);
    assert(queenType != kingType);
    assert(pawnType == PieceType("P"));
    assert(knightType == PieceType("N"));
    assert(bishopType == PieceType("B"));
    assert(rookType == PieceType("R"));
    assert(queenType == PieceType("Q"));
    assert(kingType == PieceType("K"));
    assert(PieceType("x") == PieceType::NONE);
    assert(PieceType(0) == PieceType::PAWN);
    assert(PieceType(5) == PieceType::KING);
}

void testPiece() {
    Piece defaultPiece;
    Piece whitePawn(PieceType::PAWN, Color::WHITE);
    Piece blackKnight(PieceType::KNIGHT, Color::BLACK);
    Piece noneFromNone(PieceType::NONE, Color::NONE);
    Piece noneFromNoType(PieceType::NONE, Color::WHITE);
    Piece noneFromNoColor(PieceType::PAWN, Color::NONE);

    assert(defaultPiece == Piece::NONE);
    assert(defaultPiece.type() == PieceType::NONE);
    assert(defaultPiece.color() == Color::NONE);

    assert(whitePawn.type() == PieceType::PAWN);
    assert(whitePawn.color() == Color::WHITE);
    assert(whitePawn == PieceType::PAWN);
    assert(whitePawn == Color::WHITE);
    assert(whitePawn != PieceType::KNIGHT);
    assert(whitePawn != Color::BLACK);

    assert(blackKnight.type() == PieceType::KNIGHT);
    assert(blackKnight.color() == Color::BLACK);
    assert(blackKnight == PieceType::KNIGHT);
    assert(blackKnight == Color::BLACK);

    assert(noneFromNone == Piece::NONE);
    assert(noneFromNone.type() == PieceType::NONE);
    assert(noneFromNone.color() == Color::NONE);
    assert(noneFromNoType == Piece::NONE);
    assert(noneFromNoColor == Piece::NONE);

    Piece whiteBishop(Piece::WHITE_BISHOP);
    Piece whiteRook(Piece::WHITE_ROOK);
    Piece whiteQueen(Piece::WHITE_QUEEN);
    Piece whiteKing(Piece::WHITE_KING);
    Piece blackPawn(Piece::BLACK_PAWN);
    Piece blackBishop(Piece::BLACK_BISHOP);
    Piece blackRook(Piece::BLACK_ROOK);
    Piece blackQueen(Piece::BLACK_QUEEN);
    Piece blackKing(Piece::BLACK_KING);
    assert(whiteBishop.type() == PieceType::BISHOP && whiteBishop.color() == Color::WHITE);
    assert(whiteRook.type() == PieceType::ROOK && whiteRook.color() == Color::WHITE);
    assert(whiteQueen.type() == PieceType::QUEEN && whiteQueen.color() == Color::WHITE);
    assert(whiteKing.type() == PieceType::KING && whiteKing.color() == Color::WHITE);
    assert(blackPawn.type() == PieceType::PAWN && blackPawn.color() == Color::BLACK);
    assert(blackBishop.type() == PieceType::BISHOP && blackBishop.color() == Color::BLACK);
    assert(blackRook.type() == PieceType::ROOK && blackRook.color() == Color::BLACK);
    assert(blackQueen.type() == PieceType::QUEEN && blackQueen.color() == Color::BLACK);
    assert(blackKing.type() == PieceType::KING && blackKing.color() == Color::BLACK);

    assert(Piece("P") == Piece::WHITE_PAWN);
    assert(Piece("N") == Piece::WHITE_KNIGHT);
    assert(Piece("B") == Piece::WHITE_BISHOP);
    assert(Piece("R") == Piece::WHITE_ROOK);
    assert(Piece("Q") == Piece::WHITE_QUEEN);
    assert(Piece("K") == Piece::WHITE_KING);
    assert(Piece("p") == Piece::BLACK_PAWN);
    assert(Piece("n") == Piece::BLACK_KNIGHT);
    assert(Piece("b") == Piece::BLACK_BISHOP);
    assert(Piece("r") == Piece::BLACK_ROOK);
    assert(Piece("q") == Piece::BLACK_QUEEN);
    assert(Piece("k") == Piece::BLACK_KING);
    assert(Piece("x") == Piece::NONE);

    for (int colorIndex = 0; colorIndex < 2; ++colorIndex) {
        for (int typeIndex = 0; typeIndex < 6; ++typeIndex) {
            Piece piece{PieceType(typeIndex), Color(colorIndex)};
            assert(piece.type() == PieceType(typeIndex));
            assert(piece.color() == Color(colorIndex));
        }
    }
}

void testFileAndRank() {
    File fileA(0);
    File fileH(7);
    File fileB(1);

    assert(File() == File::NONE);
    assert(fileA == File::FILE_A);
    assert(fileB == File::FILE_B);
    assert(fileH == File::FILE_H);
    assert(File::FILE_A < File::FILE_B);
    assert(File::FILE_H > File::FILE_G);
    assert(File::FILE_C <= File::FILE_C);
    assert(File::FILE_D >= File::FILE_D);

    Rank rank1(0);
    Rank rank8(7);
    Rank rank2(1);
    assert(Rank() == Rank::NONE);
    assert(rank1 == Rank::RANK_1);
    assert(rank2 == Rank::RANK_2);
    assert(rank8 == Rank::RANK_8);
    assert(Rank::RANK_1 < Rank::RANK_2);
    assert(Rank::RANK_8 > Rank::RANK_7);
    assert(Rank::RANK_4 <= Rank::RANK_4);
    assert(Rank::RANK_5 >= Rank::RANK_5);
}

void testDirection() {
    Direction north(Direction::NORTH);
    Direction east(Direction::EAST);
    Direction none(Direction::NONE);

    assert(Direction() == Direction::NONE);
    assert(north != east);
    assert(none == Direction::NONE);
    assert(static_cast<int>(Direction::NORTH) == 8);
    assert(static_cast<int>(Direction::SOUTH) == -8);
    assert(static_cast<int>(Direction::EAST) == 1);
    assert(static_cast<int>(Direction::WEST) == -1);
}

void testSquare() {
    Square defaultSquare;
    Square a1(Square::SQUARE_A1);
    Square h8(Square::SQUARE_H8);
    Square fromValidIndex(27);
    Square fromFileRank(File::FILE_E, Rank::RANK_4);
    assert(defaultSquare == Square::NONE);
    assert(a1.index() == 0);
    assert(h8.index() == 63);
    assert(fromValidIndex == Square::SQUARE_D4);
    assert(fromFileRank == Square::SQUARE_E4);
    assert(Square(File::NONE, Rank::RANK_1) == Square::NONE);
    assert(Square(File::FILE_A, Rank::NONE) == Square::NONE);

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square sq{File(file), Rank(rank)};
            assert(sq != Square::NONE);
            assert(sq.file() == File(file));
            assert(sq.rank() == Rank(rank));
            assert(sq.index() == rank * 8 + file);
        }
    }

    Square incA1(Square::SQUARE_A1);
    assert(++incA1 == Square::SQUARE_B1);
    assert(incA1++ == Square::SQUARE_B1);
    assert(incA1 == Square::SQUARE_C1);

    Square decC1(Square::SQUARE_C1);
    assert(--decC1 == Square::SQUARE_B1);
    assert(decC1-- == Square::SQUARE_B1);
    assert(decC1 == Square::SQUARE_A1);

    Square incH8(Square::SQUARE_H8);
    assert(++incH8 == Square::NONE);
    assert(incH8 == Square::NONE);

    Square decA1(Square::SQUARE_A1);
    assert(--decA1 == Square::NONE);
    assert(decA1 == Square::NONE);

    Square noneSquare;
    assert(++noneSquare == Square::NONE);
    assert(noneSquare++ == Square::NONE);
    assert(--noneSquare == Square::NONE);
    assert(noneSquare-- == Square::NONE);

    Square d4(Square::SQUARE_D4);
    assert(d4 + Direction::NORTH == Square::SQUARE_D5);
    assert(d4 + Direction::SOUTH == Square::SQUARE_D3);
    assert(d4 + Direction::EAST == Square::SQUARE_E4);
    assert(d4 + Direction::WEST == Square::SQUARE_C4);
    assert(d4 + Direction::NORTH_EAST == Square::SQUARE_E5);
    assert(d4 + Direction::NORTH_WEST == Square::SQUARE_C5);
    assert(d4 + Direction::SOUTH_EAST == Square::SQUARE_E3);
    assert(d4 + Direction::SOUTH_WEST == Square::SQUARE_C3);
    assert(d4 + Direction::NONE == d4);
    assert(noneSquare + Direction::NORTH == Square::NONE);

    Square h1(Square::SQUARE_H1);
    Square a1Edge(Square::SQUARE_A1);
    Square h8Edge(Square::SQUARE_H8);
    Square a8Edge(Square::SQUARE_A8);
    assert(h1 + Direction::EAST == Square::NONE);
    assert(h1 + Direction::NORTH_EAST == Square::NONE);
    assert(h1 + Direction::SOUTH_EAST == Square::NONE);
    assert(a1Edge + Direction::WEST == Square::NONE);
    assert(a1Edge + Direction::SOUTH == Square::NONE);
    assert(a1Edge + Direction::SOUTH_WEST == Square::NONE);
    assert(a1Edge + Direction::SOUTH_EAST == Square::NONE);
    assert(h8Edge + Direction::NORTH == Square::NONE);
    assert(h8Edge + Direction::NORTH_EAST == Square::NONE);
    assert(h8Edge + Direction::NORTH_WEST == Square::NONE);
    assert(a8Edge + Direction::NORTH == Square::NONE);
    assert(a8Edge + Direction::WEST == Square::NONE);
    assert(a8Edge + Direction::NORTH_WEST == Square::NONE);
}

void testBitboard() {
    Bitboard empty;
    Bitboard fromBits(0xF0F0ULL);
    Bitboard fileA(File::FILE_A);
    Bitboard fileH(File::FILE_H);
    Bitboard rank1(Rank::RANK_1);
    Bitboard rank8(Rank::RANK_8);

    assert(empty == 0ULL);
    assert(empty.empty());
    assert(fromBits == 0xF0F0ULL);
    assert(fileA == 0x0101010101010101ULL);
    assert(fileH == 0x8080808080808080ULL);
    assert(rank1 == 0x00000000000000FFULL);
    assert(rank8 == 0xFF00000000000000ULL);

    Bitboard a(0x0F0FULL);
    Bitboard b(0x00FFULL);
    assert((a & b) == 0x000FULL);
    assert((a | b) == 0x0FFFULL);
    assert((a ^ b) == 0x0FF0ULL);
    assert((~Bitboard(0ULL)) == ~0ULL);

    Bitboard c(0xAA00ULL);
    c &= Bitboard(0x0FF0ULL);
    assert(c == 0x0A00ULL);
    c |= Bitboard(0x000FULL);
    assert(c == 0x0A0FULL);
    c ^= Bitboard(0x0003ULL);
    assert(c == 0x0A0CULL);

    assert(Bitboard(5ULL) == 5ULL);
    assert(Bitboard(5ULL) != 6ULL);
    assert(Bitboard(5ULL) && 1ULL);
    assert(!(Bitboard(0ULL) && 1ULL));
    assert(Bitboard(0ULL) || 7ULL);
    assert(!(Bitboard(0ULL) || 0ULL));
    assert((Bitboard(0xF0ULL) & 0x0FULL) == 0ULL);
    assert((Bitboard(0xF0ULL) | 0x0FULL) == 0xFFULL);
    assert((Bitboard(0xAAULL) ^ 0xFFULL) == 0x55ULL);
    assert((Bitboard(1ULL) << 3) == 8ULL);
    assert((Bitboard(8ULL) >> 3) == 1ULL);

    Bitboard d(0x0F0FULL);
    d &= 0x00FFULL;
    assert(d == 0x000FULL);
    d |= 0x0F00ULL;
    assert(d == 0x0F0FULL);
    d ^= 0x000FULL;
    assert(d == 0x0F00ULL);

    Bitboard e;
    assert(e.empty());
    e.set(0).set(63);
    assert(e.check(0));
    assert(e.check(63));
    assert(e.count() == 2);
    e.toggle(0);
    assert(!e.check(0));
    assert(e.check(63));
    e.clear(63);
    assert(e.empty());
    e.set(5).set(9);
    e.clear();
    assert(e.empty());

    Bitboard f((1ULL << 3) | (1ULL << 20) | (1ULL << 50));
    assert(f.lsb() == 3);
    assert(f.msb() == 50);
    assert(f.count() == 3);

    Bitboard g((1ULL << 2) | (1ULL << 10) | (1ULL << 12));
    assert(g.pop() == 2ULL);
    assert(g.count() == 2);
    assert(g.pop() == 10ULL);
    assert(g.pop() == 12ULL);
    assert(g.empty());

    Bitboard h(0x123456789ABCDEF0ULL);
    assert(h.bits() == 0x123456789ABCDEF0ULL);
}

void testMove() {
    Move defaultMove;
    assert(defaultMove.move() == 0);
    assert(defaultMove.from() == Square::SQUARE_A1);
    assert(defaultMove.to() == Square::SQUARE_A1);
    assert(defaultMove.type() == Move::NORMAL);
    assert(defaultMove.promotion() == PieceType::NONE);

    Move normalMove = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    assert(normalMove.from() == Square::SQUARE_E2);
    assert(normalMove.to() == Square::SQUARE_E4);
    assert(normalMove.type() == Move::NORMAL);
    assert(normalMove.promotion() == PieceType::NONE);
    assert(normalMove.move() != 0);

    Move enPassantMove = Move::create<Move::EN_PASSANT>(Square::SQUARE_E5, Square::SQUARE_D6);
    assert(enPassantMove.from() == Square::SQUARE_E5);
    assert(enPassantMove.to() == Square::SQUARE_D6);
    assert(enPassantMove.type() == Move::EN_PASSANT);
    assert(enPassantMove.promotion() == PieceType::NONE);

    Move castlingMove = Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_G1);
    assert(castlingMove.from() == Square::SQUARE_E1);
    assert(castlingMove.to() == Square::SQUARE_G1);
    assert(castlingMove.type() == Move::CASTLING);

    Move promotionMove = Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_A8, PieceType::QUEEN);
    assert(promotionMove.from() == Square::SQUARE_A7);
    assert(promotionMove.to() == Square::SQUARE_A8);
    assert(promotionMove.type() == Move::PROMOTION);
    assert(promotionMove.promotion() == PieceType::QUEEN);

    Move promotionToKnight = Move::create<Move::PROMOTION>(Square::SQUARE_B7, Square::SQUARE_B8, PieceType::KNIGHT);
    assert(promotionToKnight.promotion() == PieceType::KNIGHT);

    Move copiedMove(normalMove.move());
    assert(copiedMove == normalMove);
    assert(copiedMove != promotionMove);
}

void testMoveList() {
    MoveList list;
    assert(list.empty());
    assert(list.size() == 0);
    assert(list.begin() == list.end());

    const Move firstMove = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    Move secondMove = Move::create<Move::EN_PASSANT>(Square::SQUARE_E5, Square::SQUARE_D6);
    const Move thirdMove = Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_G1);

    list.add(firstMove);
    list.add(std::move(secondMove));
    list.add(thirdMove);

    assert(!list.empty());
    assert(list.size() == 3);
    assert(list.front() == firstMove);
    assert(list.back() == thirdMove);

    assert(list.at(0) == firstMove);
    assert(list.at(1).type() == Move::EN_PASSANT);
    assert(list[2] == thirdMove);

    list[2] = firstMove;
    assert(list.back() == firstMove);

    std::size_t count = 0;
    for (const Move &move : list) {
        assert(move == list.at(count));
        ++count;
    }
    assert(count == list.size());

    assert(list.find(firstMove) == 0);
    assert(list.find(Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_C1)) == list.size());

    list.clear();
    assert(list.empty());
    assert(list.size() == 0);
    assert(list.begin() == list.end());

    for (std::size_t i = 0; i < MoveList::MAX_MOVES; ++i) {
        list.add(Move::create<Move::NORMAL>(Square::SQUARE_A1, Square((i + 1) % 64)));
    }
    assert(list.size() == MoveList::MAX_MOVES);
    assert(!list.empty());
}

int main() {
    testColor();
    testPieceType();
    testPiece();
    testFileAndRank();
    testDirection();
    testSquare();
    testBitboard();
    testMove();
    testMoveList();

    cout << "Success!" << endl;

    return 0;
}
