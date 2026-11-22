#pragma once

#if defined(USE_AVX2) || defined(USE_AVX512) || defined(USE_NEON)
    #define USE_SIMD
#endif

#if defined(USE_AVX2) || defined(USE_AVX512)
    #include <immintrin.h>
    #include <xmmintrin.h>
#endif

#if defined(USE_NEON)
    #include <arm_neon.h>
#endif

#include "types.hpp"
#include "utils.hpp"

namespace Sift {
#if defined(USE_AVX512)

    using VecInt8 = __m512i;
    using VecUInt8 = __m512i;
    using VecInt16 = __m512i;
    using VecHalfInt16 = __m256i;
    using VecUInt16 = __m512i;
    using VecInt32 = __m512i;
    using VecFloat32 = __m512;

#elif defined(USE_AVX2)

    using VecInt8 = __m256i;
    using VecUInt8 = __m256i;
    using VecInt16 = __m256i;
    using VecHalfInt16 = __m128i;
    using VecUInt16 = __m256i;
    using VecInt32 = __m256i;
    using VecFloat32 = __m256;

#elif defined(USE_NEON)

    using VecInt8 = int8x16_t;
    using VecUInt8 = uint8x16_t;
    using VecInt16 = int16x8_t;
    using VecHalfInt16 = int8x8_t;
    using VecUInt16 = uint16x8_t;
    using VecInt32 = int32x4_t;
    using VecFloat32 = float32x4_t;

#endif

#if defined(USE_AVX512) || defined(USE_AVX2)

    using Vec128UInt16 = __m128i;

#elif defined(USE_NEON)

    using Vec128UInt16 = uint16x8_t;

#endif

    class SIMD {
    public:
#if defined(USE_AVX512)

        static FORCE_INLINE VecInt16 addInt16(VecInt16 x, VecInt16 y) { return _mm512_add_epi16(x, y); }

        static FORCE_INLINE VecInt16 subInt16(VecInt16 x, VecInt16 y) { return _mm512_sub_epi16(x, y); }

        static FORCE_INLINE VecInt16 widenLoadInt16(VecHalfInt16 x) { return _mm512_cvtepi8_epi16(x); }

        static FORCE_INLINE VecInt16 minInt16(VecInt16 x, VecInt16 y) { return _mm512_min_epi16(x, y); }

        static FORCE_INLINE VecInt16 maxInt16(VecInt16 x, VecInt16 y) { return _mm512_max_epi16(x, y); }

        static FORCE_INLINE VecInt16 setInt16(Int16 i) { return _mm512_set1_epi16(i); }

        static FORCE_INLINE VecUInt8 setUInt8(Int32 i) { return _mm512_set1_epi32(i); }

        static FORCE_INLINE VecInt16 shiftRightInt16(VecInt16 x, Int32 shift) { return _mm512_srli_epi16(x, static_cast<Int16>(shift)); }

        static FORCE_INLINE VecInt16 shiftLeftInt16(VecInt16 x, Int32 shift) { return _mm512_slli_epi16(x, static_cast<Int16>(shift)); }

        static FORCE_INLINE VecInt16 mulHiInt16(VecInt16 x, VecInt16 y) { return _mm512_mulhi_epi16(x, y); }

    #if defined(USE_VNNI512)

