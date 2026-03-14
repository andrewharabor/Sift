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

class alignas(64) AlignedBuffer {
public:
    constexpr AlignedBuffer() : values{} {}

    std::int16_t *data() { return values.data(); }
    const std::int16_t *data() const { return values.data(); }

    std::int16_t &operator[](std::size_t index) { return values[index]; }
    const std::int16_t &operator[](std::size_t index) const { return values[index]; }

private:
    std::array<std::int16_t, Constants::NNUE_LAYER_SIZE> values;
};

class SIMD {
public:
    static constexpr std::size_t ITERATIONS = Constants::NNUE_LAYER_SIZE / Constants::SIMD_LANES;

#if defined(USE_AVX2)
    using Register = __m256i;

    static int32_t horizontalSum(Register value) noexcept {
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 8));
        value = _mm256_add_epi32(value, _mm256_srli_si256(value, 4));
        return _mm256_extract_epi32(value, 0) + _mm256_extract_epi32(value, 4);
    }

    static int32_t forward(const AlignedBuffer &inputs, const AlignedBuffer &weights) noexcept {
        Register min = _mm256_set1_epi16(0);
        Register max = _mm256_set1_epi16(Constants::NNUE_QUANT_A);
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

    static void add(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutputs = reinterpret_cast<Register *>(outputs.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pOutputs++) {
            *pOutputs = _mm256_add_epi16(*pOutputs, *pInputs);
        }
    }

    static void sub(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutputs = reinterpret_cast<Register *>(outputs.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pOutputs++) {
            *pOutputs = _mm256_sub_epi16(*pOutputs, *pInputs);
        }
    }

    static void addSub(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd++, pSub++) {
            *pOutput = _mm256_add_epi16(*pInput, *pAdd);
            *pOutput = _mm256_sub_epi16(*pOutput, *pSub);
        }
    }

    static void addSub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd++, pSub1++, pSub2++) {
            *pOutput = _mm256_add_epi16(*pInput, *pAdd);
            *pOutput = _mm256_sub_epi16(*pOutput, *pSub1);
            *pOutput = _mm256_sub_epi16(*pOutput, *pSub2);
        }
    }

    static void add2Sub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add1, const AlignedBuffer &add2, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pOutput = _mm256_add_epi16(*pInput, *pAdd1);
            *pOutput = _mm256_add_epi16(*pOutput, *pAdd2);
            *pOutput = _mm256_sub_epi16(*pOutput, *pSub1);
            *pOutput = _mm256_sub_epi16(*pOutput, *pSub2);
        }
    }

#elif defined (USE_SSE)
    using Register = __m128i;

    static int32_t horizontalSum(Register value) noexcept {
        value = _mm_add_epi32(value, _mm_srli_si128(value, 8));
        value = _mm_add_epi32(value, _mm_srli_si128(value, 4));
        return _mm_cvtsi128_si32(value);
    }

    static int32_t forward(const AlignedBuffer &inputs, const AlignedBuffer &weights) noexcept {
        Register min = _mm_set1_epi16(0);
        Register max = _mm_set1_epi16(Constants::NNUE_QUANT_A);
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

    static void add(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutputs = reinterpret_cast<Register *>(outputs.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pOutputs++) {
            *pOutputs = _mm_add_epi16(*pOutputs, *pInputs);
        }
    }

    static void sub(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const Register *pInputs = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutputs = reinterpret_cast<Register *>(outputs.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs++, pOutputs++) {
            *pOutputs = _mm_sub_epi16(*pOutputs, *pInputs);
        }
    }

    static void addSub(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub = reinterpret_cast<const Register *>(sub.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd++, pSub++) {
            *pOutput = _mm_add_epi16(*pInput, *pAdd);
            *pOutput = _mm_sub_epi16(*pOutput, *pSub);
        }
    }

    static void addSub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd = reinterpret_cast<const Register *>(add.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd++, pSub1++, pSub2++) {
            *pOutput = _mm_add_epi16(*pInput, *pAdd);
            *pOutput = _mm_sub_epi16(*pOutput, *pSub1);
            *pOutput = _mm_sub_epi16(*pOutput, *pSub2);
        }
    }

    static void add2Sub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add1, const AlignedBuffer &add2, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const Register *pInput = reinterpret_cast<const Register *>(inputs.data());
        Register *pOutput = reinterpret_cast<Register *>(outputs.data());
        const Register *pAdd1 = reinterpret_cast<const Register *>(add1.data());
        const Register *pAdd2 = reinterpret_cast<const Register *>(add2.data());
        const Register *pSub1 = reinterpret_cast<const Register *>(sub1.data());
        const Register *pSub2 = reinterpret_cast<const Register *>(sub2.data());
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput++, pOutput++, pAdd1++, pAdd2++, pSub1++, pSub2++) {
            *pOutput = _mm_add_epi16(*pInput, *pAdd1);
            *pOutput = _mm_add_epi16(*pOutput, *pAdd2);
            *pOutput = _mm_sub_epi16(*pOutput, *pSub1);
            *pOutput = _mm_sub_epi16(*pOutput, *pSub2);
        }
    }

