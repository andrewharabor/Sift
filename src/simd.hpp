#pragma once

#if defined(USE_AVX2) || defined(USE_AVX512)
#include <immintrin.h>
#endif

#if defined(USE_NEON)
#include <arm_neon.h>
#endif

#include <array>
#include <bit>
#include <cassert>

#include "types.hpp"


namespace Sift {

#if defined(USE_AVX512)

using VecUInt8 = __m512i;
using VecUInt16 = __m512i;
using VecInt8 = __m512i;
using VecInt16 = __m512i;
using VecInt32 = __m512i;

#elif defined(USE_AVX2)

using VecUInt8 = __m256i;
using VecUInt16 = __m256i;
using VecInt8 = __m256i;
using VecInt16 = __m256i;
using VecInt32 = __m256i;

#elif defined(USE_NEON)

using VecUInt8 = uint8x16_t;
using VecUInt16 = uint16x8_t;
using VecInt8 = int8x16_t;
using VecInt16 = int16x8_t;
using VecInt32 = int32x4_t;

#endif

class SIMD {

public:

#if defined(USE_AVX512)

    static constexpr std::uintptr_t ALIGNMENT = sizeof(__m512i);

    static constexpr bool PACK_REORDER = true;
    static constexpr USize PACK_GROUPING = 8;
    static constexpr USize PACK_SIZE = 8;
    static constexpr std::array<USize, PACK_SIZE> PACK_ORDERING = {0, 2, 4, 6, 1, 3, 5, 7};

    static inline VecUInt8 loadUInt8(const void *ptr) noexcept { return _mm512_load_si512(ptr); }
    static inline void storeUInt8(void *ptr, VecUInt8 v) noexcept { _mm512_store_si512(ptr, v); }
    static inline VecUInt8 zeroUInt8() noexcept { return _mm512_setzero_si512(); }

    static inline VecUInt16 loadUInt16(const void *ptr) noexcept { return _mm512_load_si512(ptr); }
    static inline void storeUInt16(void *ptr, VecUInt16 v) noexcept { _mm512_store_si512(ptr, v); }
    static inline VecUInt16 zeroUInt16() noexcept { return _mm512_setzero_si512(); }

    static inline VecInt8 loadInt8(const void *ptr) noexcept { return _mm512_load_si512(ptr); }
    static inline VecInt16 widenLoadInt8(const void *ptr) noexcept { return _mm512_cvtepi8_epi16(_mm256_loadu_si256(static_cast<const __m256i *>(ptr))); }
    static inline void storeInt8(void *ptr, VecInt8 v) noexcept { _mm512_store_si512(ptr, v); }
    static inline VecInt8 zeroInt8() noexcept { return _mm512_setzero_si512(); }
    static inline VecInt8 setInt8(Int8 v) noexcept { return _mm512_set1_epi8(v); }
    static inline VecInt8 minInt8(VecInt8 a, VecInt8 b) noexcept { return _mm512_min_epi8(a, b); }
    static inline VecInt8 maxInt8(VecInt8 a, VecInt8 b) noexcept { return _mm512_max_epi8(a, b); }
    static inline VecInt8 clampInt8(VecInt8 v, VecInt8 min, VecInt8 max) noexcept { return minInt8(maxInt8(v, min), max); }
    static inline VecInt8 addInt8(VecInt8 a, VecInt8 b) noexcept { return _mm512_add_epi8(a, b); }
    static inline VecInt8 subInt8(VecInt8 a, VecInt8 b) noexcept { return _mm512_sub_epi8(a, b); }
    static inline VecInt8 shiftLeftInt8(VecInt8 v, Int32 shift) noexcept { std::terminate(); }