        static FORCE_INLINE VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) { return _mm512_dpbusd_epi32(sum, u, i); }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8x2(VecInt32 sum, VecUInt8 u, VecInt8 i, VecUInt8 u2, VecInt8 i2) {
            return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, u, i), u2, i2);
        }

    #else

        static FORCE_INLINE VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) {
            VecInt32 sum32 = _mm512_mullAddAdjInt16_epi16(_mm512_mulAddAdjUInt8Int8_epi16(u, i), _mm512_set1_epi16(1));
            return _mm512_add_epi32(sum32, sum);
        }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8x2(VecInt32 sum, VecUInt8 u, VecInt8 i, VecUInt8 u2, VecInt8 i2) {
            VecInt16 mul1 = _mm512_mulAddAdjUInt8Int8_epi16(u, i);
            VecInt16 mul2 = _mm512_mulAddAdjUInt8Int8_epi16(u2, i2);
            VecInt32 sum32 = _mm512_mullAddAdjInt16_epi16(_mm512_add_epi16(mul1, mul2), _mm512_set1_epi16(1));
            return _mm512_add_epi32(sum32, sum);
        }

    #endif

        static FORCE_INLINE VecUInt8 packUsInt16(VecInt16 x, VecInt16 y) { return _mm512_packus_epi16(x, y); }

        static FORCE_INLINE void storeInt16(VecInt16* ptr, VecInt16 x) { _mm512_store_si512(ptr, x); }

        static FORCE_INLINE VecFloat32 castInt32(VecInt32 x) { return _mm512_cvtepi32_ps(x); }

        static FORCE_INLINE VecFloat32 addFloat32(VecFloat32 x, VecFloat32 y) { return _mm512_add_ps(x, y); }

        static FORCE_INLINE VecFloat32 mulfloat32(VecFloat32 x, VecFloat32 y) { return _mm512_mul_ps(x, y); }

        static FORCE_INLINE VecFloat32 setFloat32(Float32 x) { return _mm512_set1_ps(x); }

        static FORCE_INLINE VecFloat32 minFloat32(VecFloat32 x, VecFloat32 y) { return _mm512_min_ps(x, y); }

        static FORCE_INLINE VecFloat32 maxFloat32(VecFloat32 x, VecFloat32 y) { return _mm512_max_ps(x, y); }

        static FORCE_INLINE VecFloat32 mulAddAdjFloat32(VecFloat32 a, VecFloat32 b, VecFloat32 c) {
            return _mm512_fmullAddAdjInt16_ps(a, b, c);
        }

        static FORCE_INLINE Float32 reduceAddFloat32(VecFloat32* v) { return _mm512_reduce_add_ps(v[0]); }

        static FORCE_INLINE UInt32 nnzMask(VecInt32 chunk) { return _mm512_cmpgt_epi32_mask(chunk, _mm512_setzero_si512()); }

#elif defined(USE_AVX2)

        static FORCE_INLINE VecInt16 addInt16(VecInt16 x, VecInt16 y) { return _mm256_add_epi16(x, y); }

        static FORCE_INLINE VecInt16 subInt16(VecInt16 x, VecInt16 y) { return _mm256_sub_epi16(x, y); }

        static FORCE_INLINE VecInt16 widenLoadInt16(VecHalfInt16 x) { return _mm256_cvtepi8_epi16(x); }

        static FORCE_INLINE VecInt16 minInt16(VecInt16 x, VecInt16 y) { return _mm256_min_epi16(x, y); }

        static FORCE_INLINE VecInt16 maxInt16(VecInt16 x, VecInt16 y) { return _mm256_max_epi16(x, y); }

        static FORCE_INLINE VecInt16 setInt16(Int16 i) { return _mm256_set1_epi16(i); }

        static FORCE_INLINE VecUInt8 setUInt8(Int32 i) { return _mm256_set1_epi32(i); }

        static FORCE_INLINE VecInt16 shiftRightInt16(VecInt16 x, Int32 shift) { return _mm256_srli_epi16(x, static_cast<Int16>(shift)); }

        static FORCE_INLINE VecInt16 shiftLeftInt16(VecInt16 x, Int32 shift) { return _mm256_slli_epi16(x, static_cast<Int16>(shift)); }

        static FORCE_INLINE VecInt16 mulHiInt16(VecInt16 x, VecInt16 y) { return _mm256_mulhi_epi16(x, y); }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) {
            VecInt32 sum32 = _mm256_mullAddAdjInt16_epi16(_mm256_mulAddAdjUInt8Int8_epi16(u, i), _mm256_set1_epi16(1));
            return _mm256_add_epi32(sum32, sum);
        }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8x2(VecInt32 sum, VecUInt8 u, VecInt8 i, VecUInt8 u2, VecInt8 i2) {
            VecInt16 mul1 = _mm256_mulAddAdjUInt8Int8_epi16(u, i);
            VecInt16 mul2 = _mm256_mulAddAdjUInt8Int8_epi16(u2, i2);
            VecInt32 sum32 = _mm256_mullAddAdjInt16_epi16(_mm256_add_epi16(mul1, mul2), _mm256_set1_epi16(1));
            return _mm256_add_epi32(sum32, sum);
        }

        static FORCE_INLINE VecUInt8 packUsInt16(VecInt16 x, VecInt16 y) { return _mm256_packus_epi16(x, y); }

        static FORCE_INLINE void storeInt16(VecInt16* ptr, VecInt16 x) { _mm256_store_si256(ptr, x); }

        static FORCE_INLINE VecFloat32 castInt32(VecInt32 x) { return _mm256_cvtepi32_ps(x); }

        static FORCE_INLINE VecFloat32 addFloat32(VecFloat32 x, VecFloat32 y) { return _mm256_add_ps(x, y); }

        static FORCE_INLINE VecFloat32 mulfloat32(VecFloat32 x, VecFloat32 y) { return _mm256_mul_ps(x, y); }

        static FORCE_INLINE VecFloat32 setFloat32(Float32 x) { return _mm256_set1_ps(x); }

        static FORCE_INLINE VecFloat32 minFloat32(VecFloat32 x, VecFloat32 y) { return _mm256_min_ps(x, y); }

        static FORCE_INLINE VecFloat32 maxFloat32(VecFloat32 x, VecFloat32 y) { return _mm256_max_ps(x, y); }

        static FORCE_INLINE VecFloat32 mulAddAdjFloat32(VecFloat32 a, VecFloat32 b, VecFloat32 c) {
            return _mm256_fmullAddAdjInt16_ps(a, b, c);
        }

        static FORCE_INLINE Float32 reduceAddFloat32(VecFloat32* v) {
            v[0] = _mm256_add_ps(v[0], v[1]);
            __m128 high = _mm256_extractf128_ps(v[0], 1);
            __m128 low = _mm256_castps256_ps128(v[0]);
            __m128 sum = _mm_add_ps(high, low);
            __m128 high64 = _mm_movehl_ps(sum, sum);
            __m128 sum64 = _mm_add_ps(sum, high64);
            return (reinterpret_cast<Float32*>(&sum64))[0] + (reinterpret_cast<Float32*>(&sum64))[1];
        }

        static FORCE_INLINE UInt32 nnzMask(VecInt32 chunk) {
            return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
        }

