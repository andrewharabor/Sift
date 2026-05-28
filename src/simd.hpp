#pragma once

#include <algorithm>
#include <array>

#if defined(SIMD_AVX2)
#include <immintrin.h>
#endif

#if defined(SIMD_SSE)
#include <emmintrin.h>
#include <smmintrin.h>
#endif

#if defined(SIMD_NEON)
#include <arm_neon.h>
#endif

#include "arch.hpp"
#include "types.hpp"


namespace Syft {

class SIMD {
public:
#if defined(SIMD_AVX2)
    static constexpr USize REGISTERS = 16;
#elif defined(SIMD_SSE) || defined(SIMD_NEON)
    static constexpr USize REGISTERS = 8;
#else
    static constexpr USize REGISTERS = 1;
#endif

    static constexpr USize ITERATIONS = NNUEArch::LAYER_SIZE / REGISTERS;

#if defined(SIMD_AVX2)
    using Register = __m256i;

    static Int32 horizontalSum(Register value) noexcept {
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 8));
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 4));
        return _mm256_extract_epi32(value, 0) + _mm256_extract_epi32(value, 4);
    }

    static Int32 forward(const VecInt16 &inputs, const VecInt16 &weights) noexcept {
        Register min = _mm256_set1_epi16(0);
        Register max = _mm256_set1_epi16(NNUEArch::QA);
        Register sum = _mm256_setzero_si256();

        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        const Register *pWeights = reinterpret_cast<const Register *>(weights.data());

        for (USize i = 0; i < ITERATIONS; i++, pInputs++, pWeights++) {
            Register registerInputs = _mm256_load_si256(pInputs);
            Register registerWeights = _mm256_load_si256(pWeights);
            Register clamp = _mm256_max_epi16(registerInputs, min);
            clamp = _mm256_min_epi16(clamp, max);
            Register product = _mm256_mullo_epi16(clamp, registerWeights);
            Register result = _mm256_madd_epi16(product, clamp);
            sum = _mm256_add_epi32(sum, result);
        }

        return horizontalSum(sum);
    }

    static void add(VecInt16 &data, const VecInt16 &add) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++) {
            *pData = _mm256_add_epi16(*pData, *pAdd);
        }
    }

    static void sub(VecInt16 &data, const VecInt16 &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pSub++) {
            *pData = _mm256_sub_epi16(*pData, *pSub);
        }
    }

    static void addSub(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub++) {
            *pData = _mm256_sub_epi16(_mm256_add_epi16(*pData, *pAdd), *pSub);
        }
    }

    static void addSub2(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub1++, pSub2++) {
            *pData = _mm256_sub_epi16(_mm256_sub_epi16(_mm256_add_epi16(*pData, *pAdd), *pSub1), *pSub2);
        }
    }

    static void add2Sub2(VecInt16 &data, const VecInt16 &add1, const VecInt16 &add2, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pData = _mm256_sub_epi16(_mm256_sub_epi16(_mm256_add_epi16(_mm256_add_epi16(*pData, *pAdd1), *pAdd2), *pSub1), *pSub2);
        }
    }

#elif defined(SIMD_SSE)
    using Register = __m128i;

    static Int32 horizontalSum(Register value) noexcept {
        value = _mm_add_epi32(value, _mm_srli_si128(value, 8));
        value = _mm_add_epi32(value, _mm_srli_si128(value, 4));
        return _mm_cvtsi128_si32(value);
    }

    static Int32 forward(const VecInt16 &inputs, const VecInt16 &weights) noexcept {
        Register min = _mm_set1_epi16(0);
        Register max = _mm_set1_epi16(NNUEArch::QA);
        Register sum = _mm_setzero_si128();

        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        const Register *pWeights = reinterpret_cast<const Register *>(weights.data());

        for (USize i = 0; i < ITERATIONS; i++, pInputs++, pWeights++) {
            Register registerInputs = _mm_load_si128(pInputs);
            Register registerWeights = _mm_load_si128(pWeights);
            Register clamp = _mm_max_epi16(registerInputs, min);
            clamp = _mm_min_epi16(clamp, max);
            Register product = _mm_mullo_epi16(clamp, registerWeights);
            Register result = _mm_madd_epi16(product, clamp);
            sum = _mm_add_epi32(sum, result);
        }

        return horizontalSum(sum);
    }

    static void add(VecInt16 &data, const VecInt16 &add) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++) {
            *pData = _mm_add_epi16(*pData, *pAdd);
        }
    }

    static void sub(VecInt16 &data, const VecInt16 &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pSub++) {
            *pData = _mm_sub_epi16(*pData, *pSub);
        }
    }

    static void addSub(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub++) {
            *pData = _mm_sub_epi16(_mm_add_epi16(*pData, *pAdd), *pSub);
        }
    }

    static void addSub2(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub1++, pSub2++) {
            *pData = _mm_sub_epi16(_mm_sub_epi16(_mm_add_epi16(*pData, *pAdd), *pSub1), *pSub2);
        }
    }

    static void add2Sub2(VecInt16 &data, const VecInt16 &add1, const VecInt16 &add2, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (USize i = 0; i < ITERATIONS; i++, pData++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pData = _mm_sub_epi16(_mm_sub_epi16(_mm_add_epi16(_mm_add_epi16(*pData, *pAdd1), *pAdd2), *pSub1), *pSub2);
        }
    }

