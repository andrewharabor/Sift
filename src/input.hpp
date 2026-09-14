#pragma once

#include <algorithm>
#include <array>
#include <cassert>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "reader.hpp"
#include "types.hpp"


namespace Sift {

struct PSQBaseInputs {
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

        constexpr void setPSQRefresh(Color color) noexcept { psqRefresh[color.index()] = true; }
        constexpr bool needsPSQRefresh(Color color) const noexcept { return psqRefresh[color.index()]; }

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

        template<typename Function>
        inline void writeAddTIFeatures(Function) noexcept {};

        template<typename Function>
        inline void writeSubTIFeatures(Function) noexcept {};

        constexpr void setPawns(Bitboard, Bitboard, Bitboard, Bitboard) noexcept {};
    };
};

struct SingleBucketInputs : PSQBaseInputs {
public:
    static constexpr USize PSQ_FEATURES = 768;
    static constexpr USize BUCKET_COUNT = 1;
    static constexpr USize REFRESH_TABLE_SIZE = 1;
    static constexpr bool MIRRORED = false;
    static constexpr bool MERGED_KINGS = false;

    static constexpr bool needsMirror(Square) noexcept { return false; }
    static constexpr Square mirror(Square square, Square) noexcept { return square; }
    static constexpr USize bucket(Color, Square) noexcept { return 0; }
    static constexpr USize refreshTableIdx(Color, Square) noexcept { return 0; }
    static constexpr bool needsRefresh(Color, Square, Square) noexcept { return false; }
};

template<USize... BUCKET_INDICES>
struct KingBucketInputs : PSQBaseInputs {
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

    static constexpr bool needsMirror(Square) noexcept { return false; }
    static constexpr Square mirror(Square square, Square) noexcept { return square; }

    static constexpr USize bucket(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[kingSquare.index()];
    }

    static constexpr USize refreshTableIdx(Color color, Square kingSquare) noexcept { return bucket(color, kingSquare); }

    static constexpr bool needsRefresh(Color color, Square prevKingSquare, Square kingSquare) noexcept {
        assert(color != Color::NONE);
        assert(prevKingSquare != Square::NONE);
        assert(kingSquare != Square::NONE);
        prevKingSquare = (color == Color::WHITE) ? prevKingSquare : prevKingSquare.flipped();
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[prevKingSquare.index()] != BUCKET_LAYOUT[kingSquare.index()];
    }
};

using HalfKAInputs = KingBucketInputs<
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
struct MirroredKingBucketInputs : PSQBaseInputs {
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

public:
    static constexpr USize PSQ_FEATURES = 768;
    static constexpr USize BUCKET_COUNT = *std::ranges::max_element(BUCKET_LAYOUT) + 1;
    static constexpr USize REFRESH_TABLE_SIZE = BUCKET_COUNT * 2;
    static constexpr bool MIRRORED = true;
    static constexpr bool MERGED_KINGS = false;

    static constexpr bool needsMirror(Square kingSquare) noexcept {
        if constexpr (SIDE == MirroredKingSide::ABCD) {
            return kingSquare.file() > File::D;
        } else {
            return kingSquare.file() < File::E;
        }
    }

    static constexpr Square mirror(Square square, Square kingSquare) noexcept {
        return (needsMirror(kingSquare)) ? square.mirrored() : square;
    }

    static constexpr USize bucket(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[kingSquare.index()];
    }

    static constexpr USize refreshTableIdx(Color color, Square kingSquare) noexcept {
        kingSquare = (color == Color::WHITE) ? kingSquare : kingSquare.flipped();
        return BUCKET_LAYOUT[kingSquare.index()] * 2 + needsMirror(kingSquare);
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
        return BUCKET_LAYOUT[prevKingSquare.index()] != BUCKET_LAYOUT[kingSquare.index()];
    }
};

template<MirroredKingSide SIDE>
using MirroredSingleBucketInputs = MirroredKingBucketInputs<
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
using MirroredHalfKAInputs = MirroredKingBucketInputs<
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
struct MergedMirroredKingBucketInputs : MirroredKingBucketInputs<SIDE, BUCKET_INDICES...> {
    static_assert(sizeof...(BUCKET_INDICES) == 32);

