#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

#include "reader.hpp"
#include "simd.hpp"
#include "sparse.hpp"
#include "types.hpp"

namespace Sift {
    class Multilayer {
        static_assert(Arch::L2_SIZE % 16 == 0);
        static_assert(Arch::L3_SIZE % 16 == 0);

    private:
        static constexpr USize CHUNK8 = SIMD::CHUNK_SIZE_8;
        static constexpr USize CHUNK16 = SIMD::CHUNK_SIZE_16;
        static constexpr USize CHUNK32 = SIMD::CHUNK_SIZE_32;
        static constexpr USize CHUNK8_32 = sizeof(Int32) / sizeof(Int8);

        static constexpr USize L1_PAIRS = Arch::L1_SIZE / 2;
        static constexpr USize L2_FULL_SIZE = Arch::L2_SIZE * (1 + Arch::DUAL_ACTIVATION);

        static_assert(L1_PAIRS % (CHUNK16 * 4) == 0);
        static_assert(!Arch::SKIP_L2 || L2_FULL_SIZE == Arch::L3_SIZE);

        static constexpr USize L1_WEIGHT_SIZE = Arch::OUTPUT_BUCKETS * Arch::L1_SIZE * Arch::L2_SIZE;
        static constexpr USize L1_BIAS_SIZE = Arch::OUTPUT_BUCKETS * Arch::L2_SIZE;
        static constexpr USize L2_WEIGHT_SIZE = Arch::OUTPUT_BUCKETS * L2_FULL_SIZE * Arch::L3_SIZE;
        static constexpr USize L2_BIAS_SIZE = Arch::OUTPUT_BUCKETS * Arch::L3_SIZE;
        static constexpr USize L3_WEIGHT_SIZE = Arch::OUTPUT_BUCKETS * Arch::L3_SIZE;
        static constexpr USize L3_BIAS_SIZE = Arch::OUTPUT_BUCKETS;

        NET_PARAM(Int8, L1_WEIGHT_SIZE, l1Weights);
        NET_PARAM(Int32, L1_BIAS_SIZE, l1Biases);
        NET_PARAM(Int32, L2_WEIGHT_SIZE, l2Weights);
        NET_PARAM(Int32, L2_BIAS_SIZE, l2Biases);
        NET_PARAM(Int32, L3_WEIGHT_SIZE, l3Weights);
        NET_PARAM(Int32, L3_BIAS_SIZE, l3Biases);

        static constexpr Int32 QUANT_BITS = 6;
        static constexpr Int32 QUANT = 1 << QUANT_BITS;
        static constexpr Int32 SHIFT1 =
            16 + QUANT_BITS + QUANT_BITS - Arch::FT_SCALE_BITS - Arch::FT_QUANT_BITS - Arch::FT_QUANT_BITS - Arch::L1_QUANT_BITS;
        static constexpr Int32 SHIFT2 = (Arch::SKIP_L2) ? QUANT_BITS : 2 * QUANT_BITS;

        static constexpr Int64 SCALE64 = static_cast<Int64>(Arch::SCALE);
        static constexpr Int64 QUANT64 = static_cast<Int64>(QUANT);

