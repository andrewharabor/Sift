#pragma once

#include "arch.hpp"
#include "types.hpp"

#if defined(USE_SSE4) || defined(USE_NEON)
#define USE_SIMD
#endif

namespace Syft {

#if defined(USE_AVX512)
using VecInt16 = __m512i;
using VecInt32 = __m512i;
#elif defined(USE_AVX2)
using VecInt16 = __m256i;
using VecInt32 = __m256i;
#elif defined(USE_SSE4)
using VecInt16 = __m128i;
using VecInt32 = __m128i;
#elif defined(USE_NEON)
using VecInt16 = int16x8_t;
using VecInt32 = int32x4_t;
#endif

class SIMD {
public:

#if defined(USE_SIMD)

#if defined(USE_AVX512)
    static constexpr USize VEC_SIZE = 64;
#elif defined(USE_AVX2)
    static constexpr USize VEC_SIZE = 32;
#elif defined(USE_SSE4)
    static constexpr USize VEC_SIZE = 16;
#elif defined(USE_NEON)
    static constexpr USize VEC_SIZE = 16;
#endif

    static constexpr VecInt16 loadInt16(const Int16 *ptr) noexcept {
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

    static constexpr VecInt32 loadInt32(const Int32 *ptr) noexcept {
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

    static constexpr void storeInt16(Int16 *ptr, VecInt16 reg) noexcept {
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

    static constexpr void storeInt32(Int32 *ptr, VecInt32 reg) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, reg);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), reg);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), reg);
#elif defined(USE_NEON)
        vst1q_s32(ptr, reg);
#endif


        static constexpr VecInt16 zeroInt16() noexcept {
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

        static constexpr VecInt32 zeroInt32() noexcept {
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

        static constexpr VecInt16 set1Int16(Int16 val) noexcept {
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

        static constexpr VecInt32 set1Int32(Int32 val) noexcept {
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

        static constexpr VecInt16 addInt16(VecInt16 reg1, VecInt16 reg2) noexcept {
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

        static constexpr VecInt32 addInt32(VecInt32 reg1, VecInt32 reg2) noexcept {
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

        static constexpr VecInt16 subInt16(VecInt16 reg1, VecInt16 reg2) noexcept {
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

        static constexpr VecInt32 subInt32(VecInt32 reg1, VecInt32 reg2) noexcept {
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

        // TODO: clamp

        static constexpr VecInt16 mulLoInt16(VecInt16 reg1, VecInt16 reg2) noexcept {
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

        static constexpr VecInt32 mulLoInt32(VecInt32 reg1, VecInt32 reg2) noexcept {
#if defined(USE_AVX512)
            return _mm512_mullo_epi32(reg1, reg2);
#elif defined(USE_AVX2)
            return _mm256_mullo_epi32(reg1, reg2);
#elif defined(USE_SSE4)
            return _mm_mullo_epi32(reg1, reg2);
#elif defined(USE_NEON)
            return vmulq_s32(reg1, reg2);
#endif
        }

        static constexpr VecInt16 mulHiInt16(VecInt16 reg1, VecInt16 reg2) noexcept {
#if defined(USE_AVX512)
            return _mm512_mulhi_epi16(reg1, reg2);
#elif defined(USE_AVX2)
            return _mm256_mulhi_epi16(reg1, reg2);
#elif defined(USE_SSE4)
            return _mm_mulhi_epi16(reg1, reg2);
#elif defined(USE_NEON)
            return vqdmulhq_s16(reg1, reg2);
#endif
        }

        static constexpr VecInt32 mulAddInt16(VecInt16 reg1, VecInt16 reg2, VecInt16 reg3) noexcept {
#if defined(USE_AVX512)
            return _mm512_madd_epi16(reg1, reg2, reg3);
#elif defined(USE_AVX2)
            return _mm256_madd_epi16(reg1, reg2, reg3);
#elif defined(USE_SSE4)
            return _mm_madd_epi16(reg1, reg2, reg3);
#elif defined(USE_NEON)
            return vmlal_s16(vget_low_s16(reg1), vget_low_s16(reg2), vget_low_s16(reg3));
#endif
        }

#endif

    };

}