    static constexpr bool VALID_LAYOUT = [] {
        const auto abs = [](Int32 a) { return (a > 0) ? a : -a; };
        constexpr std::array<USize, 32> HALF_LAYOUT = {BUCKET_INDICES...};
        for (Int32 sq1 = 0; sq1 < 32; sq1++) {
            for (Int32 sq2 = 0; sq2 < 32; sq2++) {
                if (HALF_LAYOUT[static_cast<USize>(sq1)] == HALF_LAYOUT[static_cast<USize>(sq2)]) {
                    const Int32 rankDiff = abs(sq1 / 4 - sq2 / 4);
                    const Int32 fileDiff = abs(sq1 % 4 - sq2 % 4);
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
using MirroredHalfKAV2Inputs = MergedMirroredKingBucketInputs<
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
struct ThreatInputs : PSQFeatureSet {
public:
    static constexpr bool THREAT_INPUTS = true;
    static constexpr USize THREAT_FEATURES = 60144;
    static constexpr USize MAX_TI_CHANGES = 128;

    struct Updates : PSQBaseInputs::Updates {
        std::array<bool, 2> tiRefresh = {};

        std::array<TIFeature, MAX_TI_CHANGES> tiAdds = {};
        USize tiAddSize = 0;

        std::array<TIFeature, MAX_TI_CHANGES> tiSubs = {};
        USize tiSubSize = 0;

        constexpr void setTIRefresh(Color color) noexcept { tiRefresh[color.index()] = true; };
        constexpr bool needsTIRefresh(Color color) const noexcept { return tiRefresh[color.index()]; }

        constexpr void addTIFeature(TIFeature feature) noexcept {
            assert(tiAddSize < MAX_TI_CHANGES);
            tiAdds[tiAddSize++] = feature;
        };

        constexpr void subTIFeature(TIFeature feature) noexcept {
            assert(tiSubSize < MAX_TI_CHANGES);
            tiSubs[tiSubSize++] = feature;
        };

        template<typename Function>
        inline void writeAddTIFeatures(Function func) noexcept { tiAddSize += func(&tiAdds[tiAddSize]); };

        template<typename Function>
        inline void writeSubTIFeatures(Function func) noexcept { tiSubSize += func(&tiSubs[tiSubSize]); };
    };
};

template<typename PSQFeatureSet>
struct PawnPawnThreatInputs : ThreatInputs<PSQFeatureSet> {
public:
    static constexpr bool PAWN_PAWN_INPUTS = true;
    static constexpr USize THREAT_FEATURES = 64368;
    static constexpr USize THREAT_OFFSET = 4560;

    struct Updates : ThreatInputs<PSQFeatureSet>::Updates {
        std::array<Bitboard, 2> pawnsBefore = {};
        std::array<Bitboard, 2> pawnsAfter = {};

        constexpr void setPawns(Bitboard whiteBefore, Bitboard blackBefore, Bitboard whiteAfter, Bitboard blackAfter) noexcept {
            pawnsBefore[0] = whiteBefore;
            pawnsBefore[1] = blackBefore;
            pawnsAfter[0] = whiteAfter;
            pawnsAfter[1] = blackAfter;
        };
    };
};

template<USize OUTPUTS, typename FeatureSet>
struct FeatureTransformer {
    using InputFeatureSet = FeatureSet;
    using Accumulator = Accumulator<FeatureTransformer>;
    using RefreshTable = RefreshTable<FeatureTransformer, FeatureSet::REFRESH_TABLE_SIZE>;

    static constexpr USize PSQ_INPUT_SIZE = FeatureSet::BUCKET_COUNT * FeatureSet::PSQ_FEATURES;
    static constexpr USize OUTPUT_SIZE = OUTPUTS;
    static constexpr USize PSQ_WEIGHT_SIZE = PSQ_INPUT_SIZE * OUTPUT_SIZE;
    static constexpr USize THREAT_WEIGHT_SIZE = FeatureSet::THREAT_FEATURES * OUTPUT_SIZE;
    static constexpr USize BIAS_SIZE = OUTPUT_SIZE;

    static_assert(PSQ_INPUT_SIZE > 0);
    static_assert(OUTPUT_SIZE > 0);

    NET_PARAM(Int16, PSQ_WEIGHT_SIZE, psqWeights);
    NET_PARAM(Int8, THREAT_WEIGHT_SIZE, threatWeights);
    NET_PARAM(Int16, BIAS_SIZE, biases);

    constexpr const Int16 *psqWeightPtr(USize featureIdx) const noexcept { return &psqWeights[featureIdx * OUTPUT_SIZE]; }
    constexpr const Int8 *threatWeightPtr(USize featureIdx) const noexcept { return &threatWeights[featureIdx * OUTPUT_SIZE]; }

    inline bool load(ByteReader &reader) noexcept { return reader.load(psqWeights) && reader.load(threatWeights) && reader.load(biases); }

    static constexpr USize byteSize() noexcept { return sizeof(Int16) * PSQ_WEIGHT_SIZE + sizeof(Int8) * THREAT_WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE; }
};

}
