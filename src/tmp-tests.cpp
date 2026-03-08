
#include <iostream>
#include <unordered_set>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coordinates.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "zobrist.hpp"


using namespace std;
using namespace Clownfish;

Bitboard naiveKnightAttacks(Square square) {
    assert(square != Square::NONE);
    static constexpr int offsets[8][2] = {
        { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 },
        { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 }
    };
    Bitboard attacks(0ULL);
    const int file = square.file();
    const int rank = square.rank();

    for (const auto &offset : offsets) {
        const int newFile = file + offset[0];
        const int newRank = rank + offset[1];
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            attacks.set(Square(File(newFile), Rank(newRank)).index());
        }
    }

    return attacks;
}

Bitboard naiveKingAttacks(Square square) {
    assert(square != Square::NONE);
    static constexpr int offsets[8][2] = {
        { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
        { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 }
    };
    Bitboard attacks(0ULL);
    const int file = square.file();
    const int rank = square.rank();

    for (const auto &offset : offsets) {
        const int newFile = file + offset[0];
        const int newRank = rank + offset[1];
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            attacks.set(Square(File(newFile), Rank(newRank)).index());
        }
    }

    return attacks;
}

Bitboard naiveSliderAttacks(Square square, Bitboard occupied, bool rookLike) {
    assert(square != Square::NONE);
    static constexpr int bishopDirections[4][2] = {{ 1, 1 }, { 1, -1 }, { -1, -1 }, { -1, 1 }};
    static constexpr int rookDirections[4][2] = {{ 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 }};

    Bitboard attacks(0ULL);
    const int file = square.file();
    const int rank = square.rank();
    const int (*directions)[2] = rookLike ? rookDirections : bishopDirections;

    for (int i = 0; i < 4; ++i) {
        const int fileOffset = directions[i][0];
        const int rankOffset = directions[i][1];
        int currentFile = file + fileOffset;
        int currentRank = rank + rankOffset;

        while (currentFile >= 0 && currentFile < 8 && currentRank >= 0 && currentRank < 8) {
            const int index = Square(File(currentFile), Rank(currentRank)).index();
            attacks.set(index);
            if (occupied.get(index)) {
                break;
            }
            currentFile += fileOffset;
            currentRank += rankOffset;
        }
    }

    return attacks;
}

Bitboard sliderRelevantMask(Square square, bool rookLike) {
    const Bitboard edges = ((Bitboard(Rank::RANK_1) | Bitboard(Rank::RANK_8)) & ~Bitboard(square.rank())) |
        ((Bitboard(File::FILE_A) | Bitboard(File::FILE_H)) & ~Bitboard(square.file()));
    return naiveSliderAttacks(square, Bitboard(0ULL), rookLike) & ~edges;
}

Bitboard singleStepExpected(Square square, Direction direction) {
    const Square destination = square + direction;
    if (destination == Square::NONE) {
        return Bitboard(0ULL);
    }
    return Bitboard(0ULL).set(destination.index());
}

