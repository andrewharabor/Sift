#pragma once

#if defined(USE_SSE4)
#include <immintrin.h>
#endif

#if defined(USE_NEON)
#include <arm_neon.h>
#endif

#include <bit>
#include <cassert>

#include "arch.hpp"
#include "types.hpp"

#if defined(USE_SSE4) || defined(USE_NEON)
#define USE_SIMD
#endif


namespace Sift {

#if defined(USE_AVX512)
using VecUInt8 = __m512i;
using VecInt8 = __m512i;
using VecInt16 = __m512i;
using VecInt32 = __m512i;
#elif defined(USE_AVX2)
using VecUInt8 = __m256i;
using VecInt8 = __m256i;
using VecInt16 = __m256i;
using VecInt32 = __m256i;
#elif defined(USE_SSE4)
using VecUInt8 = __m128i;
using VecInt8 = __m128i;
using VecInt16 = __m128i;
using VecInt32 = __m128i;
#elif defined(USE_NEON)
using VecUInt8 = uint8x16_t;
using VecInt8 = int8x16_t;
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

    static constexpr USize WIDTH8 = VEC_SIZE / sizeof(Int8);
    static constexpr USize WIDTH16 = VEC_SIZE / sizeof(Int16);
    static constexpr USize WIDTH32 = VEC_SIZE / sizeof(Int32);

    static inline VecUInt8 loadUInt8(const UInt8 *ptr) noexcept {
#if defined(USE_AVX512)
        return _mm512_load_si512(ptr);
#elif defined(USE_AVX2)
        return _mm256_load_si256(reinterpret_cast<const __m256i *>(ptr));
#elif defined(USE_SSE4)
        return _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
#elif defined(USE_NEON)
        return vld1q_u8(ptr);
#endif
    }

    static inline VecInt8 loadInt8(const Int8 *ptr) noexcept {
#if defined(USE_AVX512)
        return _mm512_load_si512(ptr);
#elif defined(USE_AVX2)
        return _mm256_load_si256(reinterpret_cast<const __m256i *>(ptr));
#elif defined(USE_SSE4)
        return _mm_load_si128(reinterpret_cast<const __m128i *>(ptr));
#elif defined(USE_NEON)
        return vld1q_s8(ptr);
#endif
    }

    static inline VecInt16 loadInt16(const Int16 *ptr) noexcept {
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

    static inline VecInt32 loadInt32(const Int32 *ptr) noexcept {
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

    static inline void storeUInt8(UInt8 *ptr, VecUInt8 vec) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, vec);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), vec);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), vec);
#elif defined(USE_NEON)
        vst1q_u8(ptr, vec);
#endif
    }

    static inline void storeInt8(Int8 *ptr, VecInt8 vec) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, vec);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), vec);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), vec);
#elif defined(USE_NEON)
        vst1q_s8(ptr, vec);
#endif
    }

    static inline void storeInt16(Int16 *ptr, VecInt16 vec) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, vec);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), vec);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), vec);
#elif defined(USE_NEON)
        vst1q_s16(ptr, vec);
