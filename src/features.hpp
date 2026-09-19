#pragma once

#include <array>
#include <limits>
#include <string>
#include <utility>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "types.hpp"

namespace Sift {
    struct PSQFeature {
        Piece piece;
        Square square;

        template<typename FeatureSet>
        constexpr USize index(Color color, Square kingSquare) const noexcept {
            assert(piece != Piece::NONE);
            assert(square != Square::NONE);
            assert(color != Color::NONE);
            assert(kingSquare != Square::NONE);

            Square sq = (color == Color::WHITE) ? square : square.flipped();
            sq = FeatureSet::mirror(sq, kingSquare);
            Piece pc = (color == Color::WHITE) ? piece : Piece(piece.type(), ~piece.color());
            if constexpr (FeatureSet::MERGED_KINGS) {
                if (pc.type() == PieceType::KING) { pc = Piece(PieceType::KING, Color::WHITE); }
            }

            const USize bucketOffset = FeatureSet::bucket(color, kingSquare) * FeatureSet::PSQ_FEATURES;

            return bucketOffset + 64 * pc.index() + sq.index();
        }

        operator std::string() const noexcept { return std::string(piece) + "@" + std::string(square); }
    };

    struct TIFeature {
        Piece attacker;
        Square attackerSq;
        Piece victim;
        Square victimSq;

        template<typename FeatureSet>
        constexpr Int64 index(Color color, Square kingSquare) const noexcept {
            assert(attacker != Piece::NONE);
            assert(attackerSq != Square::NONE);
            assert(victim != Piece::NONE);
            assert(victimSq != Square::NONE);
            assert(color != Color::NONE);
            assert(kingSquare != Square::NONE);

            Piece atk = (color == Color::WHITE) ? attacker : Piece(attacker.type(), ~attacker.color());
            Square atkSq = (color == Color::WHITE) ? attackerSq : attackerSq.flipped();
            atkSq = FeatureSet::mirror(atkSq, kingSquare);
            Piece vic = (color == Color::WHITE) ? victim : Piece(victim.type(), ~victim.color());
            Square vicSq = (color == Color::WHITE) ? victimSq : victimSq.flipped();
            vicSq = FeatureSet::mirror(vicSq, kingSquare);

            const bool forwards = atkSq.index() < vicSq.index();

            const Int64 attackIdx = ATTACK_INDICES<FeatureSet>[atk.index()][vic.index()][forwards];
            const Int64 offset = OFFSETS<FeatureSet>.offsets[atk.index()][atkSq.index()];
            const Int64 pieceIdx = PIECE_INDICES[atk.index()][atkSq.index()][vicSq.index()];

            return FeatureSet::THREAT_OFFSET + attackIdx + offset + pieceIdx;
        }

        operator std::string() const noexcept {
            return std::string(attacker) + "@" + std::string(attackerSq) + "->" + std::string(victim) + "@" + std::string(victimSq);
        }

    private:
        static constexpr MultiArray<Int64, 6, 6> PIECE_TARGET_MAP_PAWNS = {{
            {0, 1, -1, 2, -1, -1},
            {0, 1, 2, 3, 4, -1},
            {0, 1, 2, 3, -1, -1},
            {0, 1, 2, 3, -1, -1},
            {0, 1, 2, 3, 4, -1},
            {-1, -1, -1, -1, -1, -1},
        }};

        static constexpr MultiArray<Int64, 6, 6> PIECE_TARGET_MAP_NO_PAWNS = {{
            {-1, 0, -1, 1, -1, -1},
            {0, 1, 2, 3, 4, -1},
            {0, 1, 2, 3, -1, -1},
            {0, 1, 2, 3, -1, -1},
            {0, 1, 2, 3, 4, -1},
            {-1, -1, -1, -1, -1, -1},
        }};

        template<typename FeatureSet>
        static constexpr MultiArray<Int64, 6, 6> PIECE_TARGET_MAP =
            (FeatureSet::PAWN_PAWN_INPUTS) ? PIECE_TARGET_MAP_NO_PAWNS : PIECE_TARGET_MAP_PAWNS;

        template<typename FeatureSet>
        static constexpr std::array<Int64, 6> PIECE_TARGET_COUNT = [] {
            std::array<Int64, 6> counts = {};
            for (USize atk = 0; atk < 6; atk++) {
                Int64 count = 0;
                for (USize vic = 0; vic < 6; vic++) {
                    if (PIECE_TARGET_MAP<FeatureSet>[atk][vic] >= 0) { count++; }
                }
                counts[atk] = 2 * count;
            }
            return counts;
        }();

        static inline MultiArray<Int64, 64, 64> pieceIndices(Piece piece) noexcept {
            Attacks::init();
            MultiArray<Int64, 64, 64> indices = {};
            for (UInt8 i = 0; i < 64; i++) {
                const Square from = Square(i);
                Bitboard attacks = Attacks::attacks(piece, from, Bitboard());
                for (UInt8 j = 0; j < 64; j++) {
                    const Square to = Square(j);
                    const Bitboard mask = attacks & (Bitboard(to).bits() - 1);
                    indices[i][j] = static_cast<Int64>(mask.count());
                }
            }
            return indices;
        }

