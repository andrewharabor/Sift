#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "features.hpp"
#include "geometry.hpp"
#include "loader.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"
#include "types.hpp"

#define NET_PARAM(Type, SIZE, name) \
    std::span<const Type, SIZE> name = std::span<const Type, SIZE>{static_cast<const Type *>(nullptr), SIZE}


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

template<typename FeatureTransformer>
class Accumulator {
private:
    static constexpr USize INPUT_SIZE = FeatureTransformer::PSQ_INPUT_SIZE;
    static constexpr USize WEIGHT_SIZE = FeatureTransformer::PSQ_WEIGHT_SIZE;
    static constexpr USize OUTPUT_SIZE = FeatureTransformer::OUTPUT_SIZE;

    using OutputType = std::span<Int16, OUTPUT_SIZE>;
    using ConstOutputType = std::span<const Int16, OUTPUT_SIZE>;
    using WeightType = std::span<const Int16, WEIGHT_SIZE>;

    alignas(SIMD::ALIGNMENT) MultiArray<Int16, 2, OUTPUT_SIZE> output_;

public:
    Accumulator() noexcept : output_() {}

    inline ConstOutputType output(Color color) const noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline OutputType output(Color color) noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline void init(const FeatureTransformer &ft) noexcept {
        std::ranges::copy(ft.biases, output_[0].begin());
        std::ranges::copy(ft.biases, output_[1].begin());
    }

    inline void sub1Add1From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub < INPUT_SIZE);
        assert(add < INPUT_SIZE);


        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset = sub * OUTPUT_SIZE;
        USize addOffset = add * OUTPUT_SIZE;

        assert(subOffset + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i];
        }
    }

    inline void sub2Add1From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add < INPUT_SIZE);

        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = sub1 * OUTPUT_SIZE;
        USize subOffset2 = sub2 * OUTPUT_SIZE;
        USize addOffset = add * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
        }
    }

    inline void sub2Add2From(const Accumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add1, USize add2) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add1 < INPUT_SIZE);
        assert(add2 < INPUT_SIZE);

        ConstOutputType src = acc.output(color);
        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = sub1 * OUTPUT_SIZE;
        USize subOffset2 = sub2 * OUTPUT_SIZE;
        USize addOffset1 = add1 * OUTPUT_SIZE;
        USize addOffset2 = add2 * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset1 + i] + delta[addOffset2 + i] - delta[subOffset1 + i] - delta[subOffset2 + i];
        }
    }


    inline void add1(const FeatureTransformer &ft, Color color, USize feature) noexcept {
        assert(color != Color::NONE);
        assert(feature < INPUT_SIZE);

        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize addOffset = feature * OUTPUT_SIZE;

        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] += delta[addOffset + i];
        }
    }

    inline void sub1(const FeatureTransformer &ft, Color color, USize feature) noexcept {
        assert(color != Color::NONE);
        assert(feature < INPUT_SIZE);

        ConstOutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset = feature * OUTPUT_SIZE;

        assert(subOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] -= delta[subOffset + i];
        }
    }

    inline void add4(const FeatureTransformer &ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
        assert(color != Color::NONE);
        assert(feature1 < INPUT_SIZE);
        assert(feature2 < INPUT_SIZE);
        assert(feature3 < INPUT_SIZE);
        assert(feature4 < INPUT_SIZE);

        OutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize addOffset1 = feature1 * OUTPUT_SIZE;
        USize addOffset2 = feature2 * OUTPUT_SIZE;
        USize addOffset3 = feature3 * OUTPUT_SIZE;
        USize addOffset4 = feature4 * OUTPUT_SIZE;

        assert(addOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset3 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset4 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] += delta[addOffset1 + i] + delta[addOffset2 + i] + delta[addOffset3 + i] + delta[addOffset4 + i];
        }
    }

    inline void sub4(const FeatureTransformer &ft, Color color, USize feature1, USize feature2, USize feature3, USize feature4) noexcept {
        assert(color != Color::NONE);
        assert(feature1 < INPUT_SIZE);
        assert(feature2 < INPUT_SIZE);
        assert(feature3 < INPUT_SIZE);
        assert(feature4 < INPUT_SIZE);

        ConstOutputType dst = output(color);
        WeightType delta = ft.psqWeights;
        USize subOffset1 = feature1 * OUTPUT_SIZE;
        USize subOffset2 = feature2 * OUTPUT_SIZE;
        USize subOffset3 = feature3 * OUTPUT_SIZE;
        USize subOffset4 = feature4 * OUTPUT_SIZE;

        assert(subOffset1 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset2 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset3 + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(subOffset4 + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] -= delta[subOffset1 + i] + delta[subOffset2 + i] + delta[subOffset3 + i] + delta[subOffset4 + i];
        }
    }
};

