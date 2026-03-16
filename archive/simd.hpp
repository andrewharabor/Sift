#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#if defined(USE_AVX2)
#include <immintrin.h>
#endif

#if defined(USE_SSE)
#include <emmintrin.h>
#include <smmintrin.h>
#endif

#if defined(USE_NEON)
#include <arm_neon.h>
#endif

#include "constants.hpp"


namespace Clownfish {
class alignas(64) AlignedVector {
public:
    constexpr AlignedVector() : values{} {}

    constexpr std::int16_t *data() noexcept { return values.data(); }
    constexpr const std::int16_t *data() const noexcept { return values.data(); }

    constexpr std::int16_t &operator[](std::size_t index) noexcept { return values[index]; }
    constexpr const std::int16_t &operator[](std::size_t index) const noexcept { return values[index]; }

private:
    std::array<std::int16_t, Constants::NNUE_LAYER_SIZE> values;
};

class SIMD {
public:
    static constexpr std::size_t ITERATIONS = Constants::NNUE_LAYER_SIZE / Constants::SIMD_LANES;

#if defined(USE_AVX2)
    using Register = __m256i;

    static std::int32_t horizontalSum(Register value) noexcept {
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 8));
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 4));
        return _mm256_extract_epi32(value, 0) + _mm256_extract_epi32(value, 4);
    }

    static std::int32_t forward(const AlignedVector &inputs, const AlignedVector &weights) noexcept {
        Register min = _mm256_set1_epi16(0);
        Register max = _mm256_set1_epi16(Constants::NNUE_QA);
        Register sum = _mm256_setzero_si256();

        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        const Register *pWeights = reinterpret_cast<const Register *>(weights.data());

        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pWeights++) {
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

    static void add(AlignedVector &data, const AlignedVector &add) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++) {
            *pData = _mm256_add_epi16(*pData, *pAdd);
        }
    }

    static void sub(AlignedVector &data, const AlignedVector &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pSub++) {
            *pData = _mm256_sub_epi16(*pData, *pSub);
        }
    }

    static void addSub(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub++) {
            *pData = _mm256_sub_epi16(_mm256_add_epi16(*pData, *pAdd), *pSub);
        }
    }

    static void addSub2(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub1++, pSub2++) {
            *pData = _mm256_sub_epi16(_mm256_sub_epi16(_mm256_add_epi16(*pData, *pAdd), *pSub1), *pSub2);
        }
    }

    static void add2Sub2(AlignedVector &data, const AlignedVector &add1, const AlignedVector &add2, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pData = _mm256_sub_epi16(_mm256_sub_epi16(_mm256_add_epi16(_mm256_add_epi16(*pData, *pAdd1), *pAdd2), *pSub1), *pSub2);
        }
    }

#elif defined(USE_SSE)
    using Register = __m128i;

    static std::int32_t horizontalSum(Register value) noexcept {
        value = _mm_add_epi32(value, _mm_srli_si128(value, 8));
        value = _mm_add_epi32(value, _mm_srli_si128(value, 4));
        return _mm_cvtsi128_si32(value);
    }

    static std::int32_t forward(const AlignedVector &inputs, const AlignedVector &weights) noexcept {
        Register min = _mm_set1_epi16(0);
        Register max = _mm_set1_epi16(Constants::NNUE_QA);
        Register sum = _mm_setzero_si128();

        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        const Register *pWeights = reinterpret_cast<const Register *>(weights.data());

        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pWeights++) {
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

    static void add(AlignedVector &data, const AlignedVector &add) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++) {
            *pData = _mm_add_epi16(*pData, *pAdd);
        }
    }

    static void sub(AlignedVector &data, const AlignedVector &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pSub++) {
            *pData = _mm_sub_epi16(*pData, *pSub);
        }
    }

    static void addSub(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub++) {
            *pData = _mm_sub_epi16(_mm_add_epi16(*pData, *pAdd), *pSub);
        }
    }

    static void addSub2(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd++, pSub1++, pSub2++) {
            *pData = _mm_sub_epi16(_mm_sub_epi16(_mm_add_epi16(*pData, *pAdd), *pSub1), *pSub2);
        }
    }

    static void add2Sub2(AlignedVector &data, const AlignedVector &add1, const AlignedVector &add2, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        Register *pData = reinterpret_cast<Register *>(data.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pData++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pData = _mm_sub_epi16(_mm_sub_epi16(_mm_add_epi16(_mm_add_epi16(*pData, *pAdd1), *pAdd2), *pSub1), *pSub2);
        }
    }