    static inline VecInt16 loadInt16(const void *ptr) noexcept { return _mm512_load_si512(ptr); }
    static inline void storeInt16(void *ptr, VecInt16 v) noexcept { _mm512_store_si512(ptr, v); }
    static inline VecInt16 zeroInt16() noexcept { return _mm512_setzero_si512(); }
    static inline VecInt16 setInt16(Int16 v) noexcept { return _mm512_set1_epi16(v); }
    static inline VecInt16 minInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_min_epi16(a, b); }
    static inline VecInt16 maxInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_max_epi16(a, b); }
    static inline VecInt16 clampInt16(VecInt16 v, VecInt16 min, VecInt16 max) noexcept { return minInt16(maxInt16(v, min), max); }
    static inline VecInt16 addInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_add_epi16(a, b); }
    static inline VecInt16 subInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_sub_epi16(a, b); }
    static inline VecInt16 mulLoInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_mullo_epi16(a, b); }
    static inline VecInt16 shiftLeftInt16(VecInt16 v, Int32 shift) noexcept { return _mm512_slli_epi16(v, shift); }
    static inline VecInt16 shiftRightInt16(VecInt16 v, Int32 shift) noexcept { return _mm512_srai_epi16(v, shift); }
    static inline VecInt16 shiftLeftMulHiInt16(VecInt16 a, VecInt16 b, Int32 shift) noexcept { return _mm512_mulhi_epi16(_mm512_slli_epi16(a, shift), b); }
    static inline VecInt32 mulAddAdjInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_madd_epi16(a, b); }
    static inline VecUInt8 packUsInt16(VecInt16 a, VecInt16 b) noexcept { return _mm512_packus_epi16(a, b); }

    static inline VecInt32 loadInt32(const void *ptr) noexcept { return _mm512_load_si512(ptr); }
    static inline void storeInt32(void *ptr, VecInt32 v) noexcept { _mm512_store_si512(ptr, v); }
    static inline VecInt32 zeroInt32() noexcept { return _mm512_setzero_si512(); }
    static inline VecInt32 setInt32(Int32 v) noexcept { return _mm512_set1_epi32(v); }
    static inline VecInt32 minInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_min_epi32(a, b); }
    static inline VecInt32 maxInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_max_epi32(a, b); }
    static inline VecInt32 clampInt32(VecInt32 v, VecInt32 min, VecInt32 max) noexcept { return minInt32(maxInt32(v, min), max); }
    static inline VecInt32 addInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_add_epi32(a, b); }
    static inline VecInt32 subInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_sub_epi32(a, b); }
    static inline VecInt32 mulLoInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_mullo_epi32(a, b); }
    static inline VecInt32 shiftLeftInt32(VecInt32 v, Int32 shift) noexcept { return _mm512_slli_epi32(v, shift); }
    static inline VecInt32 shiftRightInt32(VecInt32 v, Int32 shift) noexcept { return _mm512_srai_epi32(v, shift); }
    static inline VecUInt16 packUsInt32(VecInt32 a, VecInt32 b) noexcept { return _mm512_packus_epi32(a, b); }

    static inline Int32 horizAddInt32(VecInt32 v) noexcept { return _mm512_reduce_add_epi32(v); }

    static inline UInt32 nonzeroMaskUInt8(VecUInt8 v) { return _mm512_cmpneq_epi32_mask(v, _mm512_setzero_si512()); }

    static inline VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) noexcept {
#if defined(USE_VNNI512)
        return _mm512_dpbusd_epi32(sum, u, i);
#else
        const auto p = _mm512_maddubs_epi16(u, i);
        const auto w = _mm512_madd_epi16(p, _mm512_set1_epi16(1));
        return _mm512_add_epi32(sum, w);
#endif
    }

    static inline VecInt32 dotProdInt16(VecInt32 sum, VecInt16 a, VecInt16 b) noexcept {
#if defined(USE_VNNI512)
        return _mm512_dpwssd_epi32(sum, a, b);
#else
        const auto prod = mulAddAdjInt16(a, b);
        return addInt32(sum, prod);
#endif
    }

