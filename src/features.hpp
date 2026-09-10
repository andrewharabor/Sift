#pragma once

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"


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

        const USize typeIdx = static_cast<USize>(piece.type());

        const USize colorIdx = [piece, color]() -> USize {
            if constexpr (FeatureSet::MERGED_KINGS) {
                if (piece.type() == PieceType::KING) {
                    return 0;
                }
            }
            return (piece.color() == color) ? 0 : 1;
        }();

        Square sq = (color == Color::WHITE) ? square : square.flipped();
        sq = FeatureSet::mirror(sq, kingSquare);

        const USize bucketOffset = FeatureSet::bucket(color, kingSquare) * FeatureSet::PSQ_FEATURES;

        return bucketOffset + (colorIdx * 384) + (typeIdx * 64) + static_cast<USize>(sq);
    }
};

struct TIFeature {
    Piece attacker;
    Square attackerSq;
    Piece victim;
    Square victimSq;

private:
    static constexpr MultiArray<Int32, 6, 6> PIECE_TARGET_MAP_PAWNS = {{
        {0, 1, -1, 2, -1, -1},
        {0, 1, 2, 3, 4, -1},
        {0, 1, 2, 3, -1, -1},
        {0, 1, 2, 3, -1, -1},
        {0, 1, 2, 3, 4, -1},
        {-1, -1, -1, -1, -1, -1},
    }};

    static constexpr MultiArray<Int32, 6, 6> PIECE_TARGET_MAP_NO_PAWNS = {{
        {-1, 0, -1, 1, -1, -1},
        {0, 1, 2, 3, 4, -1},
        {0, 1, 2, 3, -1, -1},
        {0, 1, 2, 3, -1, -1},
        {0, 1, 2, 3, 4, -1},
        {-1, -1, -1, -1, -1, -1},
    }};

    template<typename FeatureSet>
    static constexpr MultiArray<Int32, 6, 6> PIECE_TARGET_MAP = (FeatureSet::PAWN_PAWN_INPUTS) ? PIECE_TARGET_MAP_NO_PAWNS : PIECE_TARGET_MAP_PAWNS;

    template<typename FeatureSet>
    static constexpr std::array<Int32, 6> PIECE_TARGET_COUNT = [] {
        std::array<Int32, 6> counts = {};
        for (USize attacker = 0; attacker < 6; attacker++) {
            Int32 count = 0;
            for (USize victim = 0; victim < 6; victim++) {
                if (PIECE_TARGET_MAP<FeatureSet>[attacker][victim] >= 0) {
                    count++;
                }
            }
            counts[attacker] = 2 * count;
        }
        return counts;
    }();

    static inline MultiArray<Int32, 64, 64> pieceIndices(Piece piece) noexcept {
        Attacks::init();
        MultiArray<Int32, 64, 64> indices = {};
        for (UInt8 i = 0; i < 64; i++) {
            const Square from = Square(i);
            Bitboard attacks = Attacks::attacks(piece, from, Bitboard());
            for (UInt8 j = 0; j < 64; j++) {
                const Square to = Square(j);
                const Bitboard mask = attacks & (Bitboard(to).bits() - 1);
                indices[i][j] = static_cast<Int32>(mask.count());
            }
        }
        return indices;
    }

    static inline MultiArray<Int32, 12, 64, 64> PIECE_INDICES = [] {
        MultiArray<Int32, 12, 64, 64> indices = {};
        indices[static_cast<USize>(Piece::WHITE_PAWN)] = pieceIndices(Piece::WHITE_PAWN);
        indices[static_cast<USize>(Piece::BLACK_PAWN)] = pieceIndices(Piece::BLACK_PAWN);
        for (const PieceType pieceType : {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING}) {
            const MultiArray<Int32, 64, 64> idx = pieceIndices(Piece(pieceType, Color::BLACK));
            indices[static_cast<USize>(Piece(pieceType, Color::WHITE))] = idx;
            indices[static_cast<USize>(Piece(pieceType, Color::BLACK))] = idx;
        }
        return indices;
    }();