template<typename Accumulator>
struct RefreshTableEntry {
    Accumulator accumulator = Accumulator();
    MultiArray<Bitboard, 2, 2> pieceBitboardSet = {};
    MultiArray<Bitboard, 2, 6> occupancyBitboardSet = {};

    constexpr std::span<Bitboard, 2> pieceBitboards(Color color) noexcept {
        assert(color != Color::NONE);
        return pieceBitboardSet[color.index()];
    }

    constexpr std::span<Bitboard, 6> occupancyBitboards(Color color) noexcept {
        assert(color != Color::NONE);
        return occupancyBitboardSet[color.index()];
    }
};

template<typename FeatureTransformer, USize SIZE>
struct RefreshTable {
    std::array<RefreshTableEntry<Accumulator<FeatureTransformer>>, SIZE> entries = {};

    inline void init(const FeatureTransformer &ft) noexcept {
        for (auto &entry : entries) {
            entry.accumulator.init(ft);
            entry.pieceBitboardSet.fill(Bitboard());
            entry.occupancyBitboardSet.fill(Bitboard());
        }
    }
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

    inline bool load(ByteLoader &loader) noexcept { return loader.load(psqWeights) && loader.load(threatWeights) && loader.load(biases); }

    static constexpr USize byteSize() noexcept { return sizeof(Int16) * PSQ_WEIGHT_SIZE + sizeof(Int8) * THREAT_WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE; }
};

struct ReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        const Vec<Int16> clamped = SIMD::max<Int16>(inputs, zero);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct CReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        return SIMD::mulAddAdjAcc<Int16>(sum, clamped, weights);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value; }
};

struct SCReLUActivation {
    template<Int16 MAX>
    static inline WidenedVec<Int16> actDotAcc(WidenedVec<Int16> sum, Vec<Int16> inputs, Vec<Int16> weights) noexcept {
        static const Vec<Int16> zero = SIMD::zero<Int16>();
        static const Vec<Int16> max = SIMD::set<Int16>(MAX);
        const Vec<Int16> clamped = SIMD::clamp<Int16>(inputs, zero, max);
        const Vec<Int16> crelu = SIMD::mulLo<Int16>(clamped, weights);
        return SIMD::mulAddAdjAcc<Int16>(sum, crelu, clamped);
    }

    template<Int32 MAX>
    static inline Int32 output(Int32 value) noexcept { return value / MAX; }
};

struct SingleBucketOutput {
public:
    static constexpr USize BUCKET_COUNT = 1;

    static constexpr USize bucket(const Position &) noexcept { return 0; }
};

struct OppColoredBishopBucketOutput {
public:
    static constexpr USize BUCKET_COUNT = 2;

    static constexpr USize bucket(const Position &position) noexcept {
        const bool bishops = !position.pieces(Piece::WHITE_BISHOP).empty() && !position.pieces(Piece::BLACK_BISHOP).empty();
        const bool oppColored = (position.pieces(Piece::WHITE_BISHOP) & LIGHT_SQUARES).empty() != (position.pieces(Piece::BLACK_BISHOP) & LIGHT_SQUARES).empty();
        return (bishops && oppColored) ? 1 : 0;
    }

private:
    static constexpr Bitboard LIGHT_SQUARES = Bitboard(0x55AA55AA55AA55AAULL);
};

template<USize BUCKETS>
struct MaterialCountBucketOutput {
    static_assert(BUCKETS == 2 || BUCKETS == 4 || BUCKETS == 8 || BUCKETS == 16 || BUCKETS == 32);

public:
    static constexpr USize BUCKET_COUNT = BUCKETS;

    static constexpr USize bucket(const Position &position) noexcept { return (position.occupied().count() - 2) / DIV; }

private:
    static constexpr USize DIV = 32 / BUCKET_COUNT;
};

template<typename FeatureTransformer, typename Output, typename Arch>
class PerspectiveNetwork {
private:
    using InputType = std::span<const Int16, FeatureTransformer::OUTPUT_SIZE>;

public:
    constexpr const FeatureTransformer &ft() const noexcept { return ft_; }

    inline Int32 forward(const Position &position, InputType friendlyPSQInputs, InputType enemyPSQInputs, InputType friendlyThreatInputs, InputType enemyThreatInputs) const noexcept { return arch_.forward(Output::bucket(position), friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs); }

    inline bool load(ByteLoader &loader) noexcept {
        if (!ft_.load(loader) || !arch_.load(loader)) {
            return false;
        }

        if (Arch::NEEDS_FT_PERMUTE) {
            Arch::permuteFTParams(ft_.psqWeights, ft_.threatWeights, ft_.biases);
        }
        return true;
    }

    static constexpr USize byteSize() noexcept { return FeatureTransformer::byteSize() + Arch::byteSize(); }

private:
    FeatureTransformer ft_;
    Arch arch_;
};

}