#elif defined(USE_NEON)

    private:
        static FORCE_INLINE VecInt16 mulAddAdjUInt8Int8(VecUInt8 a, VecInt8 b) {
            VecInt16 tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a))), vmovl_s8(vget_low_s8(b)));
            VecInt16 th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a))), vmovl_s8(vget_high_s8(b)));
            return vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th));
        }

        static FORCE_INLINE VecInt32 mullAddAdjInt16(VecInt16 a, VecInt16 b) {
            VecInt32 low = vmull_s16(vget_low_s16(a), vget_low_s16(b));
            VecInt32 high = vmull_high_s16(a, b);
            return vpaddq_s32(low, high);
        }

        static FORCE_INLINE Float32 reduceAddFloat32Length(Float32* v, USize length) {
            if (length == 2) { return v[0] + v[1]; }
            length /= 2;
            for (USize i = 0; i < length; i++) { v[i] += v[i + length]; }
            return reduceAddFloat32Length(v, length);
        }

    public:
        static FORCE_INLINE VecInt16 addInt16(VecInt16 x, VecInt16 y) { return vaddq_s16(x, y); }

        static FORCE_INLINE VecInt16 subInt16(VecInt16 x, VecInt16 y) { return vsubq_s16(x, y); }

        static FORCE_INLINE VecInt16 widenLoadInt16(VecHalfInt16 x) { return vshll_n_s8(x, 0); }

        static FORCE_INLINE VecInt16 minInt16(VecInt16 x, VecInt16 y) { return vminq_s16(x, y); }

        static FORCE_INLINE VecInt16 maxInt16(VecInt16 x, VecInt16 y) { return vmaxq_s16(x, y); }

        static FORCE_INLINE VecInt16 setInt16(Int16 i) { return vdupq_n_s16(i); }

        static FORCE_INLINE VecUInt8 setUInt8(Int32 i) { return vreinterpretq_u8_s32(vdupq_n_s32(i)); }

        static FORCE_INLINE VecInt16 shiftRightInt16(VecInt16 x, Int32 shift) {
            return vshlq_s16(x, vdupq_n_s16(-static_cast<Int16>(shift)));
        }

        static FORCE_INLINE VecInt16 shiftLeftInt16(VecInt16 x, Int32 shift) {
            return vshlq_s16(x, vdupq_n_s16(static_cast<Int16>(shift)));
        }

        static FORCE_INLINE VecInt16 mulHiInt16(VecInt16 x, VecInt16 y) {
            VecInt32 low = vmull_s16(vget_low_s16(x), vget_low_s16(y));
            VecInt32 high = vmull_s16(vget_high_s16(x), vget_high_s16(y));
            return vcombine_s16(vshrn_n_s32(low, 16), vshrn_n_s32(high, 16));
        }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8(VecInt32 sum, VecUInt8 u, VecInt8 i) {
            VecInt32 sum32 = mullAddAdjInt16(mulAddAdjUInt8Int8(u, i), vdupq_n_s16(1));
            return vaddq_s32(sum32, sum);
        }

        static FORCE_INLINE VecInt32 dotProdUInt8Int8x2(VecInt32 sum, VecUInt8 u, VecInt8 i, VecUInt8 u2, VecInt8 i2) {
            VecInt16 mul1 = mulAddAdjUInt8Int8(u, i);
            VecInt16 mul2 = mulAddAdjUInt8Int8(u2, i2);
            VecInt32 sum32 = mullAddAdjInt16(vaddq_s16(mul1, mul2), vdupq_n_s16(1));
            return vaddq_s32(sum32, sum);
        }

        static FORCE_INLINE VecUInt8 packUsInt16(VecInt16 x, VecInt16 y) { return vcombine_u8(vqmovun_s16(x), vqmovun_s16(y)); }

        static FORCE_INLINE void storeInt16(VecInt16* ptr, VecInt16 x) { vst1q_s16(reinterpret_cast<int16_t*>(ptr), x); }

        static FORCE_INLINE VecFloat32 castInt32(VecInt32 x) { return vcvtq_f32_s32(x); }

        static FORCE_INLINE VecFloat32 addFloat32(VecFloat32 x, VecFloat32 y) { return vaddq_f32(x, y); }

        static FORCE_INLINE VecFloat32 mulfloat32(VecFloat32 x, VecFloat32 y) { return vmulq_f32(x, y); }

        static FORCE_INLINE VecFloat32 setFloat32(Float32 x) { return vdupq_n_f32(x); }

        static FORCE_INLINE VecFloat32 minFloat32(VecFloat32 x, VecFloat32 y) { return vminq_f32(x, y); }

        static FORCE_INLINE VecFloat32 maxFloat32(VecFloat32 x, VecFloat32 y) { return vmaxq_f32(x, y); }

        static FORCE_INLINE VecFloat32 mulAddAdjFloat32(VecFloat32 a, VecFloat32 b, VecFloat32 c) { return vfmaq_f32(c, a, b); }

        static FORCE_INLINE Float32 reduceAddFloat32(VecFloat32* v) {
            return reduceAddFloat32Length(reinterpret_cast<Float32*>(v), 64 / sizeof(Float32));
        }

        static FORCE_INLINE UInt32 nnzMask(VecInt32 chunk) {
            uint32x4_t mask = vcgtq_s32(chunk, vdupq_n_s32(0));
            uint16x4_t narrowed = vmovn_u32(mask);
            uint64_t packed = vget_lane_u64(vreinterpret_u64_u16(narrowed), 0);
            return ((packed & (1ULL << 0)) >> 0) | ((packed & (1ULL << 16)) >> 15) | ((packed & (1ULL << 32)) >> 30) |
                   ((packed & (1ULL << 48)) >> 45);
        }

