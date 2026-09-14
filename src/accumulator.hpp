#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

#include "bitboard.hpp"
#include "color.hpp"
#include "types.hpp"


namespace Sift {

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

template<typename FeatureSet, typename FeatureTransformer>
struct UpdatableAccumulator {
    Accumulator<FeatureTransformer> psqAcc;
    Accumulator<FeatureTransformer> threatAcc;
    std::array<bool, 2> psqDirty;
    std::array<bool, 2> threatDirty;
    FeatureSet::Updates updates;

    constexpr void setPSQDirty() noexcept { psqDirty[0] = psqDirty[1] = true; }

    constexpr void setPSQClean(Color color) noexcept {
        assert(color != Color::NONE);
        psqDirty[color.index()] = false;
    }

    constexpr bool isPSQDirty(Color color) const noexcept {
        assert(color != Color::NONE);
        return psqDirty[color.index()];
    }

    constexpr void setThreatDirty() noexcept { threatDirty[0] = threatDirty[1] = true; }

    constexpr void setThreatClean(Color color) noexcept {
        assert(color != Color::NONE);
        threatDirty[color.index()] = false;
    }

    constexpr bool isThreatDirty(Color color) const noexcept {
        assert(color != Color::NONE);
        return threatDirty[color.index()];
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

}