        FORCE_INLINE void activateFT(std::span<const Int16, Arch::L1_SIZE> friendlyPSQInputs,
            std::span<const Int16, Arch::L1_SIZE> enemyPSQInputs, std::span<const Int16, Arch::L1_SIZE> friendlyThreatInputs,
            std::span<const Int16, Arch::L1_SIZE> enemyThreatInputs, std::span<UInt8, Arch::L1_SIZE> outputs,
            SparseIterator& sparseIter) const noexcept {
            const VecInt16 zero = SIMD::zeroInt16();
            const VecInt16 quant = SIMD::setInt16((1 << Arch::FT_QUANT_BITS) - 1);

            const auto activate = [&](std::span<const Int16, Arch::L1_SIZE> psqInputs, std::span<const Int16, Arch::L1_SIZE> threatInputs,
                                      USize offset) {
                for (USize i = 0; i < L1_PAIRS; i += CHUNK16 * 4) {
                    VecInt16 in1_0 = SIMD::loadInt16(&psqInputs[i + CHUNK16 * 0]);
                    VecInt16 in1_1 = SIMD::loadInt16(&psqInputs[i + CHUNK16 * 1]);
                    VecInt16 in1_2 = SIMD::loadInt16(&psqInputs[i + CHUNK16 * 2]);
                    VecInt16 in1_3 = SIMD::loadInt16(&psqInputs[i + CHUNK16 * 3]);
                    VecInt16 in2_0 = SIMD::loadInt16(&psqInputs[L1_PAIRS + i + CHUNK16 * 0]);
                    VecInt16 in2_1 = SIMD::loadInt16(&psqInputs[L1_PAIRS + i + CHUNK16 * 1]);
                    VecInt16 in2_2 = SIMD::loadInt16(&psqInputs[L1_PAIRS + i + CHUNK16 * 2]);
                    VecInt16 in2_3 = SIMD::loadInt16(&psqInputs[L1_PAIRS + i + CHUNK16 * 3]);
                    in1_0 = SIMD::addInt16(in1_0, SIMD::loadInt16(&threatInputs[i + CHUNK16 * 0]));
                    in1_1 = SIMD::addInt16(in1_1, SIMD::loadInt16(&threatInputs[i + CHUNK16 * 1]));
                    in1_2 = SIMD::addInt16(in1_2, SIMD::loadInt16(&threatInputs[i + CHUNK16 * 2]));
                    in1_3 = SIMD::addInt16(in1_3, SIMD::loadInt16(&threatInputs[i + CHUNK16 * 3]));
                    in2_0 = SIMD::addInt16(in2_0, SIMD::loadInt16(&threatInputs[L1_PAIRS + i + CHUNK16 * 0]));
                    in2_1 = SIMD::addInt16(in2_1, SIMD::loadInt16(&threatInputs[L1_PAIRS + i + CHUNK16 * 1]));
                    in2_2 = SIMD::addInt16(in2_2, SIMD::loadInt16(&threatInputs[L1_PAIRS + i + CHUNK16 * 2]));
                    in2_3 = SIMD::addInt16(in2_3, SIMD::loadInt16(&threatInputs[L1_PAIRS + i + CHUNK16 * 3]));
                    in1_0 = SIMD::clampInt16(in1_0, zero, quant);
                    in1_1 = SIMD::clampInt16(in1_1, zero, quant);
                    in1_2 = SIMD::clampInt16(in1_2, zero, quant);
                    in1_3 = SIMD::clampInt16(in1_3, zero, quant);
                    in2_0 = SIMD::minInt16(in2_0, quant);
                    in2_1 = SIMD::minInt16(in2_1, quant);
                    in2_2 = SIMD::minInt16(in2_2, quant);
                    in2_3 = SIMD::minInt16(in2_3, quant);
                    const VecInt16 prod_0 = SIMD::shiftLeftMulHiInt16(in1_0, in2_0, Arch::FT_SCALE_BITS);
                    const VecInt16 prod_1 = SIMD::shiftLeftMulHiInt16(in1_1, in2_1, Arch::FT_SCALE_BITS);
                    const VecInt16 prod_2 = SIMD::shiftLeftMulHiInt16(in1_2, in2_2, Arch::FT_SCALE_BITS);
                    const VecInt16 prod_3 = SIMD::shiftLeftMulHiInt16(in1_3, in2_3, Arch::FT_SCALE_BITS);
                    const VecUInt8 packed_0 = SIMD::packUsInt16(prod_0, prod_1);
                    const VecUInt8 packed_1 = SIMD::packUsInt16(prod_2, prod_3);
                    SIMD::storeUInt8(&outputs[offset + i + CHUNK8 * 0], packed_0);
                    SIMD::storeUInt8(&outputs[offset + i + CHUNK8 * 1], packed_1);
                }
            };

            activate(friendlyPSQInputs, friendlyThreatInputs, 0);
            activate(enemyPSQInputs, enemyThreatInputs, L1_PAIRS);

            for (USize i = 0; i < Arch::L1_SIZE; i += CHUNK8 * 4) {
                const VecUInt8 vec_0 = SIMD::loadUInt8(&outputs[i + CHUNK8 * 0]);
                const VecUInt8 vec_1 = SIMD::loadUInt8(&outputs[i + CHUNK8 * 1]);
                const VecUInt8 vec_2 = SIMD::loadUInt8(&outputs[i + CHUNK8 * 2]);
                const VecUInt8 vec_3 = SIMD::loadUInt8(&outputs[i + CHUNK8 * 3]);
                sparseIter.update(vec_0, vec_1);
                sparseIter.update(vec_2, vec_3);
            }
        }

