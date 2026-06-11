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

    static inline void add1(LayerVector &data, const LayerVector &add1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = addInt16(dataReg1, loadInt16(&add1[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add1[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add1[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add1[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] += add1[i];
        }
#endif
    }

    static inline void sub1(LayerVector &data, const LayerVector &sub1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = subInt16(dataReg1, loadInt16(&sub1[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub1[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub1[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub1[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] -= sub1[i];
        }
#endif
    }

    static inline void add2(LayerVector &data, const LayerVector &add1, const LayerVector &add2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = addInt16(dataReg1, loadInt16(&add1[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add1[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add1[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add1[i + WIDTH * 3]));
            dataReg1 = addInt16(dataReg1, loadInt16(&add2[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add2[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add2[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add2[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] += add1[i] + add2[i];
        }
#endif
    }

    static inline void sub2(LayerVector &data, const LayerVector &sub1, const LayerVector &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = subInt16(dataReg1, loadInt16(&sub1[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub1[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub1[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub1[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub2[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub2[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub2[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub2[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] -= sub1[i] + sub2[i];
        }
#endif
    }

    static inline void add1Sub1(LayerVector &data, const LayerVector &add1, const LayerVector &sub1) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = addInt16(dataReg1, loadInt16(&add1[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add1[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add1[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add1[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub1[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub1[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub1[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub1[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] += add1[i] - sub1[i];
        }
#endif
    }

    static inline void add1Sub2(LayerVector &data, const LayerVector &add1, const LayerVector &sub1, const LayerVector &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = addInt16(dataReg1, loadInt16(&add1[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add1[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add1[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add1[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub1[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub1[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub1[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub1[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub2[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub2[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub2[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub2[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] += add1[i] - sub1[i] - sub2[i];
        }
#endif
    }

    static inline void add2Sub2(LayerVector &data, const LayerVector &add1, const LayerVector &add2, const LayerVector &sub1, const LayerVector &sub2) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            RegInt16 dataReg1 = loadInt16(&data[i]);
            RegInt16 dataReg2 = loadInt16(&data[i + WIDTH]);
            RegInt16 dataReg3 = loadInt16(&data[i + WIDTH * 2]);
            RegInt16 dataReg4 = loadInt16(&data[i + WIDTH * 3]);
            dataReg1 = addInt16(dataReg1, loadInt16(&add1[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add1[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add1[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add1[i + WIDTH * 3]));
            dataReg1 = addInt16(dataReg1, loadInt16(&add2[i]));
            dataReg2 = addInt16(dataReg2, loadInt16(&add2[i + WIDTH]));
            dataReg3 = addInt16(dataReg3, loadInt16(&add2[i + WIDTH * 2]));
            dataReg4 = addInt16(dataReg4, loadInt16(&add2[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub1[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub1[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub1[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub1[i + WIDTH * 3]));
            dataReg1 = subInt16(dataReg1, loadInt16(&sub2[i]));
            dataReg2 = subInt16(dataReg2, loadInt16(&sub2[i + WIDTH]));
            dataReg3 = subInt16(dataReg3, loadInt16(&sub2[i + WIDTH * 2]));
            dataReg4 = subInt16(dataReg4, loadInt16(&sub2[i + WIDTH * 3]));
            storeInt16(&data[i], dataReg1);
            storeInt16(&data[i + WIDTH], dataReg2);
            storeInt16(&data[i + WIDTH * 2], dataReg3);
            storeInt16(&data[i + WIDTH * 3], dataReg4);
        }
#else
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            data[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
#endif
    }

    static inline Int32 activate(const LayerVector &friendlyAcc, const LayerVector &enemyAcc, const DualLayerVector &weights) noexcept {
#if defined(USE_SIMD)
        static_assert(Arch::LAYER_SIZE % (WIDTH * 4) == 0);
        const RegInt16 zero = zeroInt16();
        const RegInt16 quantA = setInt16(static_cast<Int16>(Arch::QUANT_A));
        RegInt32 sum = zeroInt32();
        for (USize i = 0; i < Arch::LAYER_SIZE; i += WIDTH * 4) {
            const RegInt16 friendlyClampReg1 = clampInt16(loadInt16(&friendlyAcc[i]), zero, quantA);
            const RegInt16 friendlyClampReg2 = clampInt16(loadInt16(&friendlyAcc[i + WIDTH]), zero, quantA);
            const RegInt16 friendlyClampReg3 = clampInt16(loadInt16(&friendlyAcc[i + WIDTH * 2]), zero, quantA);
            const RegInt16 friendlyClampReg4 = clampInt16(loadInt16(&friendlyAcc[i + WIDTH * 3]), zero, quantA);
            const RegInt16 enemyClampReg1 = clampInt16(loadInt16(&enemyAcc[i]), zero, quantA);
            const RegInt16 enemyClampReg2 = clampInt16(loadInt16(&enemyAcc[i + WIDTH]), zero, quantA);
            const RegInt16 enemyClampReg3 = clampInt16(loadInt16(&enemyAcc[i + WIDTH * 2]), zero, quantA);
            const RegInt16 enemyClampReg4 = clampInt16(loadInt16(&enemyAcc[i + WIDTH * 3]), zero, quantA);
            const RegInt16 friendlyWeightReg1 = loadInt16(&weights[0][i]);
            const RegInt16 friendlyWeightReg2 = loadInt16(&weights[0][i + WIDTH]);
            const RegInt16 friendlyWeightReg3 = loadInt16(&weights[0][i + WIDTH * 2]);
            const RegInt16 friendlyWeightReg4 = loadInt16(&weights[0][i + WIDTH * 3]);
            const RegInt16 enemyWeightReg1 = loadInt16(&weights[1][i]);
            const RegInt16 enemyWeightReg2 = loadInt16(&weights[1][i + WIDTH]);
            const RegInt16 enemyWeightReg3 = loadInt16(&weights[1][i + WIDTH * 2]);
            const RegInt16 enemyWeightReg4 = loadInt16(&weights[1][i + WIDTH * 3]);
            const RegInt32 friendlyProdReg1 = mulAddInt16(friendlyClampReg1, mulLoInt16(friendlyClampReg1, friendlyWeightReg1));
            const RegInt32 friendlyProdReg2 = mulAddInt16(friendlyClampReg2, mulLoInt16(friendlyClampReg2, friendlyWeightReg2));
            const RegInt32 friendlyProdReg3 = mulAddInt16(friendlyClampReg3, mulLoInt16(friendlyClampReg3, friendlyWeightReg3));
            const RegInt32 friendlyProdReg4 = mulAddInt16(friendlyClampReg4, mulLoInt16(friendlyClampReg4, friendlyWeightReg4));
            const RegInt32 enemyProdReg1 = mulAddInt16(enemyClampReg1, mulLoInt16(enemyClampReg1, enemyWeightReg1));
            const RegInt32 enemyProdReg2 = mulAddInt16(enemyClampReg2, mulLoInt16(enemyClampReg2, enemyWeightReg2));
            const RegInt32 enemyProdReg3 = mulAddInt16(enemyClampReg3, mulLoInt16(enemyClampReg3, enemyWeightReg3));
            const RegInt32 enemyProdReg4 = mulAddInt16(enemyClampReg4, mulLoInt16(enemyClampReg4, enemyWeightReg4));
            sum = addInt32(sum, friendlyProdReg1);
            sum = addInt32(sum, friendlyProdReg2);
            sum = addInt32(sum, friendlyProdReg3);
            sum = addInt32(sum, friendlyProdReg4);
            sum = addInt32(sum, enemyProdReg1);
            sum = addInt32(sum, enemyProdReg2);
            sum = addInt32(sum, enemyProdReg3);
            sum = addInt32(sum, enemyProdReg4);
        }
        return horizAddInt32(sum);
#else
        Int32 sum = 0;
        for (USize i = 0; i < Arch::LAYER_SIZE; i++) {
            const Int32 friendlyClamp1 = std::clamp(static_cast<Int32>(friendlyAcc[i]), 0, Arch::QUANT_A);
            const Int32 enemyClamp1 = std::clamp(static_cast<Int32>(enemyAcc[i]), 0, Arch::QUANT_A);
            sum += friendlyClamp1 * friendlyClamp1 * static_cast<Int32>(weights[0][i]);
            sum += enemyClamp1 * enemyClamp1 * static_cast<Int32>(weights[1][i]);
        }
        return sum;
#endif
    }

};

}
