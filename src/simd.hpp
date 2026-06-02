#pragma once

#if defined(USE_SSE4)
#include <immintrin.h>
#endif

#if defined(USE_NEON)
#include <arm_neon.h>
#endif

#include <cassert>

#include "arch.hpp"
#include "types.hpp"

#if defined(USE_SSE4) || defined(USE_NEON)
#define USE_SIMD
#endif

namespace Syft {

#if defined(USE_AVX512)
using RegInt16 = __m512i;
using RegInt32 = __m512i;
#elif defined(USE_AVX2)
using RegInt16 = __m256i;
using RegInt32 = __m256i;
#elif defined(USE_SSE4)
using RegInt16 = __m128i;
using RegInt32 = __m128i;
#elif defined(USE_NEON)
using RegInt16 = int16x8_t;
using RegInt32 = int32x4_t;
#endif

class SIMD {
public:

#if defined(USE_SIMD)

#if defined(USE_AVX512)
    static constexpr USize REG_SIZE = 64;
#elif defined(USE_AVX2)
    static constexpr USize REG_SIZE = 32;
#elif defined(USE_SSE4)
    static constexpr USize REG_SIZE = 16;
#elif defined(USE_NEON)
    static constexpr USize REG_SIZE = 16;
#endif

    static constexpr USize WIDTH = REG_SIZE / sizeof(Int16);
    static constexpr USize ITERATIONS = Arch::LAYER_SIZE / WIDTH;

    static inline RegInt16 loadInt16(const Int16 *ptr) noexcept {
#if defined(USE_AVX512)
        return _mm512_load_si512(ptr);
#elif defined(USE_AVX2)
        return _mm256_load_si256(reinterpret_cast<const __m256i *>(ptr));
#elif defined(USE_SSE4)
        return _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
#elif defined(USE_NEON)
        return vld1q_s16(ptr);
#endif
    }

    static inline RegInt32 loadInt32(const Int32 *ptr) noexcept {
#if defined(USE_AVX512)
        return _mm512_load_si512(ptr);
#elif defined(USE_AVX2)
        return _mm256_load_si256(reinterpret_cast<const __m256i *>(ptr));
#elif defined(USE_SSE4)
        return _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
#elif defined(USE_NEON)
        return vld1q_s32(ptr);
#endif
    }

    static inline void storeInt16(Int16 *ptr, RegInt16 reg) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, reg);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), reg);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), reg);
#elif defined(USE_NEON)
        vst1q_s16(ptr, reg);
#endif
    }

    static inline void storeInt32(Int32 *ptr, RegInt32 reg) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, reg);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), reg);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), reg);
#elif defined(USE_NEON)
        vst1q_s32(ptr, reg);
