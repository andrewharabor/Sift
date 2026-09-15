#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <span>

#include "reader.hpp"
#include "simd.hpp"
#include "sparse.hpp"
#include "types.hpp"

namespace Sift {
    template<typename FeatureSet, USize L1_SIZE, Int32 FT_QUANT, Int32 L1_QUANT, typename Activation, typename Output, Int32 SCALE>
    class SingleLayerArch {
    public:
        static constexpr bool PAIRWISE = false;
        static constexpr bool NEEDS_FT_PERMUTE = false;

    private:
        static constexpr USize OUTPUT_BUCKET_COUNT = Output::BUCKET_COUNT;

        static constexpr USize WEIGHT_SIZE = OUTPUT_BUCKET_COUNT * L1_SIZE * 2;
        static constexpr USize BIAS_SIZE = OUTPUT_BUCKET_COUNT;

        static constexpr Int32 QUANT = FT_QUANT * L1_QUANT;

        NET_PARAM(Int16, WEIGHT_SIZE, weights);
        NET_PARAM(Int16, BIAS_SIZE, biases);

    public:
        inline Int32 forward(USize bucket, std::span<const Int16, L1_SIZE> friendlyPSQInputs,
            std::span<const Int16, L1_SIZE> enemyPSQInputs, std::span<const Int16, L1_SIZE> friendlyThreatInputs,
            std::span<const Int16, L1_SIZE> enemyThreatInputs) const noexcept {
            const USize weightOffset = bucket * L1_SIZE * 2;
            const USize biasOffset = bucket;

            Vec<Int32> sum = SIMD::zero<Int32>();
            for (USize i = 0; i < L1_SIZE; i += SIMD::CHUNK_SIZE<Int16>) {
                Vec<Int16> friendlyInputs = SIMD::load<Int16>(&friendlyPSQInputs[i]);
                Vec<Int16> enemyInputs = SIMD::load<Int16>(&enemyPSQInputs[i]);
                if constexpr (FeatureSet::THREAT_INPUTS) {
                    friendlyInputs = SIMD::add<Int16>(friendlyInputs, SIMD::load<Int16>(&friendlyThreatInputs[i]));
                    enemyInputs = SIMD::add<Int16>(enemyInputs, SIMD::load<Int16>(&enemyThreatInputs[i]));
                }
                const Vec<Int16> friendlyWeights = SIMD::load<Int16>(&weights[weightOffset + i]);
                const Vec<Int16> enemyWeights = SIMD::load<Int16>(&weights[L1_SIZE + weightOffset + i]);
                sum = Activation::template actDotAcc<static_cast<Int16>(FT_QUANT)>(sum, friendlyInputs, friendlyWeights);
                sum = Activation::template actDotAcc<static_cast<Int16>(FT_QUANT)>(sum, enemyInputs, enemyWeights);
            }

            const Int32 total = SIMD::horizAdd<Int32>(sum);
            const Int32 bias = static_cast<Int32>(biases[biasOffset]);
            const Int32 output = bias + Activation::template output<FT_QUANT>(total);
            return output * SCALE / QUANT;
        }

        inline bool load(ByteReader& reader) noexcept { return reader.load(weights) && reader.load(biases); }

        static constexpr USize byteSize() noexcept { return sizeof(Int16) * WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE; }

        template<typename Type>
        static inline void permuteParam(std::span<Type>) {}

        static inline void permuteFTParams(std::span<const Int16>, std::span<const Int8>, std::span<const Int16>) {}
    };

    template<typename FeatureSet, USize L1_SIZE, USize L2_SIZE, USize L3_SIZE, Int32 FT_SCALE_BITS, Int32 FT_QUANT_BITS,
        Int32 L1_QUANT_BITS, bool DUAL_ACTIVATION, bool SKIP_L2, typename Output, Int32 SCALE>
    class PairwiseMultilayerArch {
        static_assert(L2_SIZE % 16 == 0);
        static_assert(L3_SIZE % 16 == 0);

    public:
        static constexpr bool PAIRWISE = true;
        static constexpr bool NEEDS_FT_PERMUTE = SIMD::PACK_REORDER;