        static inline MultiArray<Int64, 12, 64, 64> PIECE_INDICES = [] {
            MultiArray<Int64, 12, 64, 64> indices = {};
            indices[static_cast<USize>(Piece::WHITE_PAWN)] = pieceIndices(Piece::WHITE_PAWN);
            indices[static_cast<USize>(Piece::BLACK_PAWN)] = pieceIndices(Piece::BLACK_PAWN);
            for (const PieceType pieceType : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
                const MultiArray<Int64, 64, 64> idx = pieceIndices(Piece(pieceType, Color::BLACK));
                indices[Piece(pieceType, Color::WHITE).index()] = idx;
                indices[Piece(pieceType, Color::BLACK).index()] = idx;
            }
            return indices;
        }();

        template<typename FeatureSet>
        static inline auto OFFSETS = [] {
            Attacks::init();

            struct {
                std::array<std::pair<Int64, Int64>, 12> indices = {};
                MultiArray<Int64, 12, 64> offsets = {};
            } offsets;

            Int64 offset = 0;
            for (UInt8 pc = 0; pc < 12; pc++) {
                const Piece piece = Piece(pc);
                Int64 pieceOffset = 0;
                for (UInt8 sq = 0; sq < 64; sq++) {
                    const Square square = Square(sq);
                    offsets.offsets[piece.index()][sq] = pieceOffset;
                    if (piece.type() != PieceType::PAWN || (square.rank() > Rank::FIRST && square.rank() < Rank::EIGHTH)) {
                        const Bitboard attacks = Attacks::attacks(Piece(piece.type(), ~piece.color()), square, Bitboard());
                        pieceOffset += static_cast<Int64>(attacks.count());
                    }
                }
                offsets.indices[piece.index()] = {pieceOffset, offset};
                offset += PIECE_TARGET_COUNT<FeatureSet>[piece.type().index()] * pieceOffset;
            }
            return offsets;
        }();

        template<typename FeatureSet>
        static inline MultiArray<Int64, 12, 12, 2> ATTACK_INDICES = [] {
            MultiArray<Int64, 12, 12, 2> indices = {};
            for (UInt8 i = 0; i < 12; i++) {
                const Piece attacker = Piece(i);
                for (UInt8 j = 0; j < 12; j++) {
                    const Piece victim = Piece(j);
                    const bool enemies = attacker.color() != victim.color();
                    const Int64 map = PIECE_TARGET_MAP<FeatureSet>[attacker.type().index()][victim.type().index()];
                    const bool semiExcluded = ((attacker.type() == victim.type()) && (enemies || attacker.type() != PieceType::PAWN));
                    const bool excluded = map < 0;
                    const auto [pieceOffset, offset] = OFFSETS<FeatureSet>.indices[i];
                    const Int64 featureIdx =
                        offset +
                        (static_cast<Int64>(victim.color()) * (PIECE_TARGET_COUNT<FeatureSet>[attacker.type().index()] / 2) + map) *
                            pieceOffset;
                    indices[i][j][0] = (excluded) ? std::numeric_limits<Int64>::min() : featureIdx;
                    indices[i][j][1] = (excluded || semiExcluded) ? std::numeric_limits<Int64>::min() : featureIdx;
                }
            }
            return indices;
        }();
    };

    struct PPFeature {
        Square squareA;
        Color colorA;
        Square squareB;
        Color colorB;

        static constexpr std::array<Bitboard, 64> MASKS = [] {
            std::array<Bitboard, 64> masks = {};
            for (UInt8 sq = 8; sq < 56; sq++) {
                UInt8 file = sq % 8;
                Bitboard bitboard = Bitboard(File(file));
                if (file > 0) { bitboard |= Bitboard(File(file - 1)); }
                if (file < 7) { bitboard |= Bitboard(File(file + 1)); }
                masks[sq] = bitboard;
            }
            return masks;
        }();

        template<typename FeatureSet>
        constexpr UInt16 index(Color color, Square kingSquare) const noexcept {
            assert(squareA != Square::NONE);
            assert(colorA != Color::NONE);
            assert(squareB != Square::NONE);
            assert(colorB != Color::NONE);
            assert(color != Color::NONE);
            assert(kingSquare != Square::NONE);

            const UInt16 aID = pawnID<FeatureSet>(color, kingSquare, colorA, squareA);
            const UInt16 bID = pawnID<FeatureSet>(color, kingSquare, colorB, squareB);
            const UInt16 hi = std::max(aID, bID);
            const UInt16 lo = std::min(aID, bID);
            return hi * (hi - 1) / 2 + lo;
        }

        operator std::string() const noexcept {
            return std::string(Piece(PieceType::PAWN, colorA)) + "@" + std::string(squareA) + " <-> " +
                   std::string(Piece(PieceType::PAWN, colorB)) + "@" + std::string(squareB);
        }

    private:
        template<typename FeatureSet>
        static constexpr UInt16 pawnID(Color color, Square kingSquare, Color pawnColor, Square pawnSquare) noexcept {
            assert(color != Color::NONE);
            assert(kingSquare != Square::NONE);
            assert(pawnColor != Color::NONE);
            assert(pawnSquare != Square::NONE);
            assert(pawnSquare.index() >= 8 && pawnSquare.index() < 56);

            Square pawnSq = (color == Color::WHITE) ? pawnSquare : pawnSquare.flipped();
            pawnSq = FeatureSet::mirror(pawnSq, kingSquare);

            const UInt16 offset = (color != pawnColor) ? 48 : 0;
            return offset + static_cast<UInt16>(pawnSq) - 8;
        }
    };
}
