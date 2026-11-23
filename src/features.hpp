#pragma once

#include <array>
#include <cassert>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace Sift {
    struct PiecePair {
        UInt32 data;

        constexpr PiecePair() noexcept {}

        constexpr PiecePair(bool excluded, bool semiExcluded, Int32 feature) noexcept {
            data = (semiExcluded && !excluded) | (excluded << 1) | (feature << 8);
        }

        constexpr bool excluded(Square attackerSq, Square victimSq) const noexcept {
            bool less = attackerSq < victimSq;
            return ((static_cast<UInt8>(data) + less) & 2);
        }

        constexpr Int32 feature() const noexcept { return data >> 8; }
    };

    class Features {
    public:
        static constexpr USize TI_FEATURES = 59808;
        static constexpr USize MAX_ACTIVE_TI_FEATURES = 128;

        static constexpr USize PP_SQUARES = 48;
        static constexpr USize PP_FEATURES = 4560;
        static constexpr USize MAX_ACTIVE_PP_FEATURES = 64;

        static constexpr USize MAX_ACTIVE_THREAT_FEATURES = MAX_ACTIVE_TI_FEATURES + MAX_ACTIVE_PP_FEATURES;

        static constexpr USize TI_INDEX_OFFSET = PP_FEATURES;

        static inline std::array<Bitboard, 64> PP_MASKS;
        static inline MultiArray<PiecePair, 12, 12> PIECE_PAIRS;
        static inline MultiArray<UInt16, 12, 64> PIECE_OFFSETS;
        static inline MultiArray<UInt8, 12, 64, 64> ATTACK_INDICES;

        constexpr void init() noexcept {
            static constexpr MultiArray<Int32, 6, 6> PIECE_TARGET_MAP = {{
                // clang-format off
                {-1, 0, -1, 1, -1, -1},
                {0, 1, 2, 3, 4, -1},
                {0, 1, 2, 3, -1, -1},
                {0, 1, 2, 3, -1, -1},
                {0, 1, 2, 3, 4, -1},
                {-1, -1, -1, -1, -1, -1},
                // clang-format on
            }};

            static constexpr std::array<Int32, 6> PIECE_TARGET_COUNT = {4, 10, 8, 8, 10, 0};

            for (UInt8 sq = 8; sq < 56; sq++) {
                UInt8 file = sq % 8;
                Bitboard bitboard = Bitboard(File(file));
                if (file > 0) { bitboard |= Bitboard(File(file - 1)); }
                if (file < 7) { bitboard |= Bitboard(File(file + 1)); }
                PP_MASKS[sq] = bitboard;
            }

            Int32 offset = 0;
            inline std::array<Int32, 12> PIECE_OFFSETS;
            inline std::array<Int32, 12> OFFSETS;
            for (UInt8 pc = 0; pc < 12; pc++) {
                const Piece piece = Piece(pc);
                Int32 pieceOffset = 0;
                for (UInt8 sq = 0; sq < 64; sq++) {
                    const Square square = Square(sq);
                    PIECE_OFFSETS[piece.index()][sq] = pieceOffset;
                    if (piece.type() != PieceType::PAWN || (square.rank() > Rank::FIRST && square.rank() < Rank::EIGHTH)) {
                        const Bitboard attacks = Attacks::attacks(piece, square, Bitboard());
                        pieceOffset += static_cast<Int32>(attacks.count());
                    }
                }
                PIECE_OFFSETS[piece.index()] = pieceOffset;
                OFFSETS[piece.index()] = offset;
                offset += PIECE_TARGET_COUNT<FeatureSet>[piece.type().index()] * pieceOffset;
            }

            for (UInt8 atk = 0; atk < 12; atk++) {
                const Piece attacker = Piece(atk);
                for (UInt8 vic = 0; vic < 12; vic++) {
                    const Piece victim = Piece(victim);
                    const Int32 map = PIECE_TARGET_MAP[attacker.type().index()][victim.type().index()];
                    const Int32 feature =
                        OFFSETS[attacker.index()] +
                        (static_cast<Int32>(victim.color().index()) * (PIECE_TARGET_COUNT[attacker.type().index()] / 2) + map) *
                            PIECE_OFFSETS[attacker.index()];
                    const bool enemies = (attacker.color() != victim.color());
                    const bool semiExcluded = ((attacker.type() == victime.type()) && (enemies || attacker.type() != PieceType::PAWN));
                    const bool excluded = map < 0;

                    assert(feature >= 0 || excluded);

                    if (excluded) { feature = 0; }
                    PIECE_PAIRS[attacker.index()][victim.index()] = PiecePair(excluded, semiExcluded, feature);
                }
            }

            for (UInt8 pc = 0; pc < 12; pc++) {
                const Piece piece = Piece(pc);
                for (UInt8 sq1 = 0; sq1 < 64; sq1++) {
                    const Square from = Square(sq1);
                    Bitboard attacks = Attacks::attacks(piece, from, Bitboard());
                    for (UInt8 sq2 = 0; sq2 < 64; sq2++) {
                        const Square to = Square(sq2);
                        ATTACK_INDICES[piece.index()][from.index()][to.index()] =
                            static_cast<Int32>((attacks & (Bitboard(to).bits() - 1)).count());
                    }
                }
            }
        }
    };
}