    private:
        static constexpr USize CHUNK8 = SIMD::CHUNK_SIZE<Int8>;
        static constexpr USize CHUNK16 = SIMD::CHUNK_SIZE<Int16>;
        static constexpr USize CHUNK32 = SIMD::CHUNK_SIZE<Int32>;
        static constexpr USize CHUNK8_32 = sizeof(Int32) / sizeof(Int8);

        static constexpr USize L1_PAIRS = L1_SIZE / 2;
        static constexpr USize L2_FULL_SIZE = L2_SIZE * (1 + DUAL_ACTIVATION);

        static_assert(L1_PAIRS % (CHUNK16 * 4) == 0);
        static_assert(!SKIP_L2 || L2_FULL_SIZE == L3_SIZE);

        static constexpr USize OUTPUT_BUCKET_COUNT = Output::BUCKET_COUNT;

        static constexpr USize L1_WEIGHT_SIZE = OUTPUT_BUCKET_COUNT * L1_SIZE * L2_SIZE;
        static constexpr USize L1_BIAS_SIZE = OUTPUT_BUCKET_COUNT * L2_SIZE;
        static constexpr USize L2_WEIGHT_SIZE = OUTPUT_BUCKET_COUNT * L2_FULL_SIZE * L3_SIZE;
        static constexpr USize L2_BIAS_SIZE = OUTPUT_BUCKET_COUNT * L3_SIZE;
        static constexpr USize L3_WEIGHT_SIZE = OUTPUT_BUCKET_COUNT * L3_SIZE;
        static constexpr USize L3_BIAS_SIZE = OUTPUT_BUCKET_COUNT;

        NET_PARAM(Int8, L1_WEIGHT_SIZE, l1Weights);
        NET_PARAM(Int32, L1_BIAS_SIZE, l1Biases);
        NET_PARAM(Int32, L2_WEIGHT_SIZE, l2Weights);
        NET_PARAM(Int32, L2_BIAS_SIZE, l2Biases);
        NET_PARAM(Int32, L3_WEIGHT_SIZE, l3Weights);
        NET_PARAM(Int32, L3_BIAS_SIZE, l3Biases);

        static constexpr Int32 QUANT_BITS = 6;
        static constexpr Int32 QUANT = 1 << QUANT_BITS;
        static constexpr Int32 SHIFT1 = 16 + QUANT_BITS + QUANT_BITS - FT_SCALE_BITS - FT_QUANT_BITS - FT_QUANT_BITS - L1_QUANT_BITS;
        static constexpr Int32 SHIFT2 = (SKIP_L2) ? QUANT_BITS : 2 * QUANT_BITS;

        static constexpr Int64 SCALE64 = static_cast<Int64>(SCALE);
        static constexpr Int64 QUANT64 = static_cast<Int64>(QUANT);