int signum(int value) {
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

Bitboard expectedBetween(Square from, Square to) {
    Bitboard between(0ULL);
    between.set(to.index());

    const int fromFile = from.file();
    const int fromRank = from.rank();
    const int toFile = to.file();
    const int toRank = to.rank();

    const int deltaFile = toFile - fromFile;
    const int deltaRank = toRank - fromRank;
    const int absDeltaFile = deltaFile < 0 ? -deltaFile : deltaFile;
    const int absDeltaRank = deltaRank < 0 ? -deltaRank : deltaRank;

    const bool sameFile = (deltaFile == 0);
    const bool sameRank = (deltaRank == 0);
    const bool sameDiagonal = (absDeltaFile == absDeltaRank);
    if (!(sameFile || sameRank || sameDiagonal)) {
        return between;
    }

    const int stepFile = signum(deltaFile);
    const int stepRank = signum(deltaRank);

    int currentFile = fromFile + stepFile;
    int currentRank = fromRank + stepRank;
    while (currentFile != toFile || currentRank != toRank) {
        between.set(Square(File(currentFile), Rank(currentRank)).index());
        currentFile += stepFile;
        currentRank += stepRank;
    }

    return between;
}

void assertMoveRoundTrip(Position &position, Move move) {
    const Position before = position;
    const std::uint64_t predictedHash = position.zobristAfter(move);

    position.make(move);
    assert(position.hash() == predictedHash);
    assert(position.hash() == position.zobrist());

    position.unmake(move);
    assert(position == before);
    assert(position.hash() == position.zobrist());
}

void assertSequenceRoundTrip(Position position, const std::vector<Move> &sequence) {
    std::vector<Position> snapshots;
    snapshots.reserve(sequence.size() + 1);
    snapshots.push_back(position);

    for (const Move move : sequence) {
        const std::uint64_t predictedHash = position.zobristAfter(move);
        position.make(move);
        assert(position.hash() == predictedHash);
        assert(position.hash() == position.zobrist());
        snapshots.push_back(position);
    }

    for (std::size_t index = sequence.size(); index > 0; --index) {
        position.unmake(sequence[index - 1]);
        assert(position == snapshots[index - 1]);
        assert(position.hash() == position.zobrist());
    }
}

void playSimpleRepetitionCycle(Position &position) {
    const Move whiteOut = Move::create<Move::NORMAL>(Square::SQUARE_G1, Square::SQUARE_F3);
    const Move blackOut = Move::create<Move::NORMAL>(Square::SQUARE_G8, Square::SQUARE_F6);
    const Move whiteBack = Move::create<Move::NORMAL>(Square::SQUARE_F3, Square::SQUARE_G1);
    const Move blackBack = Move::create<Move::NORMAL>(Square::SQUARE_F6, Square::SQUARE_G8);

    position.make(whiteOut);
    position.make(blackOut);
    position.make(whiteBack);
    position.make(blackBack);
}

void testColor() {
    Color defaultColor;
    Color whiteColor(Color::WHITE);
    Color blackColor(Color::BLACK);
    Color whiteFromInt(0);
    Color blackFromInt(1);
    Color noneFromInt(2);

    assert(defaultColor == Color::NONE);
    assert(whiteColor != blackColor);
    assert(~whiteColor == Color::BLACK);
    assert(~blackColor == Color::WHITE);
    assert(Color::WHITE != Color::BLACK);
    assert(defaultColor == Color::NONE);
    const int whiteInt = Color(Color::WHITE);
    const int blackInt = Color(Color::BLACK);
    const int noneInt = Color(Color::NONE);
    assert(whiteInt == 0);
    assert(blackInt == 1);
    assert(noneInt == 2);
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
    const int northInt = Direction(Direction::NORTH);
    const int southInt = Direction(Direction::SOUTH);
    const int eastInt = Direction(Direction::EAST);
    const int westInt = Direction(Direction::WEST);
    assert(northInt == 8);
    assert(southInt == -8);
    assert(eastInt == 1);
    assert(westInt == -1);
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
    assert(e.get(0));
    assert(e.get(63));
    assert(e.count() == 2);
    e.toggle(0);
    assert(!e.get(0));
    assert(e.get(63));
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
    assert(defaultMove.internal() == 0);
    assert(defaultMove.from() == Square::SQUARE_A1);
    assert(defaultMove.to() == Square::SQUARE_A1);
    assert(defaultMove.type() == Move::NORMAL);
    assert(defaultMove.promotion() == PieceType::NONE);

    Move normalMove = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    assert(normalMove.from() == Square::SQUARE_E2);
    assert(normalMove.to() == Square::SQUARE_E4);
    assert(normalMove.type() == Move::NORMAL);
    assert(normalMove.promotion() == PieceType::NONE);
    assert(normalMove.internal() != 0);

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

    Move copiedMove(normalMove.internal());
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

void testAttacksAndMagic() {
    Attacks::init();

    for (int rank = 0; rank < 8; ++rank) {
        assert(Attacks::RANK_MASKS[rank] == Bitboard(Rank(rank)));
    }

    for (int file = 0; file < 8; ++file) {
        assert(Attacks::FILE_MASKS[file] == Bitboard(File(file)));
    }

    for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
        const Square square(squareIndex);
        const Bitboard bit = Bitboard(0ULL).set(squareIndex);

        assert(Attacks::shift<Direction::NORTH>(bit) == singleStepExpected(square, Direction::NORTH));
        assert(Attacks::shift<Direction::EAST>(bit) == singleStepExpected(square, Direction::EAST));
        assert(Attacks::shift<Direction::SOUTH>(bit) == singleStepExpected(square, Direction::SOUTH));
        assert(Attacks::shift<Direction::WEST>(bit) == singleStepExpected(square, Direction::WEST));
        assert(Attacks::shift<Direction::NORTH_EAST>(bit) == singleStepExpected(square, Direction::NORTH_EAST));
        assert(Attacks::shift<Direction::SOUTH_EAST>(bit) == singleStepExpected(square, Direction::SOUTH_EAST));
        assert(Attacks::shift<Direction::SOUTH_WEST>(bit) == singleStepExpected(square, Direction::SOUTH_WEST));
        assert(Attacks::shift<Direction::NORTH_WEST>(bit) == singleStepExpected(square, Direction::NORTH_WEST));

        Bitboard expectedWhitePawn(0ULL);
        const Square whiteLeft = square + Direction::NORTH_WEST;
        const Square whiteRight = square + Direction::NORTH_EAST;
        if (whiteLeft != Square::NONE) {
            expectedWhitePawn.set(whiteLeft.index());
        }
        if (whiteRight != Square::NONE) {
            expectedWhitePawn.set(whiteRight.index());
        }

        Bitboard expectedBlackPawn(0ULL);
        const Square blackLeft = square + Direction::SOUTH_EAST;
        const Square blackRight = square + Direction::SOUTH_WEST;
        if (blackLeft != Square::NONE) {
            expectedBlackPawn.set(blackLeft.index());
        }
        if (blackRight != Square::NONE) {
            expectedBlackPawn.set(blackRight.index());
        }

        assert(Attacks::pawn(square, Color::WHITE) == expectedWhitePawn);
        assert(Attacks::pawn(square, Color::BLACK) == expectedBlackPawn);

        Bitboard expectedWhiteLeft(0ULL);
        Bitboard expectedWhiteRight(0ULL);
        Bitboard expectedBlackLeft(0ULL);
        Bitboard expectedBlackRight(0ULL);
        if (whiteLeft != Square::NONE) {
            expectedWhiteLeft.set(whiteLeft.index());
        }
        if (whiteRight != Square::NONE) {
            expectedWhiteRight.set(whiteRight.index());
        }
        if (blackLeft != Square::NONE) {
            expectedBlackLeft.set(blackLeft.index());
        }
        if (blackRight != Square::NONE) {
            expectedBlackRight.set(blackRight.index());
        }

        assert(Attacks::pawnLeftAttacks<Color::WHITE>(bit) == expectedWhiteLeft);
        assert(Attacks::pawnRightAttacks<Color::WHITE>(bit) == expectedWhiteRight);
        assert(Attacks::pawnLeftAttacks<Color::BLACK>(bit) == expectedBlackLeft);
        assert(Attacks::pawnRightAttacks<Color::BLACK>(bit) == expectedBlackRight);

        assert(Attacks::knight(square) == naiveKnightAttacks(square));
        assert(Attacks::king(square) == naiveKingAttacks(square));
    }

    constexpr Bitboard fullBoard(0xFFFFFFFFFFFFFFFFULL);
    const Bitboard whiteFullExpected = Attacks::shift<Direction::NORTH_WEST>(fullBoard) | Attacks::shift<Direction::NORTH_EAST>(fullBoard);
    const Bitboard blackFullExpected = Attacks::shift<Direction::SOUTH_WEST>(fullBoard) | Attacks::shift<Direction::SOUTH_EAST>(fullBoard);
    assert((Attacks::pawnLeftAttacks<Color::WHITE>(fullBoard) | Attacks::pawnRightAttacks<Color::WHITE>(fullBoard)) == whiteFullExpected);
    assert((Attacks::pawnLeftAttacks<Color::BLACK>(fullBoard) | Attacks::pawnRightAttacks<Color::BLACK>(fullBoard)) == blackFullExpected);

    for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
        const Square square(squareIndex);

        const Bitboard rookMask = sliderRelevantMask(square, true);
        const std::uint64_t rookMaskBits = rookMask.bits();
        std::uint64_t rookOccupiedBits = 0ULL;
        do {
            const Bitboard occupied(rookOccupiedBits);
            const Bitboard expected = naiveSliderAttacks(square, occupied, true);
            const Bitboard actual = Attacks::rook(square, occupied);
            assert(actual == expected);
            assert(Attacks::slider<PieceType::ROOK>(square, occupied) == expected);

            const Bitboard noisyOccupied(rookOccupiedBits | (~rookMaskBits));
            assert(Attacks::rook(square, noisyOccupied) == expected);

            rookOccupiedBits = (rookOccupiedBits - rookMaskBits) & rookMaskBits;
        } while (rookOccupiedBits != 0ULL);

        const Bitboard bishopMask = sliderRelevantMask(square, false);
        const std::uint64_t bishopMaskBits = bishopMask.bits();
        std::uint64_t bishopOccupiedBits = 0ULL;
        do {
            const Bitboard occupied(bishopOccupiedBits);
            const Bitboard expected = naiveSliderAttacks(square, occupied, false);
            const Bitboard actual = Attacks::bishop(square, occupied);
            assert(actual == expected);
            assert(Attacks::slider<PieceType::BISHOP>(square, occupied) == expected);

            const Bitboard noisyOccupied(bishopOccupiedBits | (~bishopMaskBits));
            assert(Attacks::bishop(square, noisyOccupied) == expected);

            bishopOccupiedBits = (bishopOccupiedBits - bishopMaskBits) & bishopMaskBits;
        } while (bishopOccupiedBits != 0ULL);

        const Bitboard sampleOccupied = Bitboard(0x0102040810204080ULL) | Bitboard(0x8040201008040201ULL);
        const Bitboard queenExpected = Attacks::rook(square, sampleOccupied) | Attacks::bishop(square, sampleOccupied);
        assert(Attacks::queen(square, sampleOccupied) == queenExpected);
        assert(Attacks::slider<PieceType::QUEEN>(square, sampleOccupied) == queenExpected);
    }

    Attacks::init();
    for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
        const Square square(squareIndex);
        const Bitboard occupied = Bitboard(0x00FF00000000FF00ULL) | Bitboard(1ULL << squareIndex);
        assert(Attacks::rook(square, occupied) == naiveSliderAttacks(square, occupied, true));
        assert(Attacks::bishop(square, occupied) == naiveSliderAttacks(square, occupied, false));
    }
}