    template<typename FeatureSet>
    static inline auto OFFSETS = [] {
        Attacks::init();

        struct {
            std::array<std::pair<Int32, Int32>, 12> indices = {};
            MultiArray<Int32, 12, 64> offsets = {};
        } offsets;

        Int32 offset = 0;
        for (UInt8 p = 0; p < 12; p++) {
            const Piece piece = Piece(p);
            Int32 pieceOffset = 0;
            for (UInt8 sq = 0; sq < 64; sq++) {
                const Square square = Square(sq);
                offsets.offsets[static_cast<USize>(piece)][sq] = pieceOffset;
                if (piece.type() != PieceType::PAWN || (square.rank() > Rank::FIRST && square.rank() < Rank::EIGHTH)) {
                    const Bitboard attacks = Attacks::attacks(Piece(piece.type(), ~piece.color()), square, Bitboard());
                    pieceOffset += static_cast<Int32>(attacks.count());
                }
            }
            offsets.indices[static_cast<USize>(piece)] = {pieceOffset, offset};
            offset += PIECE_TARGET_COUNT<FeatureSet>[static_cast<USize>(piece.type())] * pieceOffset;
        }
        return offsets;
    }();

    template<typename FeatureSet>
    static inline MultiArray<Int32, 12, 12, 2> ATTACK_INDICES = [] {
        MultiArray<Int32, 12, 12, 2> indices = {};
        for (UInt8 i = 0; i < 12; i++) {
            const Piece attacker = Piece(i);
            for (UInt8 j = 0; j < 12; j++) {
                const Piece victim = Piece(j);
                const bool enemies = attacker.color() != victim.color();
                const Int32 map = PIECE_TARGET_MAP<FeatureSet>[static_cast<USize>(attacker.type())][static_cast<USize>(victim.type())];
                const bool semiExcluded = ((attacker.type() == victim.type()) && (enemies || attacker.type() != PieceType::PAWN));
                const bool excluded = map < 0;
                const auto [pieceOffset, offset] = OFFSETS<FeatureSet>.indices[i];
                const Int32 featureIdx = offset + (static_cast<Int32>(~victim.color()) * (PIECE_TARGET_COUNT<FeatureSet>[static_cast<USize>(attacker.type())] / 2) + map) * pieceOffset;
                indices[i][j][0] = (excluded) ? std::numeric_limits<Int32>::min() : featureIdx;
                indices[i][j][1] = (excluded || semiExcluded) ? std::numeric_limits<Int32>::min() : featureIdx;
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
            if (file > 0) {
                bitboard |= Bitboard(File(file - 1));
            }
            if (file < 7) {
                bitboard |= Bitboard(File(file + 1));
            }
            masks[sq] = bitboard;
        }
        return masks;
    }();
};

class PSQFeaturesBase {
public:
    static constexpr bool THREAT_INPUTS = false;
    static constexpr bool PAWN_PAWN_INPUTS = false;
    static constexpr USize THREAT_FEATURES = 0;
    static constexpr USize THREAT_OFFSET = 0;
    static constexpr USize MAX_TI_CHANGES = 0;

    struct Updates {
        std::array<bool, 2> psqRefresh = {};

        std::array<PSQFeature, 2> psqAdds = {};
        USize psqAddSize = 0;

        std::array<PSQFeature, 2> psqSubs = {};
        USize psqSubSize = 0;

        static constexpr std::array<TIFeature, 0> tiAdds = {};
        static constexpr USize tiAddSize = 0;

        static constexpr std::array<TIFeature, 0> tiSubs = {};
        static constexpr USize tiSubSize = 0;

        static constexpr std::array<Bitboard, 0> pawnsBefore = {};
        static constexpr std::array<Bitboard, 0> pawnsAfter = {};

        constexpr void setPSQRefresh(Color color) noexcept { psqRefresh[static_cast<USize>(color)] = true; }
        constexpr bool needsPSQRefresh(Color color) const noexcept { return psqRefresh[static_cast<USize>(color)]; }

        constexpr void addPSQFeature(PSQFeature feature) noexcept {
            assert(psqAddSize < 2);
            psqAdds[psqAddSize++] = feature;
        }

        constexpr void subPSQFeature(PSQFeature feature) noexcept {
            assert(psqSubSize < 2);
            psqSubs[psqSubSize++] = feature;
        }

        constexpr void setTIRefresh(Color) noexcept {};
        constexpr bool needsTIRefresh(Color) const noexcept { return false; }

        constexpr void addTIFeature(TIFeature) noexcept {};
        constexpr void subTIFeature(TIFeature) noexcept {};

        constexpr void setPawns(Bitboard, Bitboard, Bitboard, Bitboard) noexcept {};
    };
};

class SingleBucket : PSQFeaturesBase {
public:
    static constexpr USize PSQ_FEATURES = 768;
    static constexpr USize BUCKET_COUNT = 1;
    static constexpr USize REFRESH_TABLE_SIZE = 1;
    static constexpr bool MIRRORED = false;
    static constexpr bool MERGED_KINGS = false;