        inline void activateFT(std::span<const Int16, L1_SIZE> friendlyPSQInputs, std::span<const Int16, L1_SIZE> enemyPSQInputs,
            std::span<const Int16, L1_SIZE> friendlyThreatInputs, std::span<const Int16, L1_SIZE> enemyThreatInputs,
            std::span<UInt8, L1_SIZE> outputs, SparseIterator<L1_SIZE>& sparseIter) const noexcept {
            const Vec<Int16> zero = SIMD::zero<Int16>();
            const Vec<Int16> quant = SIMD::set<Int16>((1 << FT_QUANT_BITS) - 1);

            const auto activate = [&](std::span<const Int16, L1_SIZE> psqInputs, std::span<const Int16, L1_SIZE> threatInputs,
                                      USize offset) {
                for (USize i = 0; i < L1_PAIRS; i += CHUNK16 * 4) {
                    Vec<Int16> in1_0 = SIMD::load<Int16>(&psqInputs[i + CHUNK16 * 0]);
                    Vec<Int16> in1_1 = SIMD::load<Int16>(&psqInputs[i + CHUNK16 * 1]);
                    Vec<Int16> in1_2 = SIMD::load<Int16>(&psqInputs[i + CHUNK16 * 2]);
                    Vec<Int16> in1_3 = SIMD::load<Int16>(&psqInputs[i + CHUNK16 * 3]);
                    Vec<Int16> in2_0 = SIMD::load<Int16>(&psqInputs[L1_PAIRS + i + CHUNK16 * 0]);
                    Vec<Int16> in2_1 = SIMD::load<Int16>(&psqInputs[L1_PAIRS + i + CHUNK16 * 1]);
                    Vec<Int16> in2_2 = SIMD::load<Int16>(&psqInputs[L1_PAIRS + i + CHUNK16 * 2]);
                    Vec<Int16> in2_3 = SIMD::load<Int16>(&psqInputs[L1_PAIRS + i + CHUNK16 * 3]);
                    if constexpr (FeatureSet::THREAT_INPUTS) {
                        in1_0 = SIMD::add<Int16>(in1_0, SIMD::load<Int16>(&threatInputs[i + CHUNK16 * 0]));
                        in1_1 = SIMD::add<Int16>(in1_1, SIMD::load<Int16>(&threatInputs[i + CHUNK16 * 1]));
                        in1_2 = SIMD::add<Int16>(in1_2, SIMD::load<Int16>(&threatInputs[i + CHUNK16 * 2]));
                        in1_3 = SIMD::add<Int16>(in1_3, SIMD::load<Int16>(&threatInputs[i + CHUNK16 * 3]));
                        in2_0 = SIMD::add<Int16>(in2_0, SIMD::load<Int16>(&threatInputs[L1_PAIRS + i + CHUNK16 * 0]));
                        in2_1 = SIMD::add<Int16>(in2_1, SIMD::load<Int16>(&threatInputs[L1_PAIRS + i + CHUNK16 * 1]));
                        in2_2 = SIMD::add<Int16>(in2_2, SIMD::load<Int16>(&threatInputs[L1_PAIRS + i + CHUNK16 * 2]));
                        in2_3 = SIMD::add<Int16>(in2_3, SIMD::load<Int16>(&threatInputs[L1_PAIRS + i + CHUNK16 * 3]));
                    }
                    in1_0 = SIMD::clamp<Int16>(in1_0, zero, quant);
                    in1_1 = SIMD::clamp<Int16>(in1_1, zero, quant);
                    in1_2 = SIMD::clamp<Int16>(in1_2, zero, quant);
                    in1_3 = SIMD::clamp<Int16>(in1_3, zero, quant);
                    in2_0 = SIMD::clamp<Int16>(in2_0, zero, quant);
                    in2_1 = SIMD::clamp<Int16>(in2_1, zero, quant);
                    in2_2 = SIMD::clamp<Int16>(in2_2, zero, quant);
                    in2_3 = SIMD::clamp<Int16>(in2_3, zero, quant);
                    const Vec<Int16> prod_0 = SIMD::shiftLeftMulHi<Int16>(in1_0, in2_0, FT_SCALE_BITS);
                    const Vec<Int16> prod_1 = SIMD::shiftLeftMulHi<Int16>(in1_1, in2_1, FT_SCALE_BITS);
                    const Vec<Int16> prod_2 = SIMD::shiftLeftMulHi<Int16>(in1_2, in2_2, FT_SCALE_BITS);
                    const Vec<Int16> prod_3 = SIMD::shiftLeftMulHi<Int16>(in1_3, in2_3, FT_SCALE_BITS);
                    const Vec<UInt8> packed_0 = SIMD::packUs<Int16>(prod_0, prod_1);
                    const Vec<UInt8> packed_1 = SIMD::packUs<Int16>(prod_2, prod_3);
                    SIMD::store<UInt8>(&outputs[offset + i + CHUNK8 * 0], packed_0);
                    SIMD::store<UInt8>(&outputs[offset + i + CHUNK8 * 1], packed_1);
                }
            };

            activate(friendlyPSQInputs, friendlyThreatInputs, 0);
            activate(enemyPSQInputs, enemyThreatInputs, L1_PAIRS);

            for (USize i = 0; i < L1_SIZE; i += CHUNK8 * 4) {
                const Vec<UInt8> vec_0 = SIMD::load<UInt8>(&outputs[i + CHUNK8 * 0]);
                const Vec<UInt8> vec_1 = SIMD::load<UInt8>(&outputs[i + CHUNK8 * 1]);
                const Vec<UInt8> vec_2 = SIMD::load<UInt8>(&outputs[i + CHUNK8 * 2]);
                const Vec<UInt8> vec_3 = SIMD::load<UInt8>(&outputs[i + CHUNK8 * 3]);
                sparseIter.update(vec_0, vec_1);
                sparseIter.update(vec_2, vec_3);
            }
        }

