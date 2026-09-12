#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <fstream>
#include <span>

#include "loader.hpp"
#include "simd.hpp"
#include "types.hpp"


namespace Sift {

template<USize L1_SIZE>
class SparseIterator {

#if defined(USE_AVX512) && defined(USE_VBMI2)

public:
    inline void update(VecUInt8 vec1, VecUInt8 vec2) noexcept {
        const UInt32 mask = _mm512_kunpackw(SIMD::nonzeroMask<UInt8>(vec2), SIMD::nonzeroMask<UInt8>(vec1));
        const auto indices = _mm512_maskz_compress_epi16(mask, base_);
        _mm512_storeu_si512(&indices_[count_], indices);
        base_ = _mm512_add_epi16(base_, _mm512_set1_epi16(32));
        count_ += std::popcount(mask);

        assert(count_ <= indices_.size());
    }

    constexpr USize count() const noexcept { return count_; }
    constexpr USize index(USize idx) const noexcept { return indices_[idx]; }

private:
    static constexpr USize CHUNKS = L1_SIZE / (sizeof(Int32) / sizeof(Int8));

    alignas(SIMD::ALIGNMENT) std::array<UInt16, CHUNKS> indices_ = {};

    __m512i base_ = _mm512_set_epi16(
        31, 30, 29, 28, 27, 26, 25, 24,
        23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10, 9, 8,
        7, 6, 5, 4, 3, 2, 1, 0
    );

    USize count_ = 0;

#else

public:
    inline void update(VecUInt8 vec1, VecUInt8 vec2) noexcept {
        const UInt32 mask = (SIMD::nonzeroMask<UInt8>(vec2) << SIMD::CHUNK_SIZE<Int32>) | (SIMD::nonzeroMask<UInt8>(vec1));
        for (UInt32 output = 0; output < OUTPUTS_PER_CHUNK; output++) {
            const UInt32 byte = (mask >> (output * 8)) & 0xFF;
            const Vec128UInt16 nonzero = SIMD::load128UInt16(&NONZERO_INDICES[byte]);
            const Vec128UInt16 indices = SIMD::add128UInt16(base_, nonzero);
            SIMD::usStore128UInt16(&indices_[count_], indices);
            base_ = SIMD::add128UInt16(base_, SIMD::set128UInt16(8));
            count_ += std::popcount(byte);
        }

        assert(count_ <= indexes_.size());
    }

    constexpr USize count() const noexcept { return count_; }
    constexpr USize index(USize idx) const noexcept { return indices_[idx]; }

private:
    static constexpr USize ALIGNMENT = 16;

    alignas(ALIGNMENT) static constexpr MultiArray<UInt16, 256, 8> NONZERO_INDICES = [] {
        MultiArray<UInt16, 256, 8> indices = {};
        for (USize i = 0; i < 256; i++) {
            USize count = 0;
            for (UInt8 v = static_cast<UInt8>(i); v != 0; v &= v - 1) {
                const UInt8 idx = static_cast<UInt8>(std::countr_zero(v));
                indices[i][count++] = idx;
            }
        }
        return indices;
    }();

    static constexpr USize CHUNKS = L1_SIZE / (sizeof(Int32) / sizeof(Int8));
    static constexpr USize CHUNK_SIZE = 2 * SIMD::CHUNK_SIZE<Int32>;
    static constexpr USize OUTPUTS_PER_CHUNK = CHUNK_SIZE / 8;

    alignas(SIMD::ALIGNMENT) std::array<UInt16, CHUNKS> indices_ = {};
    Vec128UInt16 base_ = SIMD::zero128UInt16();
    USize count_ = 0;

#endif

#if defined(MEASURE_SPARSITY)

public:
    static inline void trackFtActs(std::span<const UInt8, L1_SIZE> ftActs) noexcept {
        for (USize i = 0; i < L1_SIZE; i++) {
            if (ftActs[i] != 0) {
                ftActCounts_[i % (L1_SIZE / 2)]++;
            }
        }
    }