    static constexpr Square mirror(Square square, Square) noexcept { return square; }
    static constexpr USize bucket(Color, Square) noexcept { return 0; }
    static constexpr USize refreshTableIdx(Color, Square) noexcept { return 0; }
    static constexpr bool needsRefresh(Color, Square, Square) noexcept { return false; }
};

template<USize... BUCKET_INDICES>
class KingBuckets : PSQFeaturesBase {
    static_assert(sizeof...(BUCKET_INDICES) == 64);

private:
    static constexpr std::array<USize, 64> BUCKET_LAYOUT = {BUCKET_INDICES...};

public:
    static constexpr USize PSQ_FEATURES = 768;
    static constexpr USize BUCKET_COUNT = *std::ranges::max_element(BUCKET_LAYOUT) + 1;
    static constexpr USize REFRESH_TABLE_SIZE = BUCKET_COUNT;
    static constexpr bool MIRRORED = false;
    static constexpr bool MERGED_KINGS = false;

    static_assert(BUCKET_COUNT > 1);

    static constexpr Square mirror(Square square, Square) noexcept { return square; }

    static constexpr USize bucket(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[static_cast<USize>(kingSquare)];
    }

    static constexpr USize refreshTableIdx(Color color, Square kingSquare) noexcept { return bucket(color, kingSquare); }

    static constexpr bool needsRefresh(Color color, Square prevKingSquare, Square kingSquare) noexcept {
        assert(color != Color::NONE);
        assert(prevKingSquare != Square::NONE);
        assert(kingSquare != Square::NONE);
        prevKingSquare = (color == Color::WHITE) ? prevKingSquare : prevKingSquare.flipped();
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[static_cast<USize>(prevKingSquare)] != BUCKET_LAYOUT[static_cast<USize>(kingSquare)];
    }
};

using HalfKA = KingBuckets<
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63
>;

enum class MirroredKingSide : UInt8 {
    ABCD,
    EFGH
};

template<MirroredKingSide SIDE, USize... BUCKET_INDICES>
class KingBucketsMirrored : PSQFeaturesBase {
    static_assert(sizeof...(BUCKET_INDICES) == 32);

private:
    static constexpr std::array<USize, 64> BUCKET_LAYOUT = [] {
        constexpr std::array<USize, 32> HALF_LAYOUT = {BUCKET_INDICES...};
        std::array<USize, 64> layout = {};
        for (USize rank = 0; rank < 8; rank++) {
            for (USize file = 0; file < 4; file++) {
                const USize halfIdx = rank * 4 + file;
                const USize fullIdx = rank * 8 + file;
                layout[fullIdx] = HALF_LAYOUT[halfIdx];
                layout[fullIdx ^ 7] = HALF_LAYOUT[halfIdx];
            }
        }
        return layout;
    }();

    static constexpr bool needsMirror(Square kingSquare) noexcept {
        if constexpr (SIDE == MirroredKingSide::ABCD) {
            return kingSquare.file() > File::D;
        } else {
            return kingSquare.file() < File::E;
        }
    }

public:
    static constexpr USize PSQ_FEATURES = 768;
    static constexpr USize BUCKET_COUNT = *std::ranges::max_element(BUCKET_LAYOUT) + 1;
    static constexpr USize REFRESH_TABLE_SIZE = BUCKET_COUNT * 2;
    static constexpr bool MIRRORED = true;
    static constexpr bool MERGED_KINGS = false;

    static constexpr Square mirror(Square square, Square kingSquare) noexcept {
        return (needsMirror(kingSquare)) ? square.mirrored() : square;
    }

    static constexpr USize bucket(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[static_cast<USize>(kingSquare)];
    }

    static constexpr USize refreshTableIdx(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[static_cast<USize>(kingSquare)] * 2 + needsMirror(kingSquare);
    }