void testZobrist() {
    assert(Zobrist::piece(Piece::BLACK_PAWN, Square::SQUARE_A1) == 0x9D39247E33776D41ULL);
    assert(Zobrist::piece(Piece::WHITE_PAWN, Square::SQUARE_A1) == 0x5355F900C2A82DC7ULL);
    assert(Zobrist::sideToMove() == 0xF8D626AAAF278509ULL);

    assert(Zobrist::piece(Piece::WHITE_KNIGHT, Square::SQUARE_C3) == Zobrist::piece(Piece::WHITE_KNIGHT, Square::SQUARE_C3));
    assert(Zobrist::piece(Piece::WHITE_KNIGHT, Square::SQUARE_C3) != Zobrist::piece(Piece::WHITE_KNIGHT, Square::SQUARE_C4));
    assert(Zobrist::piece(Piece::WHITE_KNIGHT, Square::SQUARE_C3) != Zobrist::piece(Piece::BLACK_KNIGHT, Square::SQUARE_C3));

    assert(Zobrist::castling(0) == 0ULL);
    for (int bit = 0; bit < 4; ++bit) {
        assert(Zobrist::castling(1 << bit) == Zobrist::castlingIndex(bit));
    }

    for (int rights = 0; rights < 16; ++rights) {
        std::uint64_t expected = 0ULL;
        for (int bit = 0; bit < 4; ++bit) {
            if ((rights & (1 << bit)) != 0) {
                expected ^= Zobrist::castlingIndex(bit);
            }
        }
        assert(Zobrist::castling(rights) == expected);
    }

    std::unordered_set<std::uint64_t> enPassantHashes;
    for (int file = 0; file < 8; ++file) {
        const std::uint64_t hash = Zobrist::enPassant(File(file));
        assert(hash != 0ULL);
        enPassantHashes.insert(hash);
    }
    assert(enPassantHashes.size() == 8);

    std::unordered_set<std::uint64_t> castlingIndexHashes;
    for (int bit = 0; bit < 4; ++bit) {
        const std::uint64_t hash = Zobrist::castlingIndex(bit);
        assert(hash != 0ULL);
        castlingIndexHashes.insert(hash);
    }
    assert(castlingIndexHashes.size() == 4);
}

void testColorExtras() {
    const Color noneColor(Color::NONE);
    const Color whiteColor(Color::WHITE);

    assert(~noneColor == Color::NONE);
    assert(whiteColor.internal() == Color::WHITE);
    assert(Color(Color::BLACK).internal() == Color::BLACK);
}

void testPieceTypeExtras() {
    const PieceType noneType;
    const PieceType knightType(PieceType::KNIGHT);

    assert(static_cast<std::string>(noneType) == " ");
    assert(static_cast<std::string>(knightType) == "N");
    assert(PieceType("n") == PieceType::KNIGHT);
    assert(PieceType(PieceType::QUEEN).internal() == PieceType::QUEEN);
}

void testPieceExtras() {
    const Piece blackKingFromInt(11);
    const Piece whiteBishop(Piece::WHITE_BISHOP);
    const Piece blackBishop(Piece::BLACK_BISHOP);

    assert(blackKingFromInt == Piece::BLACK_KING);
    assert(static_cast<int>(blackKingFromInt) == 11);
    assert(static_cast<std::string>(Piece(Piece::NONE)) == " ");
    assert(Piece(PieceType::QUEEN, Color::BLACK).internal() == Piece::BLACK_QUEEN);
    assert(whiteBishop < blackBishop);
}

void testFileAndRankExtras() {
    const File fileC("c");
    const Rank rank7("7");

    assert(fileC == File::FILE_C);
    assert(static_cast<std::string>(fileC) == "c");
    assert(fileC.internal() == File::FILE_C);

    assert(rank7 == Rank::RANK_7);
    assert(static_cast<std::string>(rank7) == "7");
    assert(rank7.internal() == Rank::RANK_7);

    assert(Rank(Rank::RANK_1).backRank(Color::WHITE));
    assert(Rank(Rank::RANK_8).backRank(Color::BLACK));
    assert(!Rank(Rank::RANK_7).backRank(Color::BLACK));
}

void testDirectionExtras() {
    const Direction north(Direction::NORTH);
    const Direction northForBlack(Direction::NORTH, Color::BLACK);
    const Direction eastForWhite(Direction::EAST, Color::WHITE);

    assert(northForBlack == Direction::SOUTH);
    assert(eastForWhite == Direction::EAST);
    assert(-north == Direction::SOUTH);
    assert(-Direction(Direction::NORTH_EAST) == Direction::SOUTH_WEST);
    assert(Direction(Direction::WEST).internal() == Direction::WEST);
}