        FORCE_INLINE void forwardL1(USize bucket, std::span<const UInt8, Arch::L1_SIZE> inputs, std::span<Int32, L2_FULL_SIZE> outputs,
            const SparseIterator& sparseIter) const noexcept {
            const USize weightOffset = bucket * Arch::L1_SIZE * Arch::L2_SIZE;
            const USize biasOffset = bucket * Arch::L2_SIZE;

            const Int32* inputs32 = reinterpret_cast<const Int32*>(inputs.data());

            alignas(SIMD::ALIGNMENT) MultiArray<VecInt32, Arch::L2_SIZE / CHUNK32, 4> matMul = {};

            const USize sparseCount4 = sparseIter.count() - (sparseIter.count() % 4);
            for (USize i = 0; i < sparseCount4; i += 4) {
                const USize idx_0 = sparseIter.index(i + 0);
                const USize idx_1 = sparseIter.index(i + 1);
                const USize idx_2 = sparseIter.index(i + 2);
                const USize idx_3 = sparseIter.index(i + 3);
                const USize wIdx_0 = weightOffset + idx_0 * CHUNK8_32 * Arch::L2_SIZE;
                const USize wIdx_1 = weightOffset + idx_1 * CHUNK8_32 * Arch::L2_SIZE;
                const USize wIdx_2 = weightOffset + idx_2 * CHUNK8_32 * Arch::L2_SIZE;
                const USize wIdx_3 = weightOffset + idx_3 * CHUNK8_32 * Arch::L2_SIZE;
                const VecInt32 in_0 = SIMD::setInt32(inputs32[idx_0]);
                const VecInt32 in_1 = SIMD::setInt32(inputs32[idx_1]);
                const VecInt32 in_2 = SIMD::setInt32(inputs32[idx_2]);
                const VecInt32 in_3 = SIMD::setInt32(inputs32[idx_3]);
                for (USize j = 0; j < Arch::L2_SIZE; j += CHUNK32) {
                    const VecInt8 weight_0 = SIMD::loadInt8(&l1Weights[wIdx_0 + CHUNK8_32 * j]);
                    const VecInt8 weight_1 = SIMD::loadInt8(&l1Weights[wIdx_1 + CHUNK8_32 * j]);
                    const VecInt8 weight_2 = SIMD::loadInt8(&l1Weights[wIdx_2 + CHUNK8_32 * j]);
                    const VecInt8 weight_3 = SIMD::loadInt8(&l1Weights[wIdx_3 + CHUNK8_32 * j]);
                    auto& dotProds = matMul[j / CHUNK32];
                    dotProds[0] = SIMD::dotProdUInt8Int8(dotProds[0], in_0, weight_0);
                    dotProds[1] = SIMD::dotProdUInt8Int8(dotProds[1], in_1, weight_1);
                    dotProds[2] = SIMD::dotProdUInt8Int8(dotProds[2], in_2, weight_2);
                    dotProds[3] = SIMD::dotProdUInt8Int8(dotProds[3], in_3, weight_3);
                }
            }
            for (USize i = sparseCount4; i < sparseIter.count(); i++) {
                const USize idx = sparseIter.index(i);
                const USize wIdx = weightOffset + idx * CHUNK8_32 * Arch::L2_SIZE;
                const VecInt32 input = SIMD::setInt32(inputs32[idx]);
                for (USize j = 0; j < Arch::L2_SIZE; j += CHUNK32) {
                    const VecInt8 weight = SIMD::loadInt8(&l1Weights[wIdx + CHUNK8_32 * j]);
                    auto& dotProds = matMul[j / CHUNK32];
                    dotProds[0] = SIMD::dotProdUInt8Int8(dotProds[0], input, weight);
                }
            }

            for (USize i = 0; i < Arch::L2_SIZE; i += CHUNK32) {
                const auto& dotProds = matMul[i / CHUNK32];
                const VecInt32 sum = SIMD::addInt32(SIMD::addInt32(dotProds[0], dotProds[1]), SIMD::addInt32(dotProds[2], dotProds[3]));
                const VecInt32 biases = SIMD::loadInt32(&l1Biases[biasOffset + i]);
                VecInt32 out = SIMD::addInt32(SIMD::shiftRightInt32(sum, -SHIFT1), biases);
                if constexpr (Arch::DUAL_ACTIVATION) {
                    VecInt32 out1 = SIMD::clampInt32(out, SIMD::zeroInt32(), SIMD::setInt32(QUANT * QUANT));
                    if constexpr (Arch::SKIP_L2) { out1 = SIMD::shiftLeftInt32(out1, QUANT_BITS); }
                    VecInt32 out2 = SIMD::mulLoInt32(out, out);
                    out2 = SIMD::clampInt32(out2, SIMD::zeroInt32(), SIMD::setInt32(QUANT * QUANT * QUANT * QUANT));
                    out2 = SIMD::shiftRightInt32(out2, SHIFT2);
                    SIMD::storeInt32(&outputs[i], out1);
                    SIMD::storeInt32(&outputs[Arch::L2_SIZE + i], out2);
                } else {
                    out = SIMD::clampInt32(out, SIMD::zeroInt32(), SIMD::setInt32(QUANT * QUANT));
                    out = SIMD::mulLoInt32(out, out);
                    out = SIMD::shiftRightInt32(out, SHIFT2);
                    SIMD::storeInt32(&outputs[i], out);
                }
            }
        }

