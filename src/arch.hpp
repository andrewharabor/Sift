#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <fstream>
#include <span>

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

}