void testSquareExtras() {
    const Square fromString("e4");
    const Square none;

    assert(fromString == Square::SQUARE_E4);
    assert(static_cast<std::string>(fromString) == "e4");
    assert(static_cast<std::string>(none).empty());

    const Square xorSquare = Square(Square::SQUARE_E4) ^ Square(Square::SQUARE_A1);
    assert(xorSquare.index() == (Square(Square::SQUARE_E4).index() ^ Square(Square::SQUARE_A1).index()));

    Square mirrored(Square::SQUARE_A1);
    mirrored.mirror();
    assert(mirrored == Square::SQUARE_A8);
    mirrored.mirror();
    assert(mirrored == Square::SQUARE_A1);

    assert(Square(Square::SQUARE_E4) - Direction::NORTH == Square::SQUARE_E3);
    assert(Square(Square::SQUARE_A2).relative(Color::BLACK) == Square::SQUARE_A7);

    assert(Square::sameColor(Square::SQUARE_A1, Square::SQUARE_C3));
    assert(!Square::sameColor(Square::SQUARE_A1, Square::SQUARE_H1));
    assert(Square::indexDistance(Square::SQUARE_A1, Square::SQUARE_H8) == 63);

    assert(Square::kingCastlingSquare(Color::WHITE, true) == Square::SQUARE_G1);
    assert(Square::kingCastlingSquare(Color::BLACK, false) == Square::SQUARE_C8);
    assert(Square::rookCastlingSquare(Color::WHITE, true) == Square::SQUARE_F1);
    assert(Square::rookCastlingSquare(Color::BLACK, false) == Square::SQUARE_D8);

    assert(Square(Square::SQUARE_E6).enPassantSquare() == Square::SQUARE_E5);
    assert(Square(Square::SQUARE_B2).internal() == Square::SQUARE_B2);
}

void testBitboardExtras() {
    const Bitboard fromSquare(Square::SQUARE_D5);
    const Bitboard nonEmpty(0xF0ULL);
    const Bitboard empty(0ULL);

    assert(fromSquare.count() == 1);
    assert(fromSquare.get(Square(Square::SQUARE_D5).index()));

    assert(static_cast<bool>(fromSquare));
    assert(!static_cast<bool>(empty));

    assert(!(nonEmpty && empty));
    assert(nonEmpty || empty);
}

void testMoveExtras() {
    const Move nullMove = Move::createNull();
    assert(nullMove.internal() == Move::NULL_MOVE);
    assert(nullMove == Move(Move::NULL_MOVE));

    Move scoredMove = Move::create<Move::NORMAL>(Square::SQUARE_A2, Square::SQUARE_A3);
    scoredMove.setScore(32700);
    assert(scoredMove.score() == 32700);
}

void testMoveListExtras() {
    MoveList list;
    const Move a2a3 = Move::create<Move::NORMAL>(Square::SQUARE_A2, Square::SQUARE_A3);
    const Move b2b3 = Move::create<Move::NORMAL>(Square::SQUARE_B2, Square::SQUARE_B3);

    list.add(a2a3);
    list.add(b2b3);

    const MoveList &constList = list;
    assert(constList.front() == a2a3);
    assert(constList.back() == b2b3);
    assert(constList.at(1) == b2b3);
    assert(constList.begin() + 2 == constList.end());
}

void testAttacksBetween() {
    Attacks::init();

    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            const Square fromSquare(from);
            const Square toSquare(to);
            const Bitboard actual = Attacks::between(fromSquare, toSquare);
            const Bitboard expected = expectedBetween(fromSquare, toSquare);
            if (actual != expected) {
                cerr << "between mismatch from=" << static_cast<string>(fromSquare)
                    << " to=" << static_cast<string>(toSquare)
                    << " actual=0x" << hex << actual.bits()
                    << " expected=0x" << expected.bits() << dec << endl;
                assert(false);
            }
        }
    }
}

void testAttacksBetweenSymmetry() {
    Attacks::init();

    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            const Square fromSquare(from);
            const Square toSquare(to);

            const Bitboard forward = Attacks::between(fromSquare, toSquare) & ~Bitboard(toSquare);
            const Bitboard backward = Attacks::between(toSquare, fromSquare) & ~Bitboard(fromSquare);
            assert(forward == backward);
            assert(!forward.get(fromSquare.index()));
            assert(!backward.get(toSquare.index()));
        }
    }
}

void testPositionCastlingRights() {
    using CastlingRights = Position::CastlingRights;

    CastlingRights rights;
    assert(rights.empty());

    rights.set(CastlingRights::WHITE_KINGSIDE);
    rights.set(CastlingRights::BLACK_QUEENSIDE);
    assert(rights.get(CastlingRights::WHITE_KINGSIDE));
    assert(rights.get(CastlingRights::BLACK_QUEENSIDE));
    assert(rights.get(Color::WHITE));
    assert(rights.get(Color::BLACK));

    rights.clear(CastlingRights::WHITE_KINGSIDE);
    assert(!rights.get(CastlingRights::WHITE_KINGSIDE));
    rights.clear(Color::BLACK);
    assert(!rights.get(Color::BLACK));
    assert(rights.empty());

    const CastlingRights fromSide(CastlingRights::WHITE_QUEENSIDE);
    const CastlingRights fromValue(5);
    assert(fromSide.hash() == 2);
    assert(static_cast<int>(fromValue) == 5);
    assert(fromValue.internal() == 5);

    assert(CastlingRights::hashIndex(CastlingRights::WHITE_KINGSIDE) == 0);
    assert(CastlingRights::hashIndex(CastlingRights::WHITE_QUEENSIDE) == 1);
    assert(CastlingRights::hashIndex(CastlingRights::BLACK_KINGSIDE) == 2);
    assert(CastlingRights::hashIndex(CastlingRights::BLACK_QUEENSIDE) == 3);

    assert(CastlingRights::closestSide(Square::SQUARE_A1, Square::SQUARE_E1, Color::WHITE) == CastlingRights::WHITE_QUEENSIDE);
    assert(CastlingRights::closestSide(Square::SQUARE_H1, Square::SQUARE_E1, Color::WHITE) == CastlingRights::WHITE_KINGSIDE);
    assert(CastlingRights::closestSide(Square::SQUARE_A8, Square::SQUARE_E8, Color::BLACK) == CastlingRights::BLACK_QUEENSIDE);
    assert(CastlingRights::closestSide(Square::SQUARE_H8, Square::SQUARE_E8, Color::BLACK) == CastlingRights::BLACK_KINGSIDE);

    assert(CastlingRights::rookFile(CastlingRights::WHITE_KINGSIDE) == File::FILE_H);
    assert(CastlingRights::rookFile(CastlingRights::BLACK_KINGSIDE) == File::FILE_H);
    assert(CastlingRights::rookFile(CastlingRights::WHITE_QUEENSIDE) == File::FILE_A);
    assert(CastlingRights::rookFile(CastlingRights::BLACK_QUEENSIDE) == File::FILE_A);
}

