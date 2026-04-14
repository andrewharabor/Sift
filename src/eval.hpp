#pragma once

#include <array>

#include "coords.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Clownfish {

// FIXME: replace with Eval
// https://github.com/andrewharabor/simPLY_chess/blob/main/src/simPLY_chess.py
class Eval {

public:
    static Int32 evaluate(const Position &position) noexcept {
        static constexpr Int32 colorMultipliers[2] = {1, -1};

        Int32 scores[2] = {0, 0};
        Square kingSquares[2] = {position.kingSquare(Color::WHITE), position.kingSquare(Color::BLACK)};
        for (UInt8 i = 0; i < 64; i++) {
            const Square square = Square(i);
            const Piece piece = position.pieceAt(square);
            if (piece != Piece::NONE) {
                const UInt8 color = piece.color();
                const UInt8 pieceType = piece.type();
                UInt8 squareIndex = square;
                if (piece.color() == Color::WHITE) {
                    squareIndex ^= 56;
                }
                for (UInt8 phase = 0; phase < 2; phase++) {
                    scores[phase] += PIECE_VALUES[phase][pieceType] * colorMultipliers[color];
                    scores[phase] += PIECE_TABLES[phase][pieceType][squareIndex] * colorMultipliers[color];
                    scores[phase] += floorDiv(PIECE_TROPISM_VALUES[phase][pieceType], std::max(1, Square::manhattanDistance(square, kingSquares[1 - color]))) * colorMultipliers[color];
                }
            }
        }
        const Int32 mopUp = floorDiv(MOP_UP_VALUE * (14 - Square::manhattanDistance(kingSquares[0], kingSquares[1])), 14);
        if (scores[1] > 0) {
            scores[1] += mopUp;
        } else if (scores[1] < 0) {
            scores[1] -= mopUp;
        }
        return interpolate(scores[0], scores[1], phase(position)) * (position.sideToMove() == Color::WHITE ? 1 : -1);
    }

private:
    static constexpr Int32 PIECE_VALUES[2][6] = {
            {100, 411, 445, 582, 1250, 0},
            {115, 343, 362, 624, 1141, 0}
    };