#endif
    }

    static inline void storeInt32(Int32 *ptr, VecInt32 vec) noexcept {
#if defined(USE_AVX512)
        _mm512_store_si512(ptr, vec);
#elif defined(USE_AVX2)
        _mm256_store_si256(reinterpret_cast<__m256i *>(ptr), vec);
#elif defined(USE_SSE4)
        _mm_store_si128(reinterpret_cast<__m128i *>(ptr), vec);
#elif defined(USE_NEON)
        vst1q_s32(ptr, vec);
#endif
    }

    static inline VecInt16 zeroInt16() noexcept {
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

    static inline VecInt32 zeroInt32() noexcept {
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

    static inline VecInt16 setInt16(Int16 val) noexcept {
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

    static inline VecInt32 setInt32(Int32 val) noexcept {
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

    static inline VecInt16 addInt16(VecInt16 vecA, VecInt16 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_add_epi16(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_add_epi16(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_add_epi16(vecA, vecB);
#elif defined(USE_NEON)
        return vaddq_s16(vecA, vecB);
#endif
    }

    static inline VecInt32 addInt32(VecInt32 vecA, VecInt32 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_add_epi32(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_add_epi32(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_add_epi32(vecA, vecB);
#elif defined(USE_NEON)
        return vaddq_s32(vecA, vecB);
#endif
    }

    static inline VecInt16 subInt16(VecInt16 vecA, VecInt16 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_sub_epi16(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_sub_epi16(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_sub_epi16(vecA, vecB);
#elif defined(USE_NEON)
        return vsubq_s16(vecA, vecB);
#endif
    }

    static inline VecInt32 subInt32(VecInt32 vecA, VecInt32 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_sub_epi32(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_sub_epi32(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_sub_epi32(vecA, vecB);
#elif defined(USE_NEON)
        return vsubq_s32(vecA, vecB);
#endif
    }

    static inline VecInt16 clampInt16(VecInt16 vec, VecInt16 minVec, VecInt16 maxVec) noexcept {
#if defined(USE_AVX512)
        return _mm512_min_epi16(_mm512_max_epi16(vec, minVec), maxVec);
#elif defined(USE_AVX2)
        return _mm256_min_epi16(_mm256_max_epi16(vec, minVec), maxVec);
#elif defined(USE_SSE4)
        return _mm_min_epi16(_mm_max_epi16(vec, minVec), maxVec);
#elif defined(USE_NEON)
        return vminq_s16(vmaxq_s16(vec, minVec), maxVec);
#endif
    }

    static inline VecInt32 clampInt32(VecInt32 vec, VecInt32 minVec, VecInt32 maxVec) noexcept {
#if defined(USE_AVX512)
        return _mm512_min_epi32(_mm512_max_epi32(vec, minVec), maxVec);
#elif defined(USE_AVX2)
        return _mm256_min_epi32(_mm256_max_epi32(vec, minVec), maxVec);
#elif defined(USE_SSE4)
        return _mm_min_epi32(_mm_max_epi32(vec, minVec), maxVec);
#elif defined(USE_NEON)
        return vminq_s32(vmaxq_s32(vec, minVec), maxVec);
#endif
    }

    static inline VecInt16 lShiftInt16(VecInt16 vec, Int32 shift) noexcept {
#if defined(USE_AVX512)
        return _mm512_slli_epi16(vec, shift);
#elif defined(USE_AVX2)
        return _mm256_slli_epi16(vec, shift);
#elif defined(USE_SSE4)
        return _mm_slli_epi16(vec, shift);
#elif defined(USE_NEON)
        return vshlq_s16(vec, vdupq_n_s16(static_cast<Int16>(shift)));
#endif
    }

    static inline VecInt32 lShiftInt32(VecInt32 vec, Int32 shift) noexcept {
#if defined(USE_AVX512)
        return _mm512_slli_epi32(vec, shift);
#elif defined(USE_AVX2)
        return _mm256_slli_epi32(vec, shift);
#elif defined(USE_SSE4)
        return _mm_slli_epi32(vec, shift);
#elif defined(USE_NEON)
        return vshlq_s32(vec, vdupq_n_s32(shift));
#endif
    }

    static inline VecInt16 rShiftInt16(VecInt16 vec, Int32 shift) noexcept {
#if defined(USE_AVX512)
        return _mm512_srai_epi16(vec, shift);
#elif defined(USE_AVX2)
        return _mm256_srai_epi16(vec, shift);
#elif defined(USE_SSE4)
        return _mm_srai_epi16(vec, shift);
#elif defined(USE_NEON)
        return vshlq_s16(vec, vdupq_n_s16(static_cast<Int16>(-shift)));
#endif
    }

    static inline VecInt32 rShiftInt32(VecInt32 vec, Int32 shift) noexcept {
#if defined(USE_AVX512)
        return _mm512_srai_epi32(vec, shift);
#elif defined(USE_AVX2)
        return _mm256_srai_epi32(vec, shift);
#elif defined(USE_SSE4)
        return _mm_srai_epi32(vec, shift);
#elif defined(USE_NEON)
        return vshlq_s32(vec, vdupq_n_s32(-shift));
#endif
    }

    static inline VecInt16 mulLoInt16(VecInt16 vecA, VecInt16 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_mullo_epi16(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_mullo_epi16(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_mullo_epi16(vecA, vecB);
#elif defined(USE_NEON)
        return vmulq_s16(vecA, vecB);
#endif
    }

    static inline VecInt32 mulLoInt32(VecInt32 vecA, VecInt32 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_mullo_epi32(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_mullo_epi32(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_mullo_epi32(vecA, vecB);
#elif defined(USE_NEON)
        return vmulq_s32(vecA, vecB);
#endif
    }

    static inline VecInt32 mulAddInt16(VecInt16 vecA, VecInt16 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_madd_epi16(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_madd_epi16(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_madd_epi16(vecA, vecB);
#elif defined(USE_NEON)
        int32x4_t lo = vmull_s16(vget_low_s16(vecA), vget_low_s16(vecB));
        int32x4_t hi = vmull_s16(vget_high_s16(vecA), vget_high_s16(vecB));
        return vpaddq_s32(lo, hi);
#endif
    }

    static inline VecInt16 lShiftMulHiInt16(VecInt16 vecA, VecInt16 vecB, Int32 shift) noexcept {
#if defined(USE_AVX512)
        return _mm512_mulhi_epi16(_mm512_slli_epi16(vecA, shift), vecB);
#elif defined(USE_AVX2)
        return _mm256_mulhi_epi16(_mm256_slli_epi16(vecA, shift), vecB);
#elif defined(USE_SSE4)
        return _mm_mulhi_epi16(_mm_slli_epi16(vecA, shift), vecB);
#elif defined(USE_NEON)
        return vqdmulhq_s16(vshlq_s16(vecA, vdupq_n_s16(static_cast<Int16>(shift - 1))), vecB);
#endif
    }

    static inline VecUInt8 packUsInt16(VecInt16 vecA, VecInt16 vecB) noexcept {
#if defined(USE_AVX512)
        return _mm512_packus_epi16(vecA, vecB);
#elif defined(USE_AVX2)
        return _mm256_packus_epi16(vecA, vecB);
#elif defined(USE_SSE4)
        return _mm_packus_epi16(vecA, vecB);
#elif defined(USE_NEON)
        return vcombine_u8(vqmovun_s16(vecA), vqmovun_s16(vecB));
#endif
    }

    static inline VecUInt8 tileUInt8(const UInt8 *ptr) noexcept {
#if defined(USE_AVX512)
        return _mm512_set1_epi32(*reinterpret_cast<const UInt32 *>(ptr));
#elif defined(USE_AVX2)
        return _mm256_set1_epi32(*reinterpret_cast<const UInt32 *>(ptr));
#elif defined(USE_SSE4)
        return _mm_set1_epi32(*reinterpret_cast<const UInt32 *>(ptr));
#elif defined(USE_NEON)
        return vreinterpretq_u8_u32(vdupq_n_u32(*reinterpret_cast<const UInt32 *>(ptr)));
#endif
    }

    static inline VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 vecU, VecInt8 vecI) noexcept {
#if defined(USE_AVX512_VNNI)
        return _mm512_dpbusd_epi32(sum, vecU, vecI);
#elif defined(USE_AVX512)
        return _mm512_add_epi32(sum, _mm512_madd_epi16(_mm512_maddubs_epi16(vecU, vecI), _mm512_set1_epi16(1)));
#elif defined(USE_AVX2)
        return _mm256_add_epi32(sum, _mm256_madd_epi16(_mm256_maddubs_epi16(vecU, vecI), _mm256_set1_epi16(1)));
#elif defined(USE_SSE4)
        return _mm_add_epi32(sum, _mm_madd_epi16(_mm_maddubs_epi16(vecU, vecI), _mm_set1_epi16(1)));
#elif defined(USE_NEON_DOTPROD)
        return vdotq_s32(sum, vreinterpretq_u8_s8(vecU), vecI);
#elif defined(USE_NEON)
        const int8x16_t iVecU = vreinterpretq_s8_u8(vecU);
        const int16x8_t lo = vmull_s8(vget_low_s8(iVecU), vget_low_s8(vecI));
        const int16x8_t hi = vmull_high_s8(iVecU, vecI);
        return vpadalq_s16(sum, vpaddq_s16(lo, hi));
#endif
    }

    static inline UInt32 nonzeroMaskUInt8(VecUInt8 vec) noexcept {
#if defined(USE_AVX512)
        return _mm512_cmpgt_epi32_mask(vec, _mm512_setzero_si512());
#elif defined(USE_AVX2)
        return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(vec, _mm256_setzero_si256())));
#elif defined(USE_SSE4)
        return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(vec, _mm_setzero_si128())));
#elif defined(USE_NEON)
        alignas(64) static constexpr std::array<UInt32, 4> BITS = {1, 2, 4, 8};
        return vaddvq_u32(vandq_u32(vtstq_u32(vreinterpretq_u32_u8(vec), vreinterpretq_u32_u8(vec)), vld1q_u32(BITS.data())));
#endif
    }

    static inline Int32 horizAddInt32(VecInt32 vec) noexcept {
#if defined(USE_AVX512)
        __m256i lo256 = _mm512_castsi512_si256(vec);
        __m256i hi256 = _mm512_extracti64x4_epi64(vec, 1);
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
        __m128i lo128 = _mm256_castsi256_si128(vec);
        __m128i hi128 = _mm256_extracti128_si256(vec, 1);
        __m128i sum128 = _mm_add_epi32(lo128, hi128);
        __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
        __m128i sum64 = _mm_add_epi32(hi64, sum128);
        __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
#elif defined(USE_SSE4)
        __m128i hi64 = _mm_unpackhi_epi64(vec, vec);
        __m128i sum64 = _mm_add_epi32(hi64, vec);
        __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
#elif defined(USE_NEON)
        return vaddvq_s32(vec);
#endif
    }

#endif
};

class FusedUpdates {
public:

    static inline void add1(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &add1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add1[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] += add1[i];
        }
#endif
    }

    static inline void sub1(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &sub1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub1[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] -= sub1[i];
        }
#endif
    }

    static inline void add2(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &add1, const std::array<Int16, Arch::L1_SIZE> &add2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add2[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add2[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add2[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add2[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] += add1[i] + add2[i];
        }
#endif
    }

    static inline void sub2(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &sub1, const std::array<Int16, Arch::L1_SIZE> &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub2[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub2[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub2[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub2[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] -= sub1[i] + sub2[i];
        }
#endif
    }

    static inline void add1Sub1(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &add1, const std::array<Int16, Arch::L1_SIZE> &sub1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub1[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] += add1[i] - sub1[i];
        }
#endif
    }

    static inline void add1Sub2(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &add1, const std::array<Int16, Arch::L1_SIZE> &sub1, const std::array<Int16, Arch::L1_SIZE> &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub2[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub2[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub2[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub2[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] += add1[i] - sub1[i] - sub2[i];
        }
#endif
    }

    static inline void add2Sub2(std::array<Int16, Arch::L1_SIZE> &data, const std::array<Int16, Arch::L1_SIZE> &add1, const std::array<Int16, Arch::L1_SIZE> &add2, const std::array<Int16, Arch::L1_SIZE> &sub1, const std::array<Int16, Arch::L1_SIZE> &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::L1_SIZE % (SIMD::WIDTH16 * 4) == 0);
        static constexpr USize ITERS4 = Arch::L1_SIZE / SIMD::WIDTH16;
        for (USize i = 0; i < ITERS4; i += 4) {
            VecInt16 dataVec0 = SIMD::loadInt16(&data[(i + 0) * SIMD::WIDTH16]);
            VecInt16 dataVecA = SIMD::loadInt16(&data[(i + 1) * SIMD::WIDTH16]);
            VecInt16 dataVecB = SIMD::loadInt16(&data[(i + 2) * SIMD::WIDTH16]);
            VecInt16 dataVec3 = SIMD::loadInt16(&data[(i + 3) * SIMD::WIDTH16]);
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::addInt16(dataVec0, SIMD::loadInt16(&add2[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::addInt16(dataVecA, SIMD::loadInt16(&add2[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::addInt16(dataVecB, SIMD::loadInt16(&add2[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::addInt16(dataVec3, SIMD::loadInt16(&add2[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub1[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub1[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub1[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub1[(i + 3) * SIMD::WIDTH16]));
            dataVec0 = SIMD::subInt16(dataVec0, SIMD::loadInt16(&sub2[(i + 0) * SIMD::WIDTH16]));
            dataVecA = SIMD::subInt16(dataVecA, SIMD::loadInt16(&sub2[(i + 1) * SIMD::WIDTH16]));
            dataVecB = SIMD::subInt16(dataVecB, SIMD::loadInt16(&sub2[(i + 2) * SIMD::WIDTH16]));
            dataVec3 = SIMD::subInt16(dataVec3, SIMD::loadInt16(&sub2[(i + 3) * SIMD::WIDTH16]));
            SIMD::storeInt16(&data[(i + 0) * SIMD::WIDTH16], dataVec0);
            SIMD::storeInt16(&data[(i + 1) * SIMD::WIDTH16], dataVecA);
            SIMD::storeInt16(&data[(i + 2) * SIMD::WIDTH16], dataVecB);
            SIMD::storeInt16(&data[(i + 3) * SIMD::WIDTH16], dataVec3);
        }
#else
        for (USize i = 0; i < Arch::L1_SIZE; i++) {
            data[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
#endif
    }
};

}