        inline void forwardL1(USize bucket, std::span<const UInt8, L1_SIZE> inputs, std::span<Int32, L2_FULL_SIZE> outputs,
            const SparseIterator<L1_SIZE>& sparseIter) const noexcept {
            const USize weightOffset = bucket * L1_SIZE * L2_SIZE;
            const USize biasOffset = bucket * L2_SIZE;

            const Int32* inputs32 = reinterpret_cast<const Int32*>(inputs.data());

            alignas(SIMD::ALIGNMENT) MultiArray<Vec<Int32>, L2_SIZE / CHUNK32, 4> matMul = {};

            const USize sparseCount4 = sparseIter.count() - (sparseIter.count() % 4);
            for (USize i = 0; i < sparseCount4; i += 4) {
                const USize idx_0 = sparseIter.index(i + 0);
                const USize idx_1 = sparseIter.index(i + 1);
                const USize idx_2 = sparseIter.index(i + 2);
                const USize idx_3 = sparseIter.index(i + 3);
                const USize wIdx_0 = weightOffset + idx_0 * CHUNK8_32 * L2_SIZE;
                const USize wIdx_1 = weightOffset + idx_1 * CHUNK8_32 * L2_SIZE;
                const USize wIdx_2 = weightOffset + idx_2 * CHUNK8_32 * L2_SIZE;
                const USize wIdx_3 = weightOffset + idx_3 * CHUNK8_32 * L2_SIZE;
                const Vec<Int32> in_0 = SIMD::set<Int32>(inputs32[idx_0]);
                const Vec<Int32> in_1 = SIMD::set<Int32>(inputs32[idx_1]);
                const Vec<Int32> in_2 = SIMD::set<Int32>(inputs32[idx_2]);
                const Vec<Int32> in_3 = SIMD::set<Int32>(inputs32[idx_3]);
                for (USize j = 0; j < L2_SIZE; j += CHUNK32) {
                    const Vec<Int8> weight_0 = SIMD::load<Int8>(&l1Weights[wIdx_0 + CHUNK8_32 * j]);
                    const Vec<Int8> weight_1 = SIMD::load<Int8>(&l1Weights[wIdx_1 + CHUNK8_32 * j]);
                    const Vec<Int8> weight_2 = SIMD::load<Int8>(&l1Weights[wIdx_2 + CHUNK8_32 * j]);
                    const Vec<Int8> weight_3 = SIMD::load<Int8>(&l1Weights[wIdx_3 + CHUNK8_32 * j]);
                    auto& dotProds = matMul[j / CHUNK32];
                    dotProds[0] = SIMD::dotProd<Int32>(dotProds[0], in_0, weight_0);
                    dotProds[1] = SIMD::dotProd<Int32>(dotProds[1], in_1, weight_1);
                    dotProds[2] = SIMD::dotProd<Int32>(dotProds[2], in_2, weight_2);
                    dotProds[3] = SIMD::dotProd<Int32>(dotProds[3], in_3, weight_3);
                }
            }
            for (USize i = sparseCount4; i < sparseIter.count(); i++) {
                const USize idx = sparseIter.index(i);
                const USize wIdx = weightOffset + idx * CHUNK8_32 * L2_SIZE;
                const Vec<Int32> input = SIMD::set<Int32>(inputs32[idx]);
                for (USize j = 0; j < L2_SIZE; j += CHUNK32) {
                    const Vec<Int8> weight = SIMD::load<Int8>(&l1Weights[wIdx + CHUNK8_32 * j]);
                    auto& dotProds = matMul[j / CHUNK32];
                    dotProds[0] = SIMD::dotProd<Int32>(dotProds[0], input, weight);
                }
            }

            for (USize i = 0; i < L2_SIZE; i += CHUNK32) {
                const auto& dotProds = matMul[i / CHUNK32];
                const Vec<Int32> sum =
                    SIMD::add<Int32>(SIMD::add<Int32>(dotProds[0], dotProds[1]), SIMD::add<Int32>(dotProds[2], dotProds[3]));
                const Vec<Int32> biases = SIMD::load<Int32>(&l1Biases[biasOffset + i]);
                Vec<Int32> out = SIMD::add<Int32>(SIMD::shift<Int32, SHIFT1>(sum), biases);
                if constexpr (DUAL_ACTIVATION) {
                    Vec<Int32> out1 = SIMD::clamp<Int32>(out, SIMD::zero<Int32>(), SIMD::set<Int32>(QUANT * QUANT));
                    if constexpr (SKIP_L2) { out1 = SIMD::shiftLeft<Int32>(out1, QUANT_BITS); }
                    Vec<Int32> out2 = SIMD::mulLo<Int32>(out, out);
                    out2 = SIMD::clamp<Int32>(out2, SIMD::zero<Int32>(), SIMD::set<Int32>(QUANT * QUANT * QUANT * QUANT));
                    out2 = SIMD::shiftRight<Int32>(out2, SHIFT2);
                    SIMD::store<Int32>(&outputs[i], out1);
                    SIMD::store<Int32>(&outputs[L2_SIZE + i], out2);
                } else {
                    out = SIMD::clamp<Int32>(out, SIMD::zero<Int32>(), SIMD::set<Int32>(QUANT * QUANT));
                    out = SIMD::mulLo<Int32>(out, out);
                    out = SIMD::shiftRight<Int32>(out, SHIFT2);
                    SIMD::store<Int32>(&outputs[i], out);
                }
            }
        }