    static constexpr Int32 PIECE_TABLES[2][6][64] = {
        {
            { 0, 0, 0, 0, 0, 0, 0, 0, 120, 163, 74, 116, 83, 154, 41, -13, -7, 9, 32, 38, 79, 68, 30, -24, -17, 16, 7, 26, 28, 15, 21, -28, -33, -2, -6, 20, 26, 7, 12, -30, -32, -5, -5, -12, 4, 4, 40, -15, -43, -1, -24, -33, -23, 29, 46, -27, 0, 0, 0, 0, 0, 0, 0, 0 },
            { -204, -109, -41, -60, 74, -118, -18, -130, -89, -50, 88, 44, 28, 76, 9, -21, -57, 73, 45, 79, 102, 157, 89, 54, -11, 21, 23, 65, 45, 84, 22, 27, -16, 5, 20, 16, 34, 23, 26, -10, -28, -11, 15, 12, 23, 21, 30, -20, -35, -65, -15, -4, -1, 22, -17, -23, -128, -26, -71, -40, -21, -34, -23, -28 },
            { -35, 5, -100, -45, -30, -51, 9, -10, -32, 20, -22, -16, 37, 72, 22, -57, -20, 45, 52, 49, 43, 61, 45, -2, -5, 6, 23, 61, 45, 45, 9, -2, -7, 16, 16, 32, 41, 15, 12, 5, 0, 18, 18, 18, 17, 33, 22, 12, 5, 18, 20, 0, 9, 26, 40, 1, -40, -4, -17, -26, -16, -15, -48, -26 },
            { 39, 51, 39, 62, 77, 11, 38, 52, 33, 39, 71, 76, 98, 82, 32, 54, -6, 23, 32, 44, 21, 55, 74, 20, -29, -13, 9, 32, 29, 43, -10, -24, -44, -32, -15, -1, 11, -9, 7, -28, -55, -30, -20, -21, 4, 0, -6, -40, -54, -20, -24, -11, -1, 13, -7, -87, -23, -16, 1, 21, 20, 9, -45, -32 },
            { -34, 0, 35, 15, 72, 54, 52, 55, -29, -48, -6, 1, -20, 70, 34, 66, -16, -21, 9, 10, 35, 68, 57, 70, -33, -33, -20, -20, -1, 21, -2, 1, -11, -32, -11, -12, -2, -5, 4, -4, -17, 2, -13, -2, -6, 2, 17, 6, -43, -10, 13, 2, 10, 18, -4, 1, -1, -22, -11, 12, -18, -30, -38, -61 },
            { -79, 28, 20, -18, -68, -41, 2, 16, 35, -1, -24, -9, -10, -5, -46, -35, -11, 29, 2, -20, -24, 7, 27, -27, -21, -24, -15, -33, -37, -30, -17, -44, -60, -1, -33, -48, -56, -54, -40, -62, -17, -17, -27, -56, -54, -37, -18, -33, 1, 9, -10, -78, -52, -20, 11, 10, -18, 44, 15, -66, 10, -34, 29, 17 }
        },
        {
            { 0, 0, 0, 0, 0, 0, 0, 0, 217, 211, 193, 163, 179, 161, 201, 228, 115, 122, 104, 82, 68, 65, 100, 102, 39, 29, 16, 6, -2, 5, 21, 21, 16, 11, -4, -9, -9, -10, 4, -1, 5, 9, -7, 1, 0, -6, -1, -10, 16, 10, 10, 12, 16, 0, 2, -9, 0, 0, 0, 0, 0, 0, 0, 0 },
            { -71, -46, -16, -34, -38, -33, -77, -121, -30, -10, -30, -2, -11, -30, -29, -63, -29, -24, 12, 11, -1, -11, -23, -50, -21, 4, 27, 27, 27, 13, 10, -22, -22, -7, 20, 30, 20, 21, 5, -22, -28, -4, -1, 18, 12, -4, -24, -27, -51, -24, -12, -6, -2, -24, -28, -54, -35, -62, -28, -18, -27, -22, -61, -78 },
            { -17, -26, -13, -10, -9, -11, -21, -29, -10, -5, 9, -15, -4, -16, -5, -17, 2, -10, 0, -1, -2, 7, 0, 5, -4, 11, 15, 11, 17, 12, 4, 2, -7, 4, 16, 23, 9, 12, -4, -11, -15, -4, 10, 12, 16, 4, -9, -18, -17, -22, -9, -1, 5, -11, -18, -33, -28, -11, -28, -6, -11, -20, -6, -21 },
            { 16, 12, 22, 18, 15, 15, 10, 6, 13, 16, 16, 13, -4, 4, 10, 4, 9, 9, 9, 6, 5, -4, -6, -4, 5, 4, 16, 1, 2, 1, -1, 2, 4, 6, 10, 5, -6, -7, -10, -13, -5, 0, -6, -1, -9, -15, -10, -20, -7, -7, 0, 2, -11, -11, -13, -4, -11, 2, 4, -1, -6, -16, 5, -24 },
            { -11, 27, 27, 33, 33, 23, 12, 24, -21, 24, 39, 50, 71, 30, 37, 0, -24, 7, 11, 60, 57, 43, 23, 11, 4, 27, 29, 55, 70, 49, 70, 44, -22, 34, 23, 57, 38, 41, 48, 28, -20, -33, 18, 7, 11, 21, 12, 6, -27, -28, -37, -20, -20, -28, -44, -39, -40, -34, -27, -52, -6, -39, -24, -50 },
            { -90, -43, -22, -22, -13, 18, 5, -21, -15, 21, 17, 21, 21, 46, 28, 13, 12, 21, 28, 18, 24, 55, 54, 16, -10, 27, 29, 33, 32, 40, 32, 4, -22, -5, 26, 29, 33, 28, 11, -13, -23, -4, 13, 26, 28, 20, 9, -11, -33, -13, 5, 16, 17, 5, -6, -21, -65, -41, -26, -13, -34, -17, -29, -52 }
        }
    };

    static constexpr Int32 PIECE_TROPISM_VALUES[2][6] = {
        {20, 82, 89, 116, 250, 0},
        {38, 114, 120, 208, 380, 0}
    };

    static constexpr Int32 MOP_UP_VALUE = 230;

    static constexpr Int32 PIECE_PHASES[6] = {0, 1, 1, 2, 4, 0};
    static constexpr Int32 TOTAL_PHASE = 24;

    static Int32 phase(const Position &position) noexcept {
        Int32 phase = TOTAL_PHASE;
        for (const PieceType pieceType : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN}) {
            phase -= position.pieces(pieceType).count() * PIECE_PHASES[static_cast<UInt8>(pieceType)];
        }
        return floorDiv(phase * 256 + TOTAL_PHASE / 2, TOTAL_PHASE);
    }

    static Int32 interpolate(Int32 midgameScore, Int32 endgameScore, Int32 phase) noexcept {
        return floorDiv((midgameScore * (256 - phase)) + (endgameScore * phase), 256);
    }

    static constexpr Int32 floorDiv(Int32 a, Int32 b) noexcept {
        if ((a < 0) != (b < 0) && a % b != 0) {
            return a / b - 1;
        } else {
            return a / b;
        }
    }
};

}
