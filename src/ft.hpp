#pragma once

#include "accumulator.hpp"
#include "arch.hpp"
#include "reader.hpp"
#include "types.hpp"

namespace Sift {
    class FT {
    public:
        static constexpr USize PSQ_INPUT_SIZE = Arch::BUCKET_COUNT * Arch::PSQ_FEATURES;
        static constexpr USize OUTPUT_SIZE = Arch::L1_SIZE;
        static constexpr USize PSQ_WEIGHT_SIZE = PSQ_INPUT_SIZE * OUTPUT_SIZE;
        static constexpr USize THREAT_WEIGHT_SIZE = Arch::THREAT_FEATURES * OUTPUT_SIZE;
        static constexpr USize BIAS_SIZE = OUTPUT_SIZE;

        static_assert(PSQ_INPUT_SIZE > 0);
        static_assert(OUTPUT_SIZE > 0);

        NET_PARAM(Int16, PSQ_WEIGHT_SIZE, psqWeights);
        NET_PARAM(Int8, THREAT_WEIGHT_SIZE, threatWeights);
        NET_PARAM(Int16, BIAS_SIZE, biases);

        constexpr const Int16* psqWeightPtr(USize featureIdx) const noexcept { return &psqWeights[featureIdx * OUTPUT_SIZE]; }

        constexpr const Int8* threatWeightPtr(USize featureIdx) const noexcept { return &threatWeights[featureIdx * OUTPUT_SIZE]; }

        inline bool load(ByteReader& reader) noexcept {
            return reader.load(psqWeights) && reader.load(threatWeights) && reader.load(biases);
        }

        static constexpr USize byteSize() noexcept {
            return sizeof(Int16) * PSQ_WEIGHT_SIZE + sizeof(Int8) * THREAT_WEIGHT_SIZE + sizeof(Int16) * BIAS_SIZE;
        }
    };
}