#elif defined(USE_NEON)

    static std::int32_t horizontalSum(int32x4_t value) noexcept {
        return vaddvq_s32(value);
    }

    static std::int32_t forward(const AlignedVector &inputs, const AlignedVector &weights) noexcept {
        int16x8_t min = vdupq_n_s16(0);
        int16x8_t max = vdupq_n_s16(Constants::NNUE_QA);
        int32x4_t sum = vdupq_n_s32(0);

        const std::int16_t *pInputs = inputs.data();
        const std::int16_t *pWeights = weights.data();

        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs += Constants::SIMD_LANES, pWeights += Constants::SIMD_LANES) {
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

    static void add(AlignedVector &data, const AlignedVector &add) noexcept {
        std::int16_t *pData = data.data();
        const std::int16_t *pAdd = add.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pData += Constants::SIMD_LANES, pAdd += Constants::SIMD_LANES) {
            vst1q_s16(pData, vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)));
        }
    }

    static void sub(AlignedVector &data, const AlignedVector &sub) noexcept {
        std::int16_t *pData = data.data();
        const std::int16_t *pSub = sub.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pData += Constants::SIMD_LANES, pSub += Constants::SIMD_LANES) {
            vst1q_s16(pData, vsubq_s16(vld1q_s16(pData), vld1q_s16(pSub)));
        }
    }

    static void addSub(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub) noexcept {
        std::int16_t *pData = data.data();
        const std::int16_t *pAdd = add.data();
        const std::int16_t *pSub = sub.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pData += Constants::SIMD_LANES, pAdd += Constants::SIMD_LANES, pSub += Constants::SIMD_LANES) {
            vst1q_s16(pData, vsubq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)), vld1q_s16(pSub)));
        }
    }

    static void addSub2(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        std::int16_t *pData = data.data();
        const std::int16_t *pAdd = add.data();
        const std::int16_t *pSub1 = sub1.data();
        const std::int16_t *pSub2 = sub2.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pData += Constants::SIMD_LANES, pAdd += Constants::SIMD_LANES, pSub1 += Constants::SIMD_LANES, pSub2 += Constants::SIMD_LANES) {
            vst1q_s16(pData, vsubq_s16(vsubq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd)), vld1q_s16(pSub1)), vld1q_s16(pSub2)));
        }
    }

    static void add2Sub2(AlignedVector &data, const AlignedVector &add1, const AlignedVector &add2, const AlignedVector &sub1, const AlignedVector &sub2) noexcept {
        std::int16_t *pData = data.data();
        const std::int16_t *pAdd1 = add1.data();
        const std::int16_t *pAdd2 = add2.data();
        const std::int16_t *pSub1 = sub1.data();
        const std::int16_t *pSub2 = sub2.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pData += Constants::SIMD_LANES, pAdd1 += Constants::SIMD_LANES, pAdd2 += Constants::SIMD_LANES, pSub1 += Constants::SIMD_LANES, pSub2 += Constants::SIMD_LANES) {
            vst1q_s16(pData, vsubq_s16(vsubq_s16(vaddq_s16(vaddq_s16(vld1q_s16(pData), vld1q_s16(pAdd1)), vld1q_s16(pAdd2)), vld1q_s16(pSub1)), vld1q_s16(pSub2)));
        }
    }

#else

    static constexpr std::int32_t screlu(std::int16_t value) {
        std::int16_t result = std::clamp(value, static_cast<std::int16_t>(0), static_cast<std::int16_t>(Constants::NNUE_QA));
        return result * result;
    }

    static constexpr std::int32_t forward(const AlignedVector &inputs, const AlignedVector &weights) {
        std::int32_t output = 0;
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            output += screlu(inputs[i]) * weights[i];
        }
        return output;
    }

    static constexpr void add(AlignedVector &data, const AlignedVector &add) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            data[i] += add[i];
        }
    }

    static constexpr void sub(AlignedVector &data, const AlignedVector &sub) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            data[i] -= sub[i];
        }
    }

    static constexpr void addSub(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            data[i] += add[i] - sub[i];
        }
    }

    static constexpr void addSub2(AlignedVector &data, const AlignedVector &add, const AlignedVector &sub1, const AlignedVector &sub2) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            data[i] += add[i] - sub1[i] - sub2[i];
        }
    }

    static constexpr void add2Sub2(AlignedVector &data, const AlignedVector &add1, const AlignedVector &add2, const AlignedVector &sub1, const AlignedVector &sub2) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            data[i] += add1[i] + add2[i] - sub1[i] - sub2[i];
        }
    }

#endif

};

}