#elif defined(USE_AVX2)

    static constexpr std::uintptr_t ALIGNMENT = sizeof(__m256i);

    static constexpr bool PACK_REORDER = true;
    static constexpr USize PACK_GROUPING = 8;
    static constexpr USize PACK_SIZE = 4;
    static constexpr std::array<USize, PACK_SIZE> PACK_ORDERING = {0, 2, 1, 3};

    static inline VecUInt8 loadUInt8(const void *ptr) noexcept { return _mm256_load_si256(static_cast<const VecUInt8 *>(ptr)); }
    static inline void storeUInt8(void *ptr, VecUInt8 v) noexcept { _mm256_store_si256(static_cast<VecUInt8 *>(ptr), v); }
    static inline VecUInt8 zeroUInt8() noexcept { return _mm256_setzero_si256(); }

    static inline VecUInt16 loadUInt16(const void *ptr) noexcept { return _mm256_load_si256(static_cast<const VecUInt16 *>(ptr)); }
    static inline void storeUInt16(void *ptr, VecUInt16 v) noexcept { _mm256_store_si256(static_cast<VecUInt16 *>(ptr), v); }
    static inline VecUInt16 zeroUInt16() noexcept { return _mm256_setzero_si256(); }

    static inline VecInt8 loadInt8(const void *ptr) noexcept { return _mm256_load_si256(static_cast<const VecInt8 *>(ptr)); }
    static inline VecInt16 widenLoadInt8(const void *ptr) noexcept { return _mm256_cvtepi8_epi16(_mm_loadu_si128(static_cast<const __m128i *>(ptr))); }
    static inline void storeInt8(void *ptr, VecInt8 v) noexcept { _mm256_store_si256(static_cast<VecInt8 *>(ptr), v); }
    static inline VecInt8 zeroInt8() noexcept { return _mm256_setzero_si256(); }
    static inline VecInt8 setInt8(Int8 v) noexcept { return _mm256_set1_epi8(v); }
    static inline VecInt8 minInt8(VecInt8 a, VecInt8 b) noexcept { return _mm256_min_epi8(a, b); }
    static inline VecInt8 maxInt8(VecInt8 a, VecInt8 b) noexcept { return _mm256_max_epi8(a, b); }
    static inline VecInt8 clampInt8(VecInt8 v, VecInt8 min, VecInt8 max) noexcept { return minInt8(maxInt8(v, min), max); }
    static inline VecInt8 addInt8(VecInt8 a, VecInt8 b) noexcept { return _mm256_add_epi8(a, b); }
    static inline VecInt8 subInt8(VecInt8 a, VecInt8 b) noexcept { return _mm256_sub_epi8(a, b); }
    static inline VecInt8 shiftLeftInt8(VecInt8 v, Int32 shift) noexcept { std::terminate(); }

    static inline VecInt16 loadInt16(const void *ptr) noexcept { return _mm256_load_si256(static_cast<const VecInt16 *>(ptr)); }
    static inline void storeInt16(void *ptr, VecInt16 v) noexcept { _mm256_store_si256(static_cast<VecInt16 *>(ptr), v); }
    static inline VecInt16 zeroInt16() noexcept { return _mm256_setzero_si256(); }
    static inline VecInt16 setInt16(Int16 v) noexcept { return _mm256_set1_epi16(v); }
    static inline VecInt16 minInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_min_epi16(a, b); }
    static inline VecInt16 maxInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_max_epi16(a, b); }
    static inline VecInt16 clampInt16(VecInt16 v, VecInt16 min, VecInt16 max) noexcept { return minInt16(maxInt16(v, min), max); }
    static inline VecInt16 addInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_add_epi16(a, b); }
    static inline VecInt16 subInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_sub_epi16(a, b); }
    static inline VecInt16 mulLoInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_mullo_epi16(a, b); }
    static inline VecInt16 shiftLeftInt16(VecInt16 v, Int32 shift) noexcept { return _mm256_slli_epi16(v, shift); }
    static inline VecInt16 shiftRightInt16(VecInt16 v, Int32 shift) noexcept { return _mm256_srai_epi16(v, shift); }
    static inline VecInt16 shiftLeftMulHiInt16(VecInt16 a, VecInt16 b, Int32 shift) noexcept { _mm256_mulhi_epi16(_mm256_slli_epi16(a, shift), b); }
    static inline VecInt32 mulAddAdjInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_madd_epi16(a, b); }
    static inline VecUInt8 packUsInt16(VecInt16 a, VecInt16 b) noexcept { return _mm256_packus_epi16(a, b); }

    static inline VecInt32 loadInt32(const void *ptr) noexcept { return _mm256_load_si256(static_cast<const VecInt32 *>(ptr)); }
    static inline void storeInt32(void *ptr, VecInt32 v) noexcept { _mm256_store_si256(static_cast<VecInt32 *>(ptr), v); }
    static inline VecInt32 zeroInt32() noexcept { return _mm256_setzero_si256(); }
    static inline VecInt32 setInt32(Int32 v) noexcept { return _mm256_set1_epi32(v); }
    static inline VecInt32 minInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_min_epi32(a, b); }
    static inline VecInt32 maxInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_max_epi32(a, b); }
    static inline VecInt32 clampInt32(VecInt32 v, VecInt32 min, VecInt32 max) noexcept { return minInt32(maxInt32(v, min), max); }
    static inline VecInt32 addInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_add_epi32(a, b); }
    static inline VecInt32 subInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_sub_epi32(a, b); }
    static inline VecInt32 mulLoInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_mullo_epi32(a, b); }
    static inline VecInt32 shiftLeftInt32(VecInt32 v, Int32 shift) noexcept { return _mm256_slli_epi32(v, shift); }
    static inline VecInt32 shiftRightInt32(VecInt32 v, Int32 shift) noexcept { return _mm256_srai_epi32(v, shift); }
    static inline VecUInt16 packUsInt32(VecInt32 a, VecInt32 b) noexcept { return _mm256_packus_epi32(a, b); }

    static inline Int32 horizAddInt32(VecInt32 v) noexcept {
        const auto hi128 = _mm256_extracti128_si256(v, 1);
        const auto lo128 = _mm256_castsi256_si128(v);
        const auto sum128 = _mm_add_epi32(hi128, lo128);
        const auto hi64 = _mm_unpackhi_epi64(sum128, sum128);
        const auto sum64 = _mm_add_epi32(sum128, hi64);
        const auto hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
        const auto sum32 = _mm_add_epi32(sum64, hi32);
        return _mm_cvtsi128_si32(sum32);
    }

    static inline UInt32 nonzeroMaskUInt8(VecUInt8 v) {
        const auto nz = _mm256_cmpgt_epi32(v, _mm256_setzero_si256());
        return _mm256_movemask_ps(_mm256_castsi256_ps(nz));
    }

    static inline VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) noexcept {
#if defined(USE_VNNI256)
        return _mm256_dpbusd_epi32(sum, u, i);
#else
        const auto p = _mm256_maddubs_epi16(u, i);
        const auto w = _mm256_madd_epi16(p, _mm256_set1_epi16(1));
        return _mm256_add_epi32(sum, w);
#endif
    }

    static inline VecInt32 dotProdInt16(VecInt32 sum, VecInt16 a, VecInt16 b) noexcept {
#if defined(USE_VNNI256)
        return _mm256_dpwssd_epi32(sum, a, b);
#else
        const auto prod = mulAddAdjInt16(a, b);
        return addInt32(sum, prod);
#endif
    }