void testPositionFenAndAccessors() {
    Attacks::init();

    Position position;
    assert(position.fen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    assert(position.sideToMove() == Color::WHITE);
    assert(position.halfmoveClock() == 0);
    assert(position.fullMoveNumber() == 1);
    assert(position.enPassantSquare() == Square::NONE);
    assert(position.castlingRights().get(Position::CastlingRights::WHITE_KINGSIDE));
    assert(position.castlingRights().get(Position::CastlingRights::WHITE_QUEENSIDE));
    assert(position.castlingRights().get(Position::CastlingRights::BLACK_KINGSIDE));
    assert(position.castlingRights().get(Position::CastlingRights::BLACK_QUEENSIDE));
    assert(position.hash() == position.zobrist());

    assert(position.set("   8/8/8/8/8/8/8/K6k b - - 7 42"));
    assert(position.sideToMove() == Color::BLACK);
    assert(position.halfmoveClock() == 7);
    assert(position.fullMoveNumber() == 42);
    assert(position.fen() == "8/8/8/8/8/8/8/K6k b - - 7 42");
    assert(position.hash() == position.zobrist());

    assert(!position.set(""));
    assert(!position.set("8/8/8/8/8/8/8/8 x - - 0 1"));
    assert(!position.set("8/8/8/8/8/8/8/8 w Ka - 0 1"));
    assert(!position.set("8/8/8/8/8/8/8/8 w - a3 0 1"));
    assert(!position.set("8/8/8/8/8/8/8/8 w - - abc 1"));
    assert(!position.set("8/8/8/8/8/8/8/8 w - - 0 0"));
    assert(!position.set("8/8/8/8/8/8/8/8 w - - 300 1"));
    assert(!position.set("8/8/8/8/8/8/8/7x w - - 0 1"));

    Position copy = Position();
    Position duplicate = copy;
    assert(copy == duplicate);
    const Move e2e4 = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    duplicate.make(e2e4);
    assert(!(copy == duplicate));
    duplicate.unmake(e2e4);
    assert(copy == duplicate);
}

void testPositionFenParsingEdgeCases() {
    Attacks::init();

    Position position;

    assert(position.set("4k3/8/8/8/8/8/8/4K3"));
    assert(position.fen() == "4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    assert(position.set("4k3/8/8/8/8/8/8/4K3 b"));
    assert(position.sideToMove() == Color::BLACK);
    assert(position.fen() == "4k3/8/8/8/8/8/8/4K3 b - - 0 1");
    assert(position.hash() == position.zobrist());

    assert(!position.set("8/8/8/8/8/8/8/4K3 w - - 0 1"));
    assert(!position.set("4k3/8/8/8/8/8/8/8 w - - 0 1"));

    assert(!position.set("4k3/8/8/8/8/8/8/4K3 w - i3 0 1"));
    assert(!position.set("4k3/8/8/8/8/8/8/4K3 w - - nope 1"));
    assert(!position.set("4k3/8/8/8/8/8/8/4K3 w - - 0 nope"));
    assert(!position.set("4k3/8/8/8/8/8/8/4K3 w - e3 0 1"));
    assert(!position.set("4k3/8/8/8/8/8/8/4K3 b - e6 0 1"));

    assert(position.set("4k3/8/8/8/8/8/8/4K3 w - - -1 1"));
    assert(position.set("4k3/8/8/8/8/8/8/4K3 w - - 0 0"));
}

void testPositionFenRoundTrip() {
    Attacks::init();

    const std::vector<std::string_view> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/ppp2ppp/2n5/3pp3/3PP3/2P2N2/PP3PPP/R3K2R w KQkq d6 4 12",
        "8/2k5/8/8/8/8/5K2/8 b - - 73 99",
        "7k/6Q1/6K1/8/8/8/8/8 b - - 100 60"
    };

    for (const std::string_view fen : fens) {
        Position position(fen);
        const std::string serialized = position.fen();
        Position reparsed(serialized);
        assert(reparsed == position);
        assert(reparsed.hash() == reparsed.zobrist());
    }
}

void testPositionBoardAccessorsAndMutators() {
    Attacks::init();

    Position start;
    const Bitboard whiteKingSideExpected = Bitboard(Square::SQUARE_F1) | Bitboard(Square::SQUARE_G1);
    const Bitboard whiteQueenSideExpected = Bitboard(Square::SQUARE_B1) | Bitboard(Square::SQUARE_C1) | Bitboard(Square::SQUARE_D1);
    const Bitboard blackKingSideExpected = Bitboard(Square::SQUARE_F8) | Bitboard(Square::SQUARE_G8);
    const Bitboard blackQueenSideExpected = Bitboard(Square::SQUARE_B8) | Bitboard(Square::SQUARE_C8) | Bitboard(Square::SQUARE_D8);

    assert(start.castlingPath(Color::WHITE, true) == whiteKingSideExpected);
    assert(start.castlingPath(Color::WHITE, false) == whiteQueenSideExpected);
    assert(start.castlingPath(Color::BLACK, true) == blackKingSideExpected);
    assert(start.castlingPath(Color::BLACK, false) == blackQueenSideExpected);

    Position sparse("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    assert(sparse.occupied().count() == 2);
    assert(sparse.friendly(Color::WHITE).count() == 1);
    assert(sparse.enemy(Color::WHITE) == sparse.friendly(Color::BLACK));
    assert(sparse.pieceAt(Square::SQUARE_E1) == Piece::WHITE_KING);
    assert(sparse.pieceAt(Square::SQUARE_E8) == Piece::BLACK_KING);
    assert(sparse.pieces(PieceType::KING).count() == 2);
    assert(sparse.pieces(PieceType::KING, Color::WHITE) == Bitboard(Square::SQUARE_E1));
    assert(sparse.pieces(PieceType::KING, PieceType::PAWN).count() == 2);
    assert(sparse.kingSquare(Color::WHITE) == Square::SQUARE_E1);
    assert(!sparse.nonPawnMaterial(Color::WHITE));

    sparse.placePiece(Piece::WHITE_QUEEN, Square::SQUARE_D1);
    assert(sparse.pieceAt(Square::SQUARE_D1) == Piece::WHITE_QUEEN);
    assert(sparse.pieces(PieceType::QUEEN, Color::WHITE).get(Square(Square::SQUARE_D1).index()));
    assert(sparse.nonPawnMaterial(Color::WHITE));

    sparse.removePiece(Piece::WHITE_QUEEN, Square::SQUARE_D1);
    assert(sparse.pieceAt(Square::SQUARE_D1) == Piece::NONE);
    assert(!sparse.nonPawnMaterial(Color::WHITE));
}

void testPositionMakeUnmakeAndZobrist() {
    Attacks::init();

    Position normal;
    const Move e2e4 = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    assert(!normal.isCapture(e2e4));
    assert(normal.isCheck(e2e4) == CheckType::NONE);
    assertMoveRoundTrip(normal, e2e4);

    Position capture("4k3/8/8/8/8/4p3/4P3/4K3 w - - 0 1");
    const Move e2e3 = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E3);
    assert(capture.isCapture(e2e3));
    assertMoveRoundTrip(capture, e2e3);

    Position enPassantGenerated("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
    const Move pushWithEnPassant = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4);
    const Position beforePush = enPassantGenerated;
    enPassantGenerated.make(pushWithEnPassant);
    assert(enPassantGenerated.enPassantSquare() == Square::SQUARE_E3);
    enPassantGenerated.unmake(pushWithEnPassant);
    assert(enPassantGenerated == beforePush);

    Position enPassant("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    const Move enPassantMove = Move::create<Move::EN_PASSANT>(Square::SQUARE_E5, Square::SQUARE_D6);
    assert(enPassant.isCapture(enPassantMove));
    const Position enPassantBefore = enPassant;
    enPassant.make(enPassantMove);
    assert(enPassant.pieceAt(Square::SQUARE_D6) == Piece::WHITE_PAWN);
    assert(enPassant.pieceAt(Square::SQUARE_D5) == Piece::NONE);
    enPassant.unmake(enPassantMove);
    assert(enPassant == enPassantBefore);

    Position castleKingSide("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
    const Move castleKingSideMove = Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_H1);
    assert(!castleKingSide.isCapture(castleKingSideMove));
    castleKingSide.make(castleKingSideMove);
    assert(castleKingSide.pieceAt(Square::SQUARE_G1) == Piece::WHITE_KING);
    assert(castleKingSide.pieceAt(Square::SQUARE_F1) == Piece::WHITE_ROOK);
    assert(!castleKingSide.castlingRights().get(Color::WHITE));
    castleKingSide.unmake(castleKingSideMove);
    assert(castleKingSide.pieceAt(Square::SQUARE_E1) == Piece::WHITE_KING);
    assert(castleKingSide.pieceAt(Square::SQUARE_H1) == Piece::WHITE_ROOK);

    Position castleQueenSide("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1");
    const Move castleQueenSideMove = Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_A1);
    assertMoveRoundTrip(castleQueenSide, castleQueenSideMove);

    Position promotion("7k/P7/8/8/8/8/8/7K w - - 0 1");
    const Move promoteToQueen = Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_A8, PieceType::QUEEN);
    assertMoveRoundTrip(promotion, promoteToQueen);

    Position promotionCapture("1r5k/P7/8/8/8/8/8/7K w - - 0 1");
    const Move promoteCapture = Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_B8, PieceType::KNIGHT);
    assert(promotionCapture.isCapture(promoteCapture));
    assertMoveRoundTrip(promotionCapture, promoteCapture);

    Position rookMoveRights("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const Move rookMove = Move::create<Move::NORMAL>(Square::SQUARE_A1, Square::SQUARE_A2);
    rookMoveRights.make(rookMove);
    assert(!rookMoveRights.castlingRights().get(Position::CastlingRights::WHITE_QUEENSIDE));
    assert(rookMoveRights.castlingRights().get(Position::CastlingRights::WHITE_KINGSIDE));
    rookMoveRights.unmake(rookMove);

    Position kingMoveRights("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const Move kingMove = Move::create<Move::NORMAL>(Square::SQUARE_E1, Square::SQUARE_E2);
    kingMoveRights.make(kingMove);
    assert(!kingMoveRights.castlingRights().get(Color::WHITE));
    kingMoveRights.unmake(kingMove);

    Position captureRookRights("4k2r/8/8/8/8/8/8/4K2R w Kk - 0 1");
    const Move rookCapture = Move::create<Move::NORMAL>(Square::SQUARE_H1, Square::SQUARE_H8);
    captureRookRights.make(rookCapture);
    assert(!captureRookRights.castlingRights().get(Position::CastlingRights::BLACK_KINGSIDE));
    captureRookRights.unmake(rookCapture);
    assert(captureRookRights.castlingRights().get(Position::CastlingRights::BLACK_KINGSIDE));
}

void testPositionAttackAndCheckHelpers() {
    Attacks::init();

    Position attackersPosition("6k1/8/8/4q3/3P4/2B2N2/8/4R1K1 w - - 0 1");
    const Square target = Square::SQUARE_E5;
    const Bitboard whiteAttackers = attackersPosition.attackers(target, Color::WHITE);

    assert(attackersPosition.attacked(target, Color::WHITE));
    assert(whiteAttackers.get(Square(Square::SQUARE_D4).index()));
    assert(whiteAttackers.get(Square(Square::SQUARE_F3).index()));
    assert(whiteAttackers.get(Square(Square::SQUARE_E1).index()));
    assert(!attackersPosition.attacked(target, Color::BLACK));

    Position checkedPosition("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    assert(checkedPosition.check());

    Position uncheckedPosition;
    assert(!uncheckedPosition.check());

    Position directCheckPosition("k7/8/8/8/8/8/8/1R2K3 w - - 0 1");
    const Move directCheckMove = Move::create<Move::NORMAL>(Square::SQUARE_B1, Square::SQUARE_A1);
    assert(directCheckPosition.isCheck(directCheckMove) == CheckType::DIRECT);

    Position discoveredCheckPosition("4k3/8/8/8/8/8/4B3/4R1K1 w - - 0 1");
    const Move discoveredCheckMove = Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_D3);
    assert(discoveredCheckPosition.isCheck(discoveredCheckMove) == CheckType::DISCOVERED);

    Position noCheckPosition;
    const Move noCheckMove = Move::create<Move::NORMAL>(Square::SQUARE_G1, Square::SQUARE_F3);
    assert(noCheckPosition.isCheck(noCheckMove) == CheckType::NONE);

    Position promotionCheckPosition("7k/6P1/8/8/8/8/8/7K w - - 0 1");
    const Move promotionCheckMove = Move::create<Move::PROMOTION>(Square::SQUARE_G7, Square::SQUARE_G8, PieceType::QUEEN);
    assert(promotionCheckPosition.isCheck(promotionCheckMove) == CheckType::DIRECT);

    Position enPassantCheckPosition("8/8/8/R3Pp1k/8/8/8/K7 w - f6 0 1");
    const Move enPassantCheckMove = Move::create<Move::EN_PASSANT>(Square::SQUARE_E5, Square::SQUARE_F6);
    assert(enPassantCheckPosition.isCheck(enPassantCheckMove) == CheckType::DISCOVERED);

    Position castlingCheckPosition("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    const Move castlingCheckMove = Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_H1);
    assert(castlingCheckPosition.isCheck(castlingCheckMove) == CheckType::DISCOVERED);
}

void testPositionAttackerConsistency() {
    Attacks::init();

    const std::vector<std::string_view> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "6k1/8/8/4q3/3P4/2B2N2/8/4R1K1 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1"
    };

    for (const std::string_view fen : fens) {
        Position position(fen);
        for (int colorIndex = 0; colorIndex < 2; ++colorIndex) {
            const Color color(colorIndex);
            for (int squareIndex = 0; squareIndex < 64; ++squareIndex) {
                const Square square(squareIndex);
                const bool attacked = position.attacked(square, color);
                const bool hasAttackers = static_cast<bool>(position.attackers(square, color));
                assert(attacked == hasAttackers);
            }
        }
    }
}

void testPositionNullMoveAndGameState() {
    Attacks::init();

    Position nullMovePosition;
    const Move knightMove = Move::create<Move::NORMAL>(Square::SQUARE_G1, Square::SQUARE_F3);
    nullMovePosition.make(knightMove);
    const Position beforeNull = nullMovePosition;
    const std::uint64_t expectedNullHash = nullMovePosition.zobristAfter(Move::createNull());
    nullMovePosition.makeNull();
    assert(nullMovePosition.hash() == expectedNullHash);
    assert(nullMovePosition.enPassantSquare() == Square::NONE);
    nullMovePosition.unmakeNull();
    assert(nullMovePosition == beforeNull);

    Position repetitionPosition;
    playSimpleRepetitionCycle(repetitionPosition);
    assert(repetitionPosition.repetition(1));
    playSimpleRepetitionCycle(repetitionPosition);
    assert(repetitionPosition.repetition(2));
    assert(!repetitionPosition.repetition(3));

    Position fiftyMovePosition("8/8/8/8/8/8/8/K6k w - - 100 1");
    assert(fiftyMovePosition.halfMoveDraw());

    MoveList pseudoMoveList;
    pseudoMoveList.add(Move::create<Move::NORMAL>(Square::SQUARE_A1, Square::SQUARE_A2));
    const auto fiftyMoveResult = fiftyMovePosition.halfMoveDrawResult(pseudoMoveList);
    assert(fiftyMoveResult.first == GameResultReason::FIFTY_MOVE_RULE);
    assert(fiftyMoveResult.second == GameResult::DRAW);

    Position fiftyMoveMate("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
    MoveList emptyMoveList;
    const auto fiftyMoveMateResult = fiftyMoveMate.halfMoveDrawResult(emptyMoveList);
    assert(fiftyMoveMateResult.first == GameResultReason::CHECKMATE);
    assert(fiftyMoveMateResult.second == GameResult::LOSS);

    Position kingsOnly("8/8/8/8/8/8/8/K6k w - - 0 1");
    assert(kingsOnly.insufficientMaterial());

    Position kingKnightVsKing("8/8/8/8/8/8/6N1/K6k w - - 0 1");
    assert(kingKnightVsKing.insufficientMaterial());

    Position kingBishopVsKing("8/8/8/8/8/8/6B1/K6k w - - 0 1");
    assert(kingBishopVsKing.insufficientMaterial());

    Position oppositeKingsAndSameColorBishops("7k/8/8/8/8/4b3/8/K1B5 w - - 0 1");
    assert(oppositeKingsAndSameColorBishops.insufficientMaterial());

    Position twoSameColorWhiteBishops("7k/8/8/8/8/4B3/8/K1B5 w - - 0 1");
    assert(twoSameColorWhiteBishops.insufficientMaterial());

    Position rookMaterial("7k/8/8/8/8/8/6R1/K7 w - - 0 1");
    assert(!rookMaterial.insufficientMaterial());

    Position checkmate("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    const auto checkmateResult = checkmate.gameOver(emptyMoveList);
    assert(checkmateResult.first == GameResultReason::CHECKMATE);
    assert(checkmateResult.second == GameResult::LOSS);

    Position stalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    const auto stalemateResult = stalemate.gameOver(emptyMoveList);
    assert(stalemateResult.first == GameResultReason::STALEMATE);
    assert(stalemateResult.second == GameResult::DRAW);

    const auto insufficientResult = kingsOnly.gameOver(emptyMoveList);
    assert(insufficientResult.first == GameResultReason::INSUFFICIENT_MATERIAL);
    assert(insufficientResult.second == GameResult::DRAW);

    Position repetitionGameOver;
    playSimpleRepetitionCycle(repetitionGameOver);
    MoveList nonEmptyMoveList;
    nonEmptyMoveList.add(Move::create<Move::NORMAL>(Square::SQUARE_A2, Square::SQUARE_A3));
    const auto repetitionResult = repetitionGameOver.gameOver(nonEmptyMoveList);
    assert(repetitionResult.first == GameResultReason::THREEFOLD_REPETITION);
    assert(repetitionResult.second == GameResult::DRAW);

    Position ongoing;
    MoveList availableMoves;
    availableMoves.add(Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4));
    const auto ongoingResult = ongoing.gameOver(availableMoves);
    assert(ongoingResult.first == GameResultReason::NONE);
    assert(ongoingResult.second == GameResult::NONE);
}

void testPositionNullMoveWithEnPassant() {
    Attacks::init();

    Position position("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
    assert(position.enPassantSquare() == Square::SQUARE_E3);

    const Position beforeNull = position;
    const std::uint64_t expectedHash = position.zobristAfter(Move::createNull());
    position.makeNull();
    assert(position.enPassantSquare() == Square::NONE);
    assert(position.hash() == expectedHash);
    position.unmakeNull();
    assert(position == beforeNull);
}

void testPositionMoveSequenceRoundTrip() {
    Attacks::init();

    Position position;
    const std::vector<Move> sequence = {
        Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4),
        Move::create<Move::NORMAL>(Square::SQUARE_E7, Square::SQUARE_E5),
        Move::create<Move::NORMAL>(Square::SQUARE_G1, Square::SQUARE_F3),
        Move::create<Move::NORMAL>(Square::SQUARE_B8, Square::SQUARE_C6),
        Move::create<Move::NORMAL>(Square::SQUARE_F1, Square::SQUARE_B5),
        Move::create<Move::NORMAL>(Square::SQUARE_A7, Square::SQUARE_A6),
        Move::create<Move::NORMAL>(Square::SQUARE_B5, Square::SQUARE_A4),
        Move::create<Move::NORMAL>(Square::SQUARE_G8, Square::SQUARE_F6),
        Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_H1)
    };

    std::vector<Position> snapshots;
    snapshots.reserve(sequence.size() + 1);
    snapshots.push_back(position);

    for (const Move move : sequence) {
        const std::uint64_t predictedHash = position.zobristAfter(move);
        position.make(move);
        assert(position.hash() == predictedHash);
        assert(position.hash() == position.zobrist());
        snapshots.push_back(position);
    }

    for (std::size_t index = sequence.size(); index > 0; --index) {
        position.unmake(sequence[index - 1]);
        assert(position == snapshots[index - 1]);
        assert(position.hash() == position.zobrist());
    }
}

void testPositionCastlingBlackSides() {
    Attacks::init();

    Position blackKingSide("4k2r/8/8/8/8/8/8/4K3 b k - 0 1");
    const Move blackKingSideCastling = Move::create<Move::CASTLING>(Square::SQUARE_E8, Square::SQUARE_H8);
    const Position blackKingSideBefore = blackKingSide;
    blackKingSide.make(blackKingSideCastling);
    assert(blackKingSide.pieceAt(Square::SQUARE_G8) == Piece::BLACK_KING);
    assert(blackKingSide.pieceAt(Square::SQUARE_F8) == Piece::BLACK_ROOK);
    blackKingSide.unmake(blackKingSideCastling);
    assert(blackKingSide == blackKingSideBefore);

    Position blackQueenSide("r3k3/8/8/8/8/8/8/4K3 b q - 0 1");
    const Move blackQueenSideCastling = Move::create<Move::CASTLING>(Square::SQUARE_E8, Square::SQUARE_A8);
    const Position blackQueenSideBefore = blackQueenSide;
    blackQueenSide.make(blackQueenSideCastling);
    assert(blackQueenSide.pieceAt(Square::SQUARE_C8) == Piece::BLACK_KING);
    assert(blackQueenSide.pieceAt(Square::SQUARE_D8) == Piece::BLACK_ROOK);
    blackQueenSide.unmake(blackQueenSideCastling);
    assert(blackQueenSide == blackQueenSideBefore);
}

void testPositionPromotionsAllPiecesAndColors() {
    Attacks::init();

    const std::vector<PieceType> promotionPieces = {
        PieceType::KNIGHT,
        PieceType::BISHOP,
        PieceType::ROOK,
        PieceType::QUEEN
    };

    for (const PieceType promotionPiece : promotionPieces) {
        Position whitePromotion("7k/P7/8/8/8/8/8/7K w - - 0 1");
        const Move whitePromotionMove = Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_A8, promotionPiece);
        assertMoveRoundTrip(whitePromotion, whitePromotionMove);
        whitePromotion.make(whitePromotionMove);
        assert(whitePromotion.pieceAt(Square::SQUARE_A8) == Piece(promotionPiece, Color::WHITE));
        whitePromotion.unmake(whitePromotionMove);

        Position whitePromotionCapture("1r5k/P7/8/8/8/8/8/7K w - - 0 1");
        const Move whitePromotionCaptureMove = Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_B8, promotionPiece);
        assertMoveRoundTrip(whitePromotionCapture, whitePromotionCaptureMove);

        Position blackPromotion("7k/8/8/8/8/8/p7/7K b - - 0 1");
        const Move blackPromotionMove = Move::create<Move::PROMOTION>(Square::SQUARE_A2, Square::SQUARE_A1, promotionPiece);
        assertMoveRoundTrip(blackPromotion, blackPromotionMove);
        blackPromotion.make(blackPromotionMove);
        assert(blackPromotion.pieceAt(Square::SQUARE_A1) == Piece(promotionPiece, Color::BLACK));
        blackPromotion.unmake(blackPromotionMove);

        Position blackPromotionCapture("7k/8/8/8/8/8/1p6/R6K b - - 0 1");
        const Move blackPromotionCaptureMove = Move::create<Move::PROMOTION>(Square::SQUARE_B2, Square::SQUARE_A1, promotionPiece);
        assertMoveRoundTrip(blackPromotionCapture, blackPromotionCaptureMove);
    }
}

