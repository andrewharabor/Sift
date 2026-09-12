#pragma once

#include <algorithm>
#include <array>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "loader.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "simd.hpp"
#include "types.hpp"

#define NET_PARAM(Type, SIZE, name) \
    std::span<const Type, SIZE> name = std::span<const Type, SIZE>{static_cast<const Type *>(nullptr), SIZE}


namespace Sift {

template<typename FeatureTransformer>
class PSQAccumulator {
private:
    using Type = typename FeatureTransformer::OutputType;

    static constexpr USize INPUT_SIZE = FeatureTransformer::PSQ_INPUT_SIZE;
    static constexpr USize WEIGHT_SIZE = FeatureTransformer::PSQ_WEIGHT_SIZE;
    static constexpr USize OUTPUT_SIZE = FeatureTransformer::OUTPUT_SIZE;

    alignas(SIMD::ALIGNMENT) MultiArray<Type, 2, OUTPUT_SIZE> output_;

public:
    PSQAccumulator() noexcept : output_() {}

    inline std::span<const Type, OUTPUT_SIZE> output(Color color) const noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline std::span<Type, OUTPUT_SIZE> output(Color color) noexcept {
        assert(color != Color::NONE);
        return output_[color.index()];
    }

    inline void init(const FeatureTransformer &ft) noexcept {
        std::ranges::copy(ft.biases, output_[0].begin());
        std::ranges::copy(ft.biases, output_[1].begin());
    }

    inline void sub1Add1From(const PSQAccumulator &acc, const FeatureTransformer &ft, Color color, USize sub, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub < INPUT_SIZE);
        assert(add < INPUT_SIZE);

        std::span<const Type, OUTPUT_SIZE> src = acc.output(color);
        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
        USize subOffset = sub * OUTPUT_SIZE;
        USize addOffset = add * OUTPUT_SIZE;

        assert(subOffset + OUTPUT_SIZE <= WEIGHT_SIZE);
        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] = src[i] + delta[addOffset + i] - delta[subOffset + i];
        }
    }

    inline void sub2Add1From(const PSQAccumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add < INPUT_SIZE);

        std::span<const Type, OUTPUT_SIZE> src = acc.output(color);
        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
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

    inline void sub2Add2From(const PSQAccumulator &acc, const FeatureTransformer &ft, Color color, USize sub1, USize sub2, USize add1, USize add2) noexcept {
        assert(color != Color::NONE);
        assert(sub1 < INPUT_SIZE);
        assert(sub2 < INPUT_SIZE);
        assert(add1 < INPUT_SIZE);
        assert(add2 < INPUT_SIZE);

        std::span<const Type, OUTPUT_SIZE> src = acc.output(color);
        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
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

        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
        USize addOffset = feature * OUTPUT_SIZE;

        assert(addOffset + OUTPUT_SIZE <= WEIGHT_SIZE);

        for (USize i = 0; i < OUTPUT_SIZE; i++) {
            dst[i] += delta[addOffset + i];
        }
    }

    inline void sub1(const FeatureTransformer &ft, Color color, USize feature) noexcept {
        assert(color != Color::NONE);
        assert(feature < INPUT_SIZE);

        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
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

        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
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

        std::span<Type, OUTPUT_SIZE> dst = output(color);
        std::span<const Type, WEIGHT_SIZE> delta = ft.psqWeights;
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
    std::array<RefreshTableEntry<PSQAccumulator<FeatureTransformer>>, SIZE> entries = {};

    inline void init(const FeatureTransformer &ft) noexcept {
        for (auto &entry : entries) {
            entry.accumulator.init(ft);
            entry.pieceBitboardSet.fill(Bitboard());
            entry.occupancyBitboardSet.fill(Bitboard());
        }
    }
};

template<typename PSQType, typename ThreatType, USize OUTPUTS, typename FeatureSet>
struct FeatureTransformer {
    using PSQWeightType = PSQType;
    using ThreatWeightType = ThreatType;
    using OutputType = PSQType;
    using InputFeatureSet = FeatureSet;
    using Accumulator = PSQAccumulator<FeatureTransformer>;
    using RefreshTable = RefreshTable<FeatureTransformer, FeatureSet::REFRESH_TABLE_SIZE>;

    static constexpr USize PSQ_INPUT_SIZE = FeatureSet::BUCKET_COUNT * FeatureSet::PSQ_FEATURES;
    static constexpr USize OUTPUT_SIZE = OUTPUTS;
    static constexpr USize PSQ_WEIGHT_SIZE = PSQ_INPUT_SIZE * OUTPUT_SIZE;
    static constexpr USize THREAT_WEIGHT_SIZE = FeatureSet::THREAT_FEATURES * OUTPUT_SIZE;
    static constexpr USize BIAS_SIZE = OUTPUT_SIZE;

    static_assert(PSQ_INPUT_SIZE > 0);
    static_assert(OUTPUT_SIZE > 0);