#endif
    }

    static inline RegInt16 zeroInt16() noexcept {
#if defined(USE_AVX512)
        return _mm512_setzero_si512();
#elif defined(USE_AVX2)
        return _mm256_setzero_si256();
#elif defined(USE_SSE4)
        return _mm_setzero_si128();
#elif defined(USE_NEON)
        return vdupq_n_s16(0);
#endif
    }

    static inline RegInt32 zeroInt32() noexcept {
#if defined(USE_AVX512)
        return _mm512_setzero_si512();
#elif defined(USE_AVX2)
        return _mm256_setzero_si256();
#elif defined(USE_SSE4)
        return _mm_setzero_si128();
#elif defined(USE_NEON)
        return vdupq_n_s32(0);
#endif
    }

    static inline RegInt16 setInt16(Int16 val) noexcept {
#if defined(USE_AVX512)
        return _mm512_set1_epi16(val);
#elif defined(USE_AVX2)
        return _mm256_set1_epi16(val);
#elif defined(USE_SSE4)
        return _mm_set1_epi16(val);
#elif defined(USE_NEON)
        return vdupq_n_s16(val);
#endif
    }

    static inline RegInt32 setInt32(Int32 val) noexcept {
#if defined(USE_AVX512)
        return _mm512_set1_epi32(val);
#elif defined(USE_AVX2)
        return _mm256_set1_epi32(val);
#elif defined(USE_SSE4)
        return _mm_set1_epi32(val);
#elif defined(USE_NEON)
        return vdupq_n_s32(val);
#endif
    }

    static inline RegInt16 addInt16(RegInt16 reg1, RegInt16 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_add_epi16(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_add_epi16(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_add_epi16(reg1, reg2);
#elif defined(USE_NEON)
        return vaddq_s16(reg1, reg2);
#endif
    }

    static inline RegInt32 addInt32(RegInt32 reg1, RegInt32 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_add_epi32(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_add_epi32(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_add_epi32(reg1, reg2);
#elif defined(USE_NEON)
        return vaddq_s32(reg1, reg2);
#endif
    }

    static inline RegInt16 subInt16(RegInt16 reg1, RegInt16 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_sub_epi16(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_sub_epi16(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_sub_epi16(reg1, reg2);
#elif defined(USE_NEON)
        return vsubq_s16(reg1, reg2);
#endif
    }

    static inline RegInt32 subInt32(RegInt32 reg1, RegInt32 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_sub_epi32(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_sub_epi32(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_sub_epi32(reg1, reg2);
#elif defined(USE_NEON)
        return vsubq_s32(reg1, reg2);
#endif
    }

    static inline RegInt16 clampInt16(RegInt16 reg, RegInt16 regMin, RegInt16 regMax) noexcept {
#if defined(USE_AVX512)
        return _mm512_min_epi16(_mm512_max_epi16(reg, regMin), regMax);
#elif defined(USE_AVX2)
        return _mm256_min_epi16(_mm256_max_epi16(reg, regMin), regMax);
#elif defined(USE_SSE4)
        return _mm_min_epi16(_mm_max_epi16(reg, regMin), regMax);
#elif defined(USE_NEON)
        return vminq_s16(vmaxq_s16(reg, regMin), regMax);
#endif
    }

    static inline RegInt32 clampInt32(RegInt32 reg, RegInt32 regMin, RegInt32 regMax) noexcept {
#if defined(USE_AVX512)
        return _mm512_min_epi32(_mm512_max_epi32(reg, regMin), regMax);
#elif defined(USE_AVX2)
        return _mm256_min_epi32(_mm256_max_epi32(reg, regMin), regMax);
#elif defined(USE_SSE4)
        return _mm_min_epi32(_mm_max_epi32(reg, regMin), regMax);
#elif defined(USE_NEON)
        return vminq_s32(vmaxq_s32(reg, regMin), regMax);
#endif
    }

    static inline RegInt16 mulLoInt16(RegInt16 reg1, RegInt16 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_mullo_epi16(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_mullo_epi16(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_mullo_epi16(reg1, reg2);
#elif defined(USE_NEON)
        return vmulq_s16(reg1, reg2);
#endif
    }

    static inline RegInt32 mulAddInt16(RegInt16 reg1, RegInt16 reg2) noexcept {
#if defined(USE_AVX512)
        return _mm512_madd_epi16(reg1, reg2);
#elif defined(USE_AVX2)
        return _mm256_madd_epi16(reg1, reg2);
#elif defined(USE_SSE4)
        return _mm_madd_epi16(reg1, reg2);
#elif defined(USE_NEON)
        int32x4_t lo = vmull_s16(vget_low_s16(reg1), vget_low_s16(reg2));
        int32x4_t hi = vmull_s16(vget_high_s16(reg1), vget_high_s16(reg2));
        return vpaddq_s32(lo, hi);
#endif
    }

    static inline Int32 horizAddInt32(RegInt32 reg) noexcept {
#if defined(USE_AVX512)
        __m256i lo256 = _mm512_castsi512_si256(reg);
        __m256i hi256 = _mm512_extracti64x4_epi64(reg, 1);
        __m256i sum256 = _mm256_add_epi32(lo256, hi256);
        __m128i lo128 = _mm256_castsi256_si128(sum256);
        __m128i hi128 = _mm256_extracti128_si256(sum256, 1);
        __m128i sum128 = _mm_add_epi32(lo128, hi128);
        __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
        __m128i sum64 = _mm_add_epi32(hi64, sum128);
        __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
#elif defined(USE_AVX2)
        __m128i lo128 = _mm256_castsi256_si128(reg);
        __m128i hi128 = _mm256_extracti128_si256(reg, 1);
        __m128i sum128 = _mm_add_epi32(lo128, hi128);
        __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
        __m128i sum64 = _mm_add_epi32(hi64, sum128);
        __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
#elif defined(USE_SSE4)
        __m128i hi64 = _mm_unpackhi_epi64(reg, reg);
        __m128i sum64 = _mm_add_epi32(hi64, reg);
        __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
#elif defined(USE_NEON)
        return vaddvq_s32(reg);
#endif
    }

#endif

};

}