void testPositionBlackEnPassant() {
    Attacks::init();

    Position position("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
    const Move blackEnPassantMove = Move::create<Move::EN_PASSANT>(Square::SQUARE_D4, Square::SQUARE_E3);
    const Position before = position;
    position.make(blackEnPassantMove);
    assert(position.pieceAt(Square::SQUARE_E3) == Piece::BLACK_PAWN);
    assert(position.pieceAt(Square::SQUARE_E4) == Piece::NONE);
    position.unmake(blackEnPassantMove);
    assert(position == before);
}

void testPositionSpecialMoveSequenceStress() {
    Attacks::init();

    const std::vector<Move> castlingSequence = {
        Move::create<Move::CASTLING>(Square::SQUARE_E1, Square::SQUARE_H1),
        Move::create<Move::CASTLING>(Square::SQUARE_E8, Square::SQUARE_A8)
    };
    assertSequenceRoundTrip(Position("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"), castlingSequence);

    const std::vector<Move> enPassantSequence = {
        Move::create<Move::NORMAL>(Square::SQUARE_E2, Square::SQUARE_E4),
        Move::create<Move::EN_PASSANT>(Square::SQUARE_D4, Square::SQUARE_E3),
        Move::create<Move::NORMAL>(Square::SQUARE_E1, Square::SQUARE_F1),
        Move::create<Move::NORMAL>(Square::SQUARE_E8, Square::SQUARE_F8)
    };
    assertSequenceRoundTrip(Position("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1"), enPassantSequence);

    const std::vector<Move> dualPromotionSequence = {
        Move::create<Move::PROMOTION>(Square::SQUARE_A7, Square::SQUARE_A8, PieceType::ROOK),
        Move::create<Move::PROMOTION>(Square::SQUARE_H2, Square::SQUARE_H1, PieceType::KNIGHT)
    };
    assertSequenceRoundTrip(Position("4k3/P7/8/8/8/8/7p/4K3 w - - 0 1"), dualPromotionSequence);
}

int main() {
    testColor();
    testColorExtras();
    testPieceType();
    testPieceTypeExtras();
    testPiece();
    testPieceExtras();
    testFileAndRank();
    testFileAndRankExtras();
    testDirection();
    testDirectionExtras();
    testSquare();
    testSquareExtras();
    testBitboard();
    testBitboardExtras();
    testMove();
    testMoveExtras();
    testMoveList();
    testMoveListExtras();
    testAttacksAndMagic();
    testAttacksBetween();
    testAttacksBetweenSymmetry();
    testZobrist();
    testPositionCastlingRights();
    testPositionFenAndAccessors();
    testPositionFenParsingEdgeCases();
    testPositionFenRoundTrip();
    testPositionBoardAccessorsAndMutators();
    testPositionMakeUnmakeAndZobrist();
    testPositionAttackAndCheckHelpers();
    testPositionAttackerConsistency();
    testPositionNullMoveAndGameState();
    testPositionNullMoveWithEnPassant();
    testPositionMoveSequenceRoundTrip();
    testPositionCastlingBlackSides();
    testPositionPromotionsAllPiecesAndColors();
    testPositionBlackEnPassant();
    testPositionSpecialMoveSequenceStress();

    cout << "Success!" << endl;

    return 0;
}
