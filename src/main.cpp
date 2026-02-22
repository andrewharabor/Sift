
#include <iostream>

#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"


using namespace std;
using namespace Clownfish;

namespace {

void testColor() {
    Color defaultColor;
    Color whiteColor(Color::WHITE);
    Color blackColor(Color::BLACK);

    assert(defaultColor == Color::NONE);
    assert(whiteColor != blackColor);
    assert(!whiteColor == Color::BLACK);
    assert(!blackColor == Color::WHITE);
    assert(Color::WHITE != Color::BLACK);
    assert(defaultColor == Color::NONE);
    assert(static_cast<int>(Color::WHITE) == 0);
    assert(static_cast<int>(Color::BLACK) == 1);
    assert(static_cast<int>(Color::NONE) == 2);
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
    File fileInvalidLow(-1);
    File fileInvalidHigh(8);

    assert(File() == File::NONE);
    assert(fileA == File::FILE_A);
    assert(fileH == File::FILE_H);
    assert(fileInvalidLow == File::NONE);
    assert(fileInvalidHigh == File::NONE);
    assert(File::FILE_A < File::FILE_B);
    assert(File::FILE_H > File::FILE_G);
    assert(File::FILE_C <= File::FILE_C);
    assert(File::FILE_D >= File::FILE_D);

    Rank rank1(0);
    Rank rank8(7);
    Rank rankInvalidLow(-1);
    Rank rankInvalidHigh(8);
    assert(Rank() == Rank::NONE);
    assert(rank1 == Rank::RANK_1);
    assert(rank8 == Rank::RANK_8);
    assert(rankInvalidLow == Rank::NONE);
    assert(rankInvalidHigh == Rank::NONE);
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
    Square fromInvalidIndexLow(-1);
    Square fromInvalidIndexHigh(64);
    Square fromFileRank(File::FILE_E, Rank::RANK_4);
    assert(defaultSquare == Square::NONE);
    assert(a1.index() == 0);
    assert(h8.index() == 63);
    assert(fromValidIndex == Square::SQUARE_D4);
    assert(fromInvalidIndexLow == Square::NONE);
    assert(fromInvalidIndexHigh == Square::NONE);
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

} // namespace

int main() {
    testColor();
    testPieceType();
    testPiece();
    testFileAndRank();
    testDirection();
    testSquare();

    cout << "success!" << endl;

    return 0;
}