    NET_PARAM(PSQWeightType, PSQ_WEIGHT_SIZE, psqWeights);
    NET_PARAM(ThreatWeightType, THREAT_WEIGHT_SIZE, threatWeights);
    NET_PARAM(OutputType, BIAS_SIZE, biases);

    constexpr const PSQWeightType *psqWeightPtr(USize featureIdx) const noexcept { return &psqWeights[featureIdx * OUTPUT_SIZE]; }
    constexpr const ThreatWeightType *threatWeightPtr(USize featureIdx) const noexcept { return &threatWeights[featureIdx * OUTPUT_SIZE]; }

    inline bool load(NetLoader &loader) noexcept { return loader.load(psqWeights) && loader.load(threatWeights) && loader.load(biases); }

    static constexpr USize byteSize() noexcept { return sizeof(PSQWeightType) * PSQ_WEIGHT_SIZE + sizeof(ThreatWeightType) * THREAT_WEIGHT_SIZE + sizeof(OutputType) * BIAS_SIZE; }
};

struct ReLUActivation {
    template<typename Type, Type MAX>
    static inline WidenedVec<Type> actDotAcc(WidenedVec<Type> sum, Vec<Type> inputs, Vec<Type> weights) noexcept {
        static const Vec<Type> zero = SIMD::zero<Type>();
        const Vec<Type> clamped = SIMD::max<Type>(inputs, zero);
        return SIMD::mulAddAdjAcc<Type>(sum, clamped, weights);
    }

    template<typename Type, Type MAX>
    static inline Type quantize(Type value) noexcept { return value; }
};

struct CReLUActivation {
    template<typename Type, Type MAX>
    static inline WidenedVec<Type> actDotAcc(WidenedVec<Type> sum, Vec<Type> inputs, Vec<Type> weights) noexcept {
        static const Vec<Type> zero = SIMD::zero<Type>();
        static const Vec<Type> max = SIMD::set<Type>(MAX);
        const Vec<Type> clamped = SIMD::clamp<Type>(inputs, zero, max);
        return SIMD::mulAddAdjAcc<Type>(sum, clamped, weights);
    }

    template<typename Type, Type MAX>
    static inline Type quantize(Type value) noexcept { return value; }
};

struct SCReLUActivation {
    template<typename Type, Type MAX>
    static inline WidenedVec<Type> actDotAcc(WidenedVec<Type> sum, Vec<Type> inputs, Vec<Type> weights) noexcept {
        static const Vec<Type> zero = SIMD::zero<Type>();
        static const Vec<Type> max = SIMD::set<Type>(MAX);
        const Vec<Type> clamped = SIMD::clamp<Type>(inputs, zero, max);
        const Vec<Type> crelu = SIMD::mulLo<Type>(clamped, weights);
        return SIMD::mulAddAdjAcc<Type>(sum, crelu, clamped);
    }

    template<typename Type, Type MAX>
    static inline Type quantize(Type value) noexcept { return value / MAX; }
};

class SingleBucketOutput {
public:
    static constexpr USize BUCKET_COUNT = 1;

    static constexpr USize bucket(const Position &) noexcept { return 0; }
};

class OppColoredBishopBucketOutput {
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
class MaterialBucketOutput {
    static_assert(BUCKETS == 2 || BUCKETS == 4 || BUCKETS == 8 || BUCKETS == 16 || BUCKETS == 32);

public:
    static constexpr USize BUCKET_COUNT = BUCKETS;

    static constexpr USize bucket(const Position &position) noexcept { return (position.occupied().count() - 2) / DIV; }

private:
    static constexpr USize DIV = 32 / BUCKET_COUNT;
};

template<typename FeatureTransformer, typename Output, typename Arch>
class PerspectiveNetwork {
public:
    using InputType = std::span<const typename FeatureTransformer::OutputType, FeatureTransformer::OUTPUT_SIZE>;

    constexpr const FeatureTransformer &ft() const noexcept { return ft_; }

    inline typename Arch::OutputType forward(const Position &position, InputType friendlyPSQInputs, InputType enemyPSQInputs, InputType friendlyThreatInputs, InputType enemyThreatInputs) const noexcept { return arch_.forward(Output::bucket(position), friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs); }

    inline bool load(NetLoader &loader) noexcept {
        if (!ft_.load(loader) || !arch_.load(loader)) {
            return false;
        }

        if (Arch::NEEDS_FT_PERMUTE) {
            Arch::template permuteFT<typename FeatureTransformer::PSQWeightType, typename FeatureTransformer::ThreatWeightType, typename FeatureTransformer::OutputType>(ft_.psqWeights, ft_.threatWeights, ft_.biases);
        }
        return true;
    }

    constexpr USize byteSize() const noexcept { return FeatureTransformer::byteSize() + Arch::byteSize(); }

private:
    FeatureTransformer ft_;
    Arch arch_;
};

}
