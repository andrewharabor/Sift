#pragma once

#include <array>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "move.hpp"
#include "types.hpp"
#include "zobrist.hpp"


namespace Clownfish {

class CuckooTable {
public:
    static void init() noexcept {
        keys_.fill(0);
        moves_.fill(Move::NULL_MOVE);

        [[maybe_unused]] UInt32 count = 0;
        for (PieceType pieceType : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            for (Color color : {Color::WHITE, Color::BLACK}) {
                for (UInt8 i = 0; i < 63; i++) {
                    for (UInt8 j = i + 1; j < 64; j++) {
                        Square from = Square(i);
                        Square to = Square(j);
                        Bitboard attacks = Bitboard();
                        if (pieceType == PieceType::KNIGHT) {
                            attacks = Attacks::knight(from);
                        } else if (pieceType == PieceType::BISHOP) {
                            attacks = Attacks::bishop(from, Bitboard());
                        } else if (pieceType == PieceType::ROOK) {
                            attacks = Attacks::rook(from, Bitboard());
                        } else if (pieceType == PieceType::QUEEN) {
                            attacks = Attacks::queen(from, Bitboard());
                        } else if (pieceType == PieceType::KING) {
                            attacks = Attacks::king(from);
                        }

                        if (!(attacks & Bitboard(to))) {
                            continue;
                        }

                        Move move = Move(from, to, MoveType::NORMAL);
                        UInt64 hash = Zobrist::piece(Piece(pieceType, color), from) ^ Zobrist::piece(Piece(pieceType, color), to) ^ Zobrist::sideToMove();

                        UInt64 index = hash1(hash);
                        while (true) {
                            std::swap(keys_[index], hash);
                            std::swap(moves_[index], move);
                            if (move == Move::NULL_MOVE) {
                                break;
                            }

                            if (index == hash1(hash)) {
                                index = hash2(hash);
                            } else {
                                index = hash1(hash);
                            }
                        }
                        count++;
                    }
                }
            }
        }

        assert(count == 3668);
    }

    static constexpr Move probe(UInt64 key) noexcept {
        UInt64 index = hash1(key);
        if (keys_[index] == key) {
            return moves_[index];
        }

        index = hash2(key);
        if (keys_[index] == key) {
            return moves_[index];
        }

        return Move::NULL_MOVE;
    }

private:
    static constexpr USize TABLE_SIZE = 8192;
    static inline std::array<UInt64, TABLE_SIZE> keys_;
    static inline std::array<Move, TABLE_SIZE> moves_;

    static constexpr UInt64 hash1(UInt64 value) noexcept { return value % TABLE_SIZE; }
    static constexpr UInt64 hash2(UInt64 value) noexcept { return (value >> 16) % TABLE_SIZE; }
};

}