    static inline void writeFtActCounts() noexcept {
        std::ofstream file = std::ofstream("ft-acts.txt", std::ios::binary);

        assert(file.is_open());

        bool first = true;
        for (const USize count : ftActCounts_) {
            if (!first) {
                file << ", ";
            }

            first = false;

            file << count;
        }
        file << std::endl;

        file.close();
    }

private:
    static inline std::array<USize, L1_SIZE / 2> ftActCounts_ = {};

#endif

};

template<typename FeatureSet, USize L1_SIZE, Int32 FT_QUANT, Int32 L1_QUANT, typename Activation, typename Output, Int32 SCALE>
class SingleLayerArch {
public:
    static constexpr bool PAIRWISE = false;
    static constexpr bool NEEDS_FT_PERMUTE = false;

private:
    using InputType = std::span<const Int16, L1_SIZE>;

    static constexpr USize OUTPUT_BUCKET_COUNT = Output::BUCKET_COUNT;
    static constexpr USize WEIGHT_SIZE = OUTPUT_BUCKET_COUNT * L1_SIZE * 2;
    static constexpr USize BIAS_SIZE = OUTPUT_BUCKET_COUNT;

    static constexpr Int32 QUANT = FT_QUANT * L1_QUANT;

    NET_PARAM(Int16, WEIGHT_SIZE, l1Weights);
    NET_PARAM(Int16, BIAS_SIZE, l1Biases);

public:
    inline Int32 forward(USize bucket, InputType friendlyPSQInputs, InputType enemyPSQInputs, InputType friendlyThreatInputs, InputType enemyThreatInputs) const noexcept {
        const USize weightOffset = bucket * L1_SIZE * 2;
        const USize biasOffset = bucket;

        Vec<Int32> sum = SIMD::zero<Int32>();
        for (USize i = 0; i < L1_SIZE; i += SIMD::CHUNK_SIZE<Int16>) {
            Vec<Int16> friendlyInputs = SIMD::load<Int16>(&friendlyPSQInputs[i]);
            Vec<Int16> enemyInputs = SIMD::load<Int16>(&enemyPSQInputs[i]);
            if constexpr (FeatureSet::THREAT_INPUTS) {
                friendlyInputs = SIMD::add<Int16>(friendlyInputs, SIMD::load<Int16>(&friendlyThreatInputs[i]));
                enemyInputs = SIMD::add<Int16>(enemyInputs, SIMD::load<Int16>(&enemyThreatInputs[i]));
            }
            const Vec<Int16> friendlyWeights = SIMD::load<Int16>(&l1Weights[weightOffset + i]);
            const Vec<Int16> enemyWeights = SIMD::load<Int16>(&l1Weights[L1_SIZE + weightOffset + i]);
            sum = Activation::template actDotAcc<static_cast<Int16>(FT_QUANT)>(sum, friendlyInputs, friendlyWeights);
            sum = Activation::template actDotAcc<static_cast<Int16>(FT_QUANT)>(sum, enemyInputs, enemyWeights);
        }

        const Int32 total = SIMD::horizAdd<Int32>(sum);
        const Int32 bias = static_cast<Int32>(l1Biases[biasOffset]);
        const Int32 output = bias + Activation::template output<FT_QUANT>(total);
        return output * SCALE / QUANT;
    }

    inline bool load(ByteLoader &loader) noexcept { return loader.load(l1Weights) && loader.load(l1Biases); }

    constexpr USize byteSize() const noexcept { return sizeof(Int16) * WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE; }

    template<typename Type>
    static inline void permuteParam(std::span<Type>) {}

    static inline void permuteFTParams(std::span<const Int16>, std::span<const Int8>, std::span<const Int16>) {}
};

// template<typename FeatureSet, USize L1_SIZE, USize L2_SIZE, USize L3_SIZE, Int32 FT_SCALE_BITS, Int32 FT_QUANT_BITS, Int32 L1_QUANT_BITS, bool DUAL_ACTIVATION, bool SKIP_L2, typename Output, Int32 SCALE>
// class PairwiseMultilayerArch {
// public:

// private:

// public:

// };

}