#elif defined(SIMD_NEON)

    static Int32 horizontalSum(int32x4_t value) noexcept {
        return vaddvq_s32(value);
    }

    static Int32 forward(const VecInt16 &inputs, const VecInt16 &weights) noexcept {
        int16x8_t min = vdupq_n_s16(0);
        int16x8_t max = vdupq_n_s16(NNUEArch::QA);
        int32x4_t sum = vdupq_n_s32(0);

        const Int16 *pInputs = inputs.data();
        const Int16 *pWeights = weights.data();

        for (USize i = 0; i < ITERATIONS; i++, pInputs += REGISTERS, pWeights += REGISTERS) {
            int16x8_t registerInputs = vld1q_s16(pInputs);
            int16x8_t registerWeights = vld1q_s16(pWeights);
            int16x8_t clamp = vmaxq_s16(registerInputs, min);
            clamp = vminq_s16(clamp, max);
            int16x8_t product = vmulq_s16(clamp, registerWeights);
            int32x4_t result = vmull_s16(vget_low_s16(product), vget_low_s16(clamp));
            result = vmlal_s16(result, vget_high_s16(product), vget_high_s16(clamp));
            sum = vaddq_s32(sum, result);
        }

        return horizontalSum(sum);
    }

    static void add(VecInt16 &data, const VecInt16 &add) noexcept {
        Int16 *pData = data.data();
        const Int16 *pAdd = add.data();
        for (USize i = 0; i < ITERATIONS; i++, pData += REGISTERS, pAdd += REGISTERS) {
            vst1q_s16(pData, vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)));
        }
    }

    static void sub(VecInt16 &data, const VecInt16 &sub) noexcept {
        Int16 *pData = data.data();
        const Int16 *pSub = sub.data();
        for (USize i = 0; i < ITERATIONS; i++, pData += REGISTERS, pSub += REGISTERS) {
            vst1q_s16(pData, vsubq_s16(vld1q_s16(pData), vld1q_s16(pSub)));
        }
    }

    static void addSub(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub) noexcept {
        Int16 *pData = data.data();
        const Int16 *pAdd = add.data();
        const Int16 *pSub = sub.data();
        for (USize i = 0; i < ITERATIONS; i++, pData += REGISTERS, pAdd += REGISTERS, pSub += REGISTERS) {
            vst1q_s16(pData, vsubq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)), vld1q_s16(pSub)));
        }
    }

    static void addSub2(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Int16 *pData = data.data();
        const Int16 *pAdd = add.data();
        const Int16 *pSub1 = sub1.data();
        const Int16 *pSub2 = sub2.data();
        for (USize i = 0; i < ITERATIONS; i++, pData += REGISTERS, pAdd += REGISTERS, pSub1 += REGISTERS, pSub2 += REGISTERS) {
            vst1q_s16(pData, vsubq_s16(vsubq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)), vld1q_s16(pSub1)), vld1q_s16(pSub2)));
        }
    }

    static void add2Sub2(VecInt16 &data, const VecInt16 &add1, const VecInt16 &add2, const VecInt16 &sub1, const VecInt16 &sub2) noexcept {
        Int16 *pData = data.data();
        const Int16 *pAdd1 = add1.data();
        const Int16 *pAdd2 = add2.data();
        const Int16 *pSub1 = sub1.data();
        const Int16 *pSub2 = sub2.data();
        for (USize i = 0; i < ITERATIONS; i++, pData += REGISTERS, pAdd1 += REGISTERS, pAdd2 += REGISTERS, pSub1 += REGISTERS, pSub2 += REGISTERS) {
            vst1q_s16(pData, vsubq_s16(vsubq_s16(vaddq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd1)), vld1q_s16(pAdd2)), vld1q_s16(pSub1)), vld1q_s16(pSub2)));
        }
    }

#else

    static constexpr Int32 screlu(Int16 value) {
        Int16 result = std::clamp(value, static_cast<Int16>(0), static_cast<Int16>(NNUEArch::QA));
        return result * result;
    }

    static constexpr Int32 forward(const VecInt16 &inputs, const VecInt16 &weights) {
        Int32 output = 0;
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            output += screlu(inputs[i]) * weights[i];
        }
        return output;
    }

    static constexpr void add(VecInt16 &data, const VecInt16 &add) {
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            data[i] += add[i];
        }
    }

    static constexpr void sub(VecInt16 &data, const VecInt16 &sub) {
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            data[i] -= sub[i];
        }
    }

    static constexpr void addSub(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub) {
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            data[i] += add[i] - sub[i];
        }
    }

    static constexpr void addSub2(VecInt16 &data, const VecInt16 &add, const VecInt16 &sub1, const VecInt16 &sub2) {
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            data[i] += add[i] - sub1[i] - sub2[i];
        }
    }

    static constexpr void add2Sub2(VecInt16 &data, const VecInt16 &add1, const VecInt16 &add2, const VecInt16 &sub1, const VecInt16 &sub2) {
        for (USize i = 0; i < NNUEArch::LAYER_SIZE; i++) {
            data[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
    }

#endif

};

}