#elif defined(USE_NEON)

    static int32_t horizontalSum(int32x4_t value) noexcept {
        return vaddvq_s32(value);
    }

    static int32_t forward(const AlignedBuffer &inputs, const AlignedBuffer &weights) noexcept {
        int16x8_t min = vdupq_n_s16(0);
        int16x8_t max = vdupq_n_s16(Constants::NNUE_QUANT_A);
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

    static void add(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const std::int16_t *pInputs = inputs.data();
        std::int16_t *pOutputs = outputs.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs += Constants::SIMD_LANES, pOutputs += Constants::SIMD_LANES) {
            int16x8_t registerInputs = vld1q_s16(pInputs);
            int16x8_t registerOutputs = vld1q_s16(pOutputs);
            vst1q_s16(pOutputs, vaddq_s16(registerOutputs, registerInputs));
        }
    }

    static void sub(const AlignedBuffer &inputs, AlignedBuffer &outputs) noexcept {
        const std::int16_t *pInputs = inputs.data();
        std::int16_t *pOutputs = outputs.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pInputs += Constants::SIMD_LANES, pOutputs += Constants::SIMD_LANES) {
            int16x8_t registerInputs = vld1q_s16(pInputs);
            int16x8_t registerOutputs = vld1q_s16(pOutputs);
            vst1q_s16(pOutputs, vsubq_s16(registerOutputs, registerInputs));
        }
    }

    static void addSub(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub) noexcept {
        const std::int16_t *pInput = inputs.data();
        std::int16_t *pOutput = outputs.data();
        const std::int16_t *pAdd = add.data();
        const std::int16_t *pSub = sub.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput += Constants::SIMD_LANES, pOutput += Constants::SIMD_LANES, pAdd += Constants::SIMD_LANES, pSub += Constants::SIMD_LANES) {
            int16x8_t registerInput = vld1q_s16(pInput);
            int16x8_t registerAdd = vld1q_s16(pAdd);
            int16x8_t registerSub = vld1q_s16(pSub);
            vst1q_s16(pOutput, vsubq_s16(vaddq_s16(registerInput, registerAdd), registerSub));
        }
    }

    static void addSub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const std::int16_t *pInput = inputs.data();
        std::int16_t *pOutput = outputs.data();
        const std::int16_t *pAdd = add.data();
        const std::int16_t *pSub1 = sub1.data();
        const std::int16_t *pSub2 = sub2.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput += Constants::SIMD_LANES, pOutput += Constants::SIMD_LANES, pAdd += Constants::SIMD_LANES, pSub1 += Constants::SIMD_LANES, pSub2 += Constants::SIMD_LANES) {
            int16x8_t registerInput = vld1q_s16(pInput);
            int16x8_t registerAdd = vld1q_s16(pAdd);
            int16x8_t registerSub1 = vld1q_s16(pSub1);
            int16x8_t registerSub2 = vld1q_s16(pSub2);
            int16x8_t intermediate = vsubq_s16(vaddq_s16(registerInput, registerAdd), registerSub1);
            vst1q_s16(pOutput, vsubq_s16(intermediate, registerSub2));
        }
    }

    static void add2Sub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add1, const AlignedBuffer &add2, const AlignedBuffer &sub1, const AlignedBuffer &sub2) noexcept {
        const std::int16_t *pInput = inputs.data();
        std::int16_t *pOutput = outputs.data();
        const std::int16_t *pAdd1 = add1.data();
        const std::int16_t *pAdd2 = add2.data();
        const std::int16_t *pSub1 = sub1.data();
        const std::int16_t *pSub2 = sub2.data();
        for (std::size_t i = 0; i < ITERATIONS; i++, pInput += Constants::SIMD_LANES, pOutput += Constants::SIMD_LANES, pAdd1 += Constants::SIMD_LANES, pAdd2 += Constants::SIMD_LANES, pSub1 += Constants::SIMD_LANES, pSub2 += Constants::SIMD_LANES) {
            int16x8_t registerInput = vld1q_s16(pInput);
            int16x8_t registerAdd1 = vld1q_s16(pAdd1);
            int16x8_t registerAdd2 = vld1q_s16(pAdd2);
            int16x8_t registerSub1 = vld1q_s16(pSub1);
            int16x8_t registerSub2 = vld1q_s16(pSub2);
            int16x8_t intermediate = vaddq_s16(vaddq_s16(registerInput, registerAdd1), registerAdd2);
            intermediate = vsubq_s16(intermediate, registerSub1);
            vst1q_s16(pOutput, vsubq_s16(intermediate, registerSub2));
        }
    }

#else

    static int32_t screlu(int16_t input) {
        int16_t val = std::clamp(input, static_cast<int16_t>(0), static_cast<int16_t>(Constants::NNUE_QUANT_A));
        return val * val;
    }

    static int32_t forward(const AlignedBuffer &inputs, const AlignedBuffer &weights) {
        int32_t output = 0;
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            output += screlu(inputs[i]) * weights[i];
        }
        return output;
    }

    static void add(const AlignedBuffer &inputs, AlignedBuffer &outputs) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            outputs[i] += inputs[i];
        }
    }

    static void sub(const AlignedBuffer &inputs, AlignedBuffer &outputs) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            outputs[i] -= inputs[i];
        }
    }

    static void addSub(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            outputs[i] = inputs[i] + add[i] - sub[i];
        }
    }

    static void addSub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add, const AlignedBuffer &sub1, const AlignedBuffer &sub2) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            outputs[i] = inputs[i] + add[i] - sub1[i] - sub2[i];
        }
    }

    static void add2Sub2(const AlignedBuffer &inputs, AlignedBuffer &outputs, const AlignedBuffer &add1, const AlignedBuffer &add2, const AlignedBuffer &sub1, const AlignedBuffer &sub2) {
        for (std::size_t i = 0; i < Constants::NNUE_LAYER_SIZE; i++) {
            outputs[i] = inputs[i] + add1[i] + add2[i] - sub1[i] - sub2[i];
        }
    }

#endif

};

}
