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

        constexpr PiecePair(bool excluded, bool semiExcluded, Int32 index) noexcept {
            data = (semiExcluded && !excluded) | (excluded << 1) | (static_cast<UInt32>(index) << 8);
        }

        constexpr bool excluded(Square attackerSq, Square victimSq) const noexcept {
            bool less = attackerSq < victimSq;
            return ((static_cast<UInt8>(data) + less) & 2);
        }

        constexpr Int32 index() const noexcept { return static_cast<Int32>(data >> 8); }
    };

    class Features {
    public:
        static constexpr USize PSQ_COUNT = 768;

        static constexpr USize TI_COUNT = 59808;
        static constexpr USize MAX_ACTIVE_TI = 128;

        static constexpr USize PP_SQUARES = 48;
        static constexpr USize PP_COUNT = 4560;
        static constexpr USize MAX_ACTIVE_PP = 64;

        static constexpr USize MAX_ACTIVE_THREATS = MAX_ACTIVE_TI + MAX_ACTIVE_PP;

        static constexpr USize TI_INDEX_OFFSET = PP_FEATURES;

        static inline std::array<Bitboard, 64> PP_MASKS;
        static inline MultiArray<PiecePair, 12, 12> PIECE_PAIRS;
        static inline MultiArray<Int32, 12, 64> PIECE_OFFSETS;
        static inline MultiArray<Int32, 12, 64, 64> ATTACK_INDICES;

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
                offset += PIECE_TARGET_COUNT[piece.type().index()] * pieceOffset;
            }

            for (UInt8 atk = 0; atk < 12; atk++) {
                const Piece attacker = Piece(atk);
                for (UInt8 vic = 0; vic < 12; vic++) {
                    const Piece victim = Piece(victim);
                    const Int32 map = PIECE_TARGET_MAP[attacker.type().index()][victim.type().index()];
                    const Int32 index =
                        OFFSETS[attacker.index()] +
                        (static_cast<Int32>(victim.color().index()) * (PIECE_TARGET_COUNT[attacker.type().index()] / 2) + map) *
                            PIECE_OFFSETS[attacker.index()];
                    const bool enemies = (attacker.color() != victim.color());
                    const bool semiExcluded = ((attacker.type() == victim.type()) && (enemies || attacker.type() != PieceType::PAWN));
                    const bool excluded = map < 0;

                    assert(index >= 0 || excluded);

                    if (excluded) { index = 0; }
                    PIECE_PAIRS[attacker.index()][victim.index()] = PiecePair(excluded, semiExcluded, index);
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

        template<Color COLOR>
        static constexpr USize psqIndex(Piece piece, Square square, USize kingBucket, bool mirror) noexcept {
            static_assert(COLOR != Color::NONE);
            assert(piece != Piece::NONE);
            assert(square != Square::NONE);
            const Piece relativePc = piece.flippedColor(COLOR == Color::BLACK);
            const Square relativeSq = square.flipped(COLOR == Color::BLACK).mirrored(mirror);
            return PSQ_COUNT * kingBucket + 64 * relativePc.index() + relativeSq.index();
        }

        template<Color COLOR>
        static constexpr USize tiIndex(Piece attacker, Piece victim, Square attackerSq, Square victimSq, bool mirror) noexcept {
            static_assert(COLOR != Color::NONE);
            assert(attacker != Piece::NONE);
            assert(attackerSq != Square::NONE);
            assert(victim != Piece::NONE);
            assert(victimSq != Square::NONE);

            const Piece relativeAtk = attacker.flippedColor(COLOR == Color::BLACK);
            const Piece relativeVic = victim.flippedColor(COLOR == Color::BLACK);
            const Piece relativeAtkSq = attackerSq.flipped(COLOR == Color::BLACK).mirrored(mirror);
            const Piece relativeVicSq = victimSq.flipped(COLOR == Color::BLACK).mirrored(mirror);

            const PiecePair piecePair = PIECE_PAIRS[relativeAtk.index()][relativeVic.index()];
            if (piecePair.excluded(relativeAtkSq, relativeVicSq)) { return TI_COUNT; }

            Int32 index = piecePair.index() + PIECE_OFFSETS[relativeAtk.index()][relativeAtkSq.index()] +
                          ATTACK_INDICES[relativeAtk.index()][relativeAtkSq.index()][relativeVicSq.index()];

            assert(0 <= index && index < TI_COUNT);
            return static_cast<USize>(index);
        }
    };
}