        inline void propagateL2(USize bucket, std::span<const Int32, L2_FULL_SIZE> inputs,
            std::span<Int32, L3_SIZE> outputs) const noexcept {
            const USize weightOffset = bucket * L2_FULL_SIZE * L3_SIZE;
            const USize biasOffset = bucket * L3_SIZE;

            std::memcpy(outputs.data(), &l2Biases[biasOffset], outputs.size_bytes());

            if constexpr (CHUNK32 * 4 > L3_SIZE) {
                for (USize i = 0; i < L2_FULL_SIZE; i++) {
                    Int32 input = inputs[i];
                    if constexpr (SKIP_L2) { input >>= QUANT_BITS; }
                    const Vec<Int32> in = SIMD::set<Int32>(input);
                    for (USize j = 0; j < L3_SIZE; j += CHUNK32 * 2) {
                        const Vec<Int32> weight_0 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 0]);
                        const Vec<Int32> weight_1 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 1]);
                        const Vec<Int32> prod_0 = SIMD::mulLo<Int32>(in, weight_0);
                        const Vec<Int32> prod_1 = SIMD::mulLo<Int32>(in, weight_1);
                        Vec<Int32> out_0 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 0]);
                        Vec<Int32> out_1 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 1]);
                        out_0 = SIMD::add<Int32>(out_0, prod_0);
                        out_1 = SIMD::add<Int32>(out_1, prod_1);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 0], out_0);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 1], out_1);
                    }
                }
            } else {
                for (USize i = 0; i < L2_FULL_SIZE; i++) {
                    Int32 input = inputs[i];
                    if constexpr (SKIP_L2) { input >>= QUANT_BITS; }
                    const Vec<Int32> in = SIMD::set<Int32>(input);
                    for (USize j = 0; j < L3_SIZE; j += CHUNK32 * 4) {
                        const Vec<Int32> weight_0 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 0]);
                        const Vec<Int32> weight_1 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 1]);
                        const Vec<Int32> weight_2 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 2]);
                        const Vec<Int32> weight_3 = SIMD::load<Int32>(&l2Weights[weightOffset + i * L3_SIZE + j + CHUNK32 * 3]);
                        const Vec<Int32> prod_0 = SIMD::mulLo<Int32>(in, weight_0);
                        const Vec<Int32> prod_1 = SIMD::mulLo<Int32>(in, weight_1);
                        const Vec<Int32> prod_2 = SIMD::mulLo<Int32>(in, weight_2);
                        const Vec<Int32> prod_3 = SIMD::mulLo<Int32>(in, weight_3);
                        Vec<Int32> out_0 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 0]);
                        Vec<Int32> out_1 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 1]);
                        Vec<Int32> out_2 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 2]);
                        Vec<Int32> out_3 = SIMD::load<Int32>(&outputs[j + CHUNK32 * 3]);
                        out_0 = SIMD::add<Int32>(out_0, prod_0);
                        out_1 = SIMD::add<Int32>(out_1, prod_1);
                        out_2 = SIMD::add<Int32>(out_2, prod_2);
                        out_3 = SIMD::add<Int32>(out_3, prod_3);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 0], out_0);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 1], out_1);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 2], out_2);
                        SIMD::store<Int32>(&outputs[j + CHUNK32 * 3], out_3);
                    }
                }
            }
        }

        inline void activateForwardL3(USize bucket, [[maybe_unused]] std::span<const Int32, L2_FULL_SIZE> skipped,
            std::span<const Int32, L3_SIZE> inputs, Int32& output) const noexcept {
            const USize weightOffset = bucket * L3_SIZE;
            const USize biasOffset = bucket;

            const Vec<Int32> zero = SIMD::zero<Int32>();
            const Vec<Int32> quant = SIMD::set<Int32>(QUANT * QUANT * QUANT);

            Vec<Int32> sum;

            if constexpr (CHUNK32 * 4 > L3_SIZE) {
                Vec<Int32> out_0 = zero;
                Vec<Int32> out_1 = zero;
                for (USize i = 0; i < L3_SIZE; i += CHUNK32 * 2) {
                    Vec<Int32> in_0 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 0]);
                    Vec<Int32> in_1 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 1]);
                    in_0 = SIMD::clamp<Int32>(in_0, zero, quant);
                    in_1 = SIMD::clamp<Int32>(in_1, zero, quant);
                    if constexpr (SKIP_L2) {
                        const Vec<Int32> skip_0 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 0]);
                        const Vec<Int32> skip_1 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 1]);
                        in_0 = SIMD::add<Int32>(in_0, skip_0);
                        in_1 = SIMD::add<Int32>(in_1, skip_1);
                    }
                    const Vec<Int32> weight_0 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 0]);
                    const Vec<Int32> weight_1 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 1]);
                    in_0 = SIMD::mulLo<Int32>(in_0, weight_0);
                    in_1 = SIMD::mulLo<Int32>(in_1, weight_1);
                    out_0 = SIMD::add<Int32>(out_0, in_0);
                    out_1 = SIMD::add<Int32>(out_1, in_1);
                }

                sum = SIMD::add<Int32>(out_0, out_1);
            } else {
                Vec<Int32> out_0 = zero;
                Vec<Int32> out_1 = zero;
                Vec<Int32> out_2 = zero;
                Vec<Int32> out_3 = zero;
                for (USize i = 0; i < L3_SIZE; i += CHUNK32 * 4) {
                    Vec<Int32> in_0 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 0]);
                    Vec<Int32> in_1 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 1]);
                    Vec<Int32> in_2 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 2]);
                    Vec<Int32> in_3 = SIMD::load<Int32>(&inputs[i + CHUNK32 * 3]);
                    in_0 = SIMD::clamp<Int32>(in_0, zero, quant);
                    in_1 = SIMD::clamp<Int32>(in_1, zero, quant);
                    in_2 = SIMD::clamp<Int32>(in_2, zero, quant);
                    in_3 = SIMD::clamp<Int32>(in_3, zero, quant);
                    if constexpr (SKIP_L2) {
                        const Vec<Int32> skip_0 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 0]);
                        const Vec<Int32> skip_1 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 1]);
                        const Vec<Int32> skip_2 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 2]);
                        const Vec<Int32> skip_3 = SIMD::load<Int32>(&skipped[i + CHUNK32 * 3]);
                        in_0 = SIMD::add<Int32>(in_0, skip_0);
                        in_1 = SIMD::add<Int32>(in_1, skip_1);
                        in_2 = SIMD::add<Int32>(in_2, skip_2);
                        in_3 = SIMD::add<Int32>(in_3, skip_3);
                    }
                    const Vec<Int32> weight_0 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 0]);
                    const Vec<Int32> weight_1 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 1]);
                    const Vec<Int32> weight_2 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 2]);
                    const Vec<Int32> weight_3 = SIMD::load<Int32>(&l3Weights[weightOffset + i + CHUNK32 * 3]);
                    in_0 = SIMD::mulLo<Int32>(in_0, weight_0);
                    in_1 = SIMD::mulLo<Int32>(in_1, weight_1);
                    in_2 = SIMD::mulLo<Int32>(in_2, weight_2);
                    in_3 = SIMD::mulLo<Int32>(in_3, weight_3);
                    out_0 = SIMD::add<Int32>(out_0, in_0);
                    out_1 = SIMD::add<Int32>(out_1, in_1);
                    out_2 = SIMD::add<Int32>(out_2, in_2);
                    out_3 = SIMD::add<Int32>(out_3, in_3);
                }

                const Vec<Int32> sum1 = SIMD::add<Int32>(out_0, out_1);
                const Vec<Int32> sum2 = SIMD::add<Int32>(out_2, out_3);
                sum = SIMD::add<Int32>(sum1, sum2);
            }

            output = SIMD::horizAdd<Int32>(sum) + l3Biases[biasOffset];
        }

    public:
        inline Int32 forward(USize bucket, std::span<const Int16, L1_SIZE> friendlyPSQInputs,
            std::span<const Int16, L1_SIZE> enemyPSQInputs, std::span<const Int16, L1_SIZE> friendlyThreatInputs,
            std::span<const Int16, L1_SIZE> enemyThreatInputs) const noexcept {
            SparseIterator<L1_SIZE> sparseIter;

            alignas(SIMD::ALIGNMENT) std::array<UInt8, L1_SIZE> ftOut = {};
            alignas(SIMD::ALIGNMENT) std::array<Int32, L2_FULL_SIZE> l1Out = {};
            alignas(SIMD::ALIGNMENT) std::array<Int32, L3_SIZE> l2Out = {};
            Int32 l3Out = 0;

            activateFT(friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs, ftOut, sparseIter);
            forwardL1(bucket, ftOut, l1Out, sparseIter);
            propagateL2(bucket, l1Out, l2Out);
            activateForwardL3(bucket, l1Out, l2Out, l3Out);

#if defined(MEASURE_SPARSITY)
            SparseIterator<L1_SIZE>::trackFtActs(ftOut);
#endif

            Int64 output = static_cast<Int64>(l3Out);
            output *= SCALE64;
            output /= QUANT64 * QUANT64 * QUANT64 * QUANT64;
            return static_cast<Int32>(output);
        }

        inline bool load(ByteReader& reader) noexcept {
            return reader.load(l1Weights) && reader.load(l1Biases) && reader.load(l2Weights) && reader.load(l2Biases) &&
                   reader.load(l3Weights) && reader.load(l3Biases);
        }

        static constexpr USize byteSize() noexcept {
            return sizeof(Int8) * L1_WEIGHT_SIZE + sizeof(Int32) * L1_BIAS_SIZE + sizeof(Int32) * L2_WEIGHT_SIZE +
                   sizeof(Int32) * L2_BIAS_SIZE + sizeof(Int32) * L3_WEIGHT_SIZE + sizeof(Int32) * L3_BIAS_SIZE;
        };

        template<typename Type>
        static inline void permuteParam(std::span<Type> param) noexcept {
            if constexpr (!SIMD::PACK_REORDER) { return; }

            static constexpr USize CHUNK_SIZE = SIMD::PACK_SIZE * SIMD::PACK_GROUPING;

            MultiArray<Type, SIMD::PACK_SIZE, SIMD::PACK_GROUPING> tmp = {};
            for (USize i = 0; i < param.size(); i += CHUNK_SIZE) {
                std::copy(&param[i], &param[i] + CHUNK_SIZE, &tmp[0][0]);
                for (USize j = 0; j < SIMD::PACK_SIZE; j++) {
                    std::ranges::copy(tmp[SIMD::PACK_ORDERING[j]], &param[i + j * SIMD::PACK_GROUPING]);
                }
            }
        }

        static inline void permuteFTParams(std::span<const Int16> psqWeights, std::span<const Int8> threatWeights,
            std::span<const Int16> biases) {
            if constexpr (!SIMD::PACK_REORDER) { return; }

            const auto unconst = []<typename Type>(
                                     std::span<const Type> x) { return std::span<Type>{const_cast<Type*>(x.data()), x.size()}; };

            permuteParam(unconst(psqWeights));
            permuteParam(unconst(threatWeights));
            permuteParam(unconst(biases));
        }
    };
}