#endif

#if defined(USE_AVX512) || defined(USE_AVX2)

        static FORCE_INLINE Vec128UInt16 zero128UInt16() { return _mm_setzero_si128(); }

        static FORCE_INLINE Vec128UInt16 set128UInt16(UInt16 u) { return _mm_set1_epi16(static_cast<Int16>(u)); }

        static FORCE_INLINE Vec128UInt16 load128UInt16(const UInt16* ptr) {
            return _mm_loadu_si128(reinterpret_cast<const Vec128UInt16*>(ptr));
        }

        static FORCE_INLINE void store128UInt16(UInt16* ptr, Vec128UInt16 x) { _mm_storeu_si128(reinterpret_cast<Vec128UInt16*>(ptr), x); }

        static FORCE_INLINE Vec128UInt16 add128UInt16(Vec128UInt16 x, Vec128UInt16 y) { return _mm_add_epi16(x, y); }

#elif defined(USE_NEON)

        static FORCE_INLINE Vec128UInt16 zero128UInt16() { return vdupq_n_u16(0); }

        static FORCE_INLINE Vec128UInt16 set128UInt16(UInt16 u) { return vdupq_n_u16(u); }

        static FORCE_INLINE Vec128UInt16 load128UInt16(const UInt16* ptr) { return vld1q_u16(ptr); }

        static FORCE_INLINE void store128UInt16(UInt16* ptr, Vec128UInt16 x) { vst1q_u16(ptr, x); }

        static FORCE_INLINE Vec128UInt16 add128UInt16(Vec128UInt16 x, Vec128UInt16 y) { return vaddq_u16(x, y); }

#endif
    };
}