        FORCE_INLINE void propagateL2(USize bucket, std::span<const Int32, L2_FULL_SIZE> inputs,
            std::span<Int32, Arch::L3_SIZE> outputs) const noexcept {
            const USize weightOffset = bucket * L2_FULL_SIZE * Arch::L3_SIZE;
            const USize biasOffset = bucket * Arch::L3_SIZE;

            std::memcpy(outputs.data(), &l2Biases[biasOffset], outputs.size_bytes());

            if constexpr (CHUNK32 * 4 > Arch::L3_SIZE) {
                for (USize i = 0; i < L2_FULL_SIZE; i++) {
                    Int32 input = inputs[i];
                    if constexpr (Arch::SKIP_L2) { input >>= QUANT_BITS; }
                    const VecInt32 in = SIMD::setInt32(input);
                    for (USize j = 0; j < Arch::L3_SIZE; j += CHUNK32 * 2) {
                        const VecInt32 weight_0 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 0]);
                        const VecInt32 weight_1 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 1]);
                        const VecInt32 prod_0 = SIMD::mulLoInt32(in, weight_0);
                        const VecInt32 prod_1 = SIMD::mulLoInt32(in, weight_1);
                        VecInt32 out_0 = SIMD::loadInt32(&outputs[j + CHUNK32 * 0]);
                        VecInt32 out_1 = SIMD::loadInt32(&outputs[j + CHUNK32 * 1]);
                        out_0 = SIMD::addInt32(out_0, prod_0);
                        out_1 = SIMD::addInt32(out_1, prod_1);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 0], out_0);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 1], out_1);
                    }
                }
            } else {
                for (USize i = 0; i < L2_FULL_SIZE; i++) {
                    Int32 input = inputs[i];
                    if constexpr (Arch::SKIP_L2) { input >>= QUANT_BITS; }
                    const VecInt32 in = SIMD::setInt32(input);
                    for (USize j = 0; j < Arch::L3_SIZE; j += CHUNK32 * 4) {
                        const VecInt32 weight_0 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 0]);
                        const VecInt32 weight_1 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 1]);
                        const VecInt32 weight_2 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 2]);
                        const VecInt32 weight_3 = SIMD::loadInt32(&l2Weights[weightOffset + i * Arch::L3_SIZE + j + CHUNK32 * 3]);
                        const VecInt32 prod_0 = SIMD::mulLoInt32(in, weight_0);
                        const VecInt32 prod_1 = SIMD::mulLoInt32(in, weight_1);
                        const VecInt32 prod_2 = SIMD::mulLoInt32(in, weight_2);
                        const VecInt32 prod_3 = SIMD::mulLoInt32(in, weight_3);
                        VecInt32 out_0 = SIMD::loadInt32(&outputs[j + CHUNK32 * 0]);
                        VecInt32 out_1 = SIMD::loadInt32(&outputs[j + CHUNK32 * 1]);
                        VecInt32 out_2 = SIMD::loadInt32(&outputs[j + CHUNK32 * 2]);
                        VecInt32 out_3 = SIMD::loadInt32(&outputs[j + CHUNK32 * 3]);
                        out_0 = SIMD::addInt32(out_0, prod_0);
                        out_1 = SIMD::addInt32(out_1, prod_1);
                        out_2 = SIMD::addInt32(out_2, prod_2);
                        out_3 = SIMD::addInt32(out_3, prod_3);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 0], out_0);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 1], out_1);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 2], out_2);
                        SIMD::storeInt32(&outputs[j + CHUNK32 * 3], out_3);
                    }
                }
            }
        }

        FORCE_INLINE void activateForwardL3(USize bucket, [[maybe_unused]] std::span<const Int32, L2_FULL_SIZE> skipped,
            std::span<const Int32, Arch::L3_SIZE> inputs, Int32& output) const noexcept {
            const USize weightOffset = bucket * Arch::L3_SIZE;
            const USize biasOffset = bucket;

            const VecInt32 zero = SIMD::zeroInt32();
            const VecInt32 quant = SIMD::setInt32(QUANT * QUANT * QUANT);

            VecInt32 sum;

            if constexpr (CHUNK32 * 4 > Arch::L3_SIZE) {
                VecInt32 out_0 = zero;
                VecInt32 out_1 = zero;
                for (USize i = 0; i < Arch::L3_SIZE; i += CHUNK32 * 2) {
                    VecInt32 in_0 = SIMD::loadInt32(&inputs[i + CHUNK32 * 0]);
                    VecInt32 in_1 = SIMD::loadInt32(&inputs[i + CHUNK32 * 1]);
                    in_0 = SIMD::clampInt32(in_0, zero, quant);
                    in_1 = SIMD::clampInt32(in_1, zero, quant);
                    if constexpr (Arch::SKIP_L2) {
                        const VecInt32 skip_0 = SIMD::loadInt32(&skipped[i + CHUNK32 * 0]);
                        const VecInt32 skip_1 = SIMD::loadInt32(&skipped[i + CHUNK32 * 1]);
                        in_0 = SIMD::addInt32(in_0, skip_0);
                        in_1 = SIMD::addInt32(in_1, skip_1);
                    }
                    const VecInt32 weight_0 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 0]);
                    const VecInt32 weight_1 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 1]);
                    in_0 = SIMD::mulLoInt32(in_0, weight_0);
                    in_1 = SIMD::mulLoInt32(in_1, weight_1);
                    out_0 = SIMD::addInt32(out_0, in_0);
                    out_1 = SIMD::addInt32(out_1, in_1);
                }

                sum = SIMD::addInt32(out_0, out_1);
            } else {
                VecInt32 out_0 = zero;
                VecInt32 out_1 = zero;
                VecInt32 out_2 = zero;
                VecInt32 out_3 = zero;
                for (USize i = 0; i < Arch::L3_SIZE; i += CHUNK32 * 4) {
                    VecInt32 in_0 = SIMD::loadInt32(&inputs[i + CHUNK32 * 0]);
                    VecInt32 in_1 = SIMD::loadInt32(&inputs[i + CHUNK32 * 1]);
                    VecInt32 in_2 = SIMD::loadInt32(&inputs[i + CHUNK32 * 2]);
                    VecInt32 in_3 = SIMD::loadInt32(&inputs[i + CHUNK32 * 3]);
                    in_0 = SIMD::clampInt32(in_0, zero, quant);
                    in_1 = SIMD::clampInt32(in_1, zero, quant);
                    in_2 = SIMD::clampInt32(in_2, zero, quant);
                    in_3 = SIMD::clampInt32(in_3, zero, quant);
                    if constexpr (Arch::SKIP_L2) {
                        const VecInt32 skip_0 = SIMD::loadInt32(&skipped[i + CHUNK32 * 0]);
                        const VecInt32 skip_1 = SIMD::loadInt32(&skipped[i + CHUNK32 * 1]);
                        const VecInt32 skip_2 = SIMD::loadInt32(&skipped[i + CHUNK32 * 2]);
                        const VecInt32 skip_3 = SIMD::loadInt32(&skipped[i + CHUNK32 * 3]);
                        in_0 = SIMD::addInt32(in_0, skip_0);
                        in_1 = SIMD::addInt32(in_1, skip_1);
                        in_2 = SIMD::addInt32(in_2, skip_2);
                        in_3 = SIMD::addInt32(in_3, skip_3);
                    }
                    const VecInt32 weight_0 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 0]);
                    const VecInt32 weight_1 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 1]);
                    const VecInt32 weight_2 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 2]);
                    const VecInt32 weight_3 = SIMD::loadInt32(&l3Weights[weightOffset + i + CHUNK32 * 3]);
                    in_0 = SIMD::mulLoInt32(in_0, weight_0);
                    in_1 = SIMD::mulLoInt32(in_1, weight_1);
                    in_2 = SIMD::mulLoInt32(in_2, weight_2);
                    in_3 = SIMD::mulLoInt32(in_3, weight_3);
                    out_0 = SIMD::addInt32(out_0, in_0);
                    out_1 = SIMD::addInt32(out_1, in_1);
                    out_2 = SIMD::addInt32(out_2, in_2);
                    out_3 = SIMD::addInt32(out_3, in_3);
                }

                const VecInt32 sum1 = SIMD::addInt32(out_0, out_1);
                const VecInt32 sum2 = SIMD::addInt32(out_2, out_3);
                sum = SIMD::addInt32(sum1, sum2);
            }

            output = SIMD::horizAddInt32(sum) + l3Biases[biasOffset];
        }

    public:
        FORCE_INLINE Int32 forward(USize bucket, std::span<const Int16, Arch::L1_SIZE> friendlyPSQInputs,
            std::span<const Int16, Arch::L1_SIZE> enemyPSQInputs, std::span<const Int16, Arch::L1_SIZE> friendlyThreatInputs,
            std::span<const Int16, Arch::L1_SIZE> enemyThreatInputs) const noexcept {
            SparseIterator sparseIter;

            alignas(SIMD::ALIGNMENT) std::array<UInt8, Arch::L1_SIZE> ftOut = {};
            alignas(SIMD::ALIGNMENT) std::array<Int32, L2_FULL_SIZE> l1Out = {};
            alignas(SIMD::ALIGNMENT) std::array<Int32, Arch::L3_SIZE> l2Out = {};
            Int32 l3Out = 0;

            activateFT(friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs, ftOut, sparseIter);
            forwardL1(bucket, ftOut, l1Out, sparseIter);
            propagateL2(bucket, l1Out, l2Out);
            activateForwardL3(bucket, l1Out, l2Out, l3Out);

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
    };
}