#elif defined(USE_NEON)

    static constexpr std::uintptr_t ALIGNMENT = sizeof(int16x8_t);

    static constexpr bool PACK_REORDER = false;
    static constexpr USize PACK_GROUPING = 1;
    static constexpr USize PACK_SIZE = 0;
    static constexpr std::array<USize, 0> PACK_ORDERING = {};

    static inline VecUInt8 loadUInt8(const void *ptr) noexcept { return vld1q_u8(static_cast<const UInt8 *>(ptr)); }
    static inline void storeUInt8(void *ptr, VecUInt8 v) noexcept { vst1q_u8(static_cast<UInt8 *>(ptr), v); }
    static inline VecUInt8 zeroUInt8() noexcept { return vdupq_n_u8(0); }

    static inline VecUInt16 loadUInt16(const void *ptr) noexcept { return vld1q_u16(static_cast<const UInt16 *>(ptr)); }
    static inline void storeUInt16(void *ptr, VecUInt16 v) noexcept { vst1q_u16(static_cast<UInt16 *>(ptr), v); }
    static inline VecUInt16 zeroUInt16() noexcept { return vdupq_n_u16(0); }

    static inline VecInt8 loadInt8(const void *ptr) noexcept { return vld1q_s8(static_cast<const Int8 *>(ptr)); }
    static inline VecInt16 widenLoadInt8(const void *ptr) noexcept { return vmovl_s8(vld1_s8(static_cast<const Int8 *>(ptr))); }
    static inline void storeInt8(void *ptr, VecInt8 v) noexcept { vst1q_s8(static_cast<Int8 *>(ptr), v); }
    static inline VecInt8 zeroInt8() noexcept { return vdupq_n_s8(0); }
    static inline VecInt8 setInt8(Int8 v) noexcept { return vdupq_n_s8(v); }
    static inline VecInt8 minInt8(VecInt8 a, VecInt8 b) noexcept { return vminq_s8(a, b); }
    static inline VecInt8 maxInt8(VecInt8 a, VecInt8 b) noexcept { return vmaxq_s8(a, b); }
    static inline VecInt8 clampInt8(VecInt8 v, VecInt8 min, VecInt8 max) noexcept { return minInt8(maxInt8(v, min), max); }
    static inline VecInt8 addInt8(VecInt8 a, VecInt8 b) noexcept { return vaddq_s8(a, b); }
    static inline VecInt8 subInt8(VecInt8 a, VecInt8 b) noexcept { return vsubq_s8(a, b); }
    static inline VecInt8 shiftLeftInt8(VecInt8 v, Int32 shift) noexcept { return vshlq_s8(v, vdupq_n_s8(static_cast<UInt8>(shift))); }

    static inline VecInt16 loadInt16(const void *ptr) noexcept { return vld1q_s16(static_cast<const Int16 *>(ptr)); }
    static inline void storeInt16(void *ptr, VecInt16 v) noexcept { vst1q_s16(static_cast<Int16 *>(ptr), v); }
    static inline VecInt16 zeroInt16() noexcept { return vdupq_n_s16(0); }
    static inline VecInt16 setInt16(Int16 v) noexcept { return vdupq_n_s16(v); }
    static inline VecInt16 minInt16(VecInt16 a, VecInt16 b) noexcept { return vminq_s16(a, b); }
    static inline VecInt16 maxInt16(VecInt16 a, VecInt16 b) noexcept { return vmaxq_s16(a, b); }
    static inline VecInt16 clampInt16(VecInt16 v, VecInt16 min, VecInt16 max) noexcept { return minInt16(maxInt16(v, min), max); }
    static inline VecInt16 addInt16(VecInt16 a, VecInt16 b) noexcept { return vaddq_s16(a, b); }
    static inline VecInt16 subInt16(VecInt16 a, VecInt16 b) noexcept { return vsubq_s16(a, b); }
    static inline VecInt16 mulLoInt16(VecInt16 a, VecInt16 b) noexcept { return vmulq_s16(a, b); }
    static inline VecInt16 shiftLeftInt16(VecInt16 v, Int32 shift) noexcept { return vshlq_s16(v, vdupq_n_s16(static_cast<Int16>(shift))); }
    static inline VecInt16 shiftRightInt16(VecInt16 v, Int32 shift) noexcept { return shiftLeftInt16(v, -shift); }
    static inline VecInt16 shiftLeftMulHiInt16(VecInt16 a, VecInt16 b, Int32 shift) noexcept { return vqdmulhq_s16(vshlq_s16(a, vdupq_n_s16(static_cast<Int16>(shift - 1))), b); }
    static inline VecInt32 mulAddAdjInt16(VecInt16 a, VecInt16 b) noexcept { return vpaddq_s32(vmull_s16(vget_low_s16(a), vget_low_s16(b)), vmull_high_s16(a, b)); }
    static inline VecUInt8 packUsInt16(VecInt16 a, VecInt16 b) noexcept { return vcombine_u8(vqmovun_s16(a), vqmovun_s16(b)); }

    static inline VecInt32 loadInt32(const void *ptr) noexcept { return vld1q_s32(static_cast<const Int32 *>(ptr)); }
    static inline void storeInt32(void *ptr, VecInt32 v) noexcept { vst1q_s32(static_cast<Int32 *>(ptr), v); }
    static inline VecInt32 zeroInt32() noexcept { return vdupq_n_s32(0); }
    static inline VecInt32 setInt32(Int32 v) noexcept { return vdupq_n_s32(v); }
    static inline VecInt32 minInt32(VecInt32 a, VecInt32 b) noexcept { return vminq_s32(a, b); }
    static inline VecInt32 maxInt32(VecInt32 a, VecInt32 b) noexcept { return vmaxq_s32(a, b); }
    static inline VecInt32 clampInt32(VecInt32 v, VecInt32 min, VecInt32 max) noexcept { return minInt32(maxInt32(v, min), max); }
    static inline VecInt32 addInt32(VecInt32 a, VecInt32 b) noexcept { return vaddq_s32(a, b); }
    static inline VecInt32 subInt32(VecInt32 a, VecInt32 b) noexcept { return vsubq_s32(a, b); }
    static inline VecInt32 mulLoInt32(VecInt32 a, VecInt32 b) noexcept { return vmulq_s32(a, b); }
    static inline VecInt32 shiftLeftInt32(VecInt32 v, Int32 shift) noexcept { return vshlq_s32(v, vdupq_n_s32(static_cast<Int32>(shift))); }
    static inline VecInt32 shiftRightInt32(VecInt32 v, Int32 shift) noexcept { return shiftLeftInt32(v, -shift); }
    static inline VecUInt16 packUsInt32(VecInt32 a, VecInt32 b) noexcept { return vcombine_u16(vqmovun_s32(a), vqmovun_s32(b)); }

    static inline Int32 horizAddInt32(VecInt32 v) noexcept { return vaddvq_s32(v); }

    static inline UInt32 nonzeroMaskUInt8(VecUInt8 v) {
        alignas(ALIGNMENT) static constexpr std::array<UInt32, 4> MASK = {1, 2, 4, 8};
        return vaddvq_u32(vandq_u32(vtstq_u32(v, v), vld1q_u32(MASK.data())));
    }

    static inline VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) noexcept {
        const auto i0 = vreinterpretq_u8_s8(u);

#if defined(USE_NEON_DOTPROD)
        return vdotq_s32(sum, i0, i);
#else
        const auto lo = vmull_s8(vget_low_s8(i0), vget_low_s8(i));
        const auto hi = vmull_high_s8(i0, i);
        const auto p = vpaddq_s16(lo, hi);
        return vpadalq_s16(sum, p);
#endif
    }

    static inline VecInt32 dotProdInt16(VecInt32 sum, VecInt16 a, VecInt16 b) noexcept {
        const auto prod = mulAddAdjInt16(a, b);
        return addInt32(sum, prod);
    }

#endif
};

}