    static constexpr bool needsRefresh(Color color, Square prevKingSquare, Square kingSquare) noexcept {
        assert(color != Color::NONE);
        assert(prevKingSquare != Square::NONE);
        assert(kingSquare != Square::NONE);

        if (needsMirror(prevKingSquare) != needsMirror(kingSquare)) {
            return true;
        }

        prevKingSquare = (color == Color::WHITE) ? prevKingSquare : prevKingSquare.flipped();
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[static_cast<USize>(prevKingSquare)] != BUCKET_LAYOUT[static_cast<USize>(kingSquare)];
    }
};

template<MirroredKingSide SIDE>
using SingleBucketMirrored = KingBucketsMirrored<
    SIDE,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
>;

template <MirroredKingSide SIDE>
using HalfKAMirrored = KingBucketsMirrored<
    SIDE,
    0, 1, 2, 3,
    4, 5, 6, 7,
    8, 9, 10, 11,
    12, 13, 14, 15,
    16, 17, 18, 19,
    20, 21, 22, 23,
    24, 25, 26, 27,
    28, 29, 30, 31
>;

template<MirroredKingSide SIDE, USize... BUCKET_INDICES>
class KingBucketsMergedMirrored : KingBucketsMirrored<SIDE, BUCKET_INDICES...> {
    static_assert(sizeof...(BUCKET_INDICES) == 32);

    static constexpr bool VALID_LAYOUT = [] {
        constexpr std::array<USize, 32> HALF_LAYOUT = {BUCKET_INDICES...};
        for (Int32 sq1 = 0; sq1 < 32; sq1++) {
            for (Int32 sq2 = 0; sq2 < 32; sq2++) {
                if (HALF_LAYOUT[static_cast<USize>(sq1)] == HALF_LAYOUT[static_cast<USize>(sq2)]) {
                    const Int32 rankDiff = std::abs(sq1 / 4 - sq2 / 4);
                    const Int32 fileDiff = std::abs(sq1 % 4 - sq2 % 4);
                    if (rankDiff > 1 || fileDiff > 1) {
                        return false;
                    }
                }
            }
        }
        return true;
    }();

    static_assert(VALID_LAYOUT);

public:
    static constexpr USize PSQ_FEATURES = 704;
    static constexpr bool MERGED_KINGS = true;
};

template <MirroredKingSide SIDE>
using HalfKAV2Mirrored = KingBucketsMergedMirrored<
    SIDE,
    0, 1, 2, 3,
    4, 5, 6, 7,
    8, 9, 10, 11,
    12, 13, 14, 15,
    16, 17, 18, 19,
    20, 21, 22, 23,
    24, 25, 26, 27,
    28, 29, 30, 31
>;

template<typename PSQFeatureSet>
class ThreatInputs : PSQFeatureSet {
public:
    static constexpr bool THREAT_INPUTS = true;
    static constexpr USize THREAT_FEATURES = 60144;
    static constexpr USize MAX_TI_CHANGES = 128;

    struct Updates : PSQFeaturesBase::Updates {
        std::array<bool, 2> tiRefresh = {};

        std::array<TIFeature, MAX_TI_CHANGES> tiAdds = {};
        USize tiAddSize = 0;

        std::array<TIFeature, MAX_TI_CHANGES> tiSubs = {};
        USize tiSubSize = 0;

        constexpr void setTIRefresh(Color color) noexcept { tiRefresh[static_cast<USize>(color)] = true; };
        constexpr bool needsTIRefresh(Color color) const noexcept { return tiRefresh[static_cast<USize>(color)]; }

        constexpr void addTIFeature(TIFeature) noexcept {
            assert(tiAddSize < MAX_TI_CHANGES);
            tiAdds[tiAddSize++] = feature;
        };

        constexpr void subTIFeature(TIFeature) noexcept {
            assert(tiSubSize < MAX_TI_CHANGES);
            tiSubs[tiSubSize++] = feature;
        };
    }
};

template<typename PSQFeatureSet>
class PawnPawnThreatInputs : ThreatInputs<PSQFeatureSet> {
public:
    static constexpr bool PAWN_PAWN_INPUTS = true;
    static constexpr USize THREAT_FEATURES = 64368;
    static constexpr USize THREAT_OFFSET = 4560;

    struct Updates : ThreatInputs<PSQFeatureSet>::Updates {
        std::array<Bitboard, 2> pawnsBefore = {};
        std::array<Bitboard, 2> pawnsAfter = {};

        constexpr void setPawns(Bitboard blackBefore, Bitboard whiteBefore, Bitboard blackAfter, Bitboard whiteAfter) noexcept {
            pawnsBefore[0] = whiteBefore;
            pawnsBefore[1] = blackBefore;
            pawnsAfter[0] = whiteAfter;
            pawnsAfter[1] = blackAfter;
        };
    }
};

}
