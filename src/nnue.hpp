#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <vector>

#include "arch.hpp"
#include "features.hpp"
#include "loader.hpp"
#include "network.hpp"
#include "numa.hpp"
#include "simd.hpp"
#include "types.hpp"
#include "utils.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_SILENCE_BITCODE_WARNING

#include "incbin/incbin.h"


INCBIN(std::byte, EMBEDDED_NETWORK, TOSTRING(NETWORK_FILE));

namespace Sift {

class NNUE {
public:
    static constexpr USize L1_SIZE = 1024;
    static constexpr USize L2_SIZE = 32;
    static constexpr USize L3_SIZE = 32;

    static constexpr Int32 FT_SCALE_BITS = 7;
    static constexpr Int32 FT_QUANT_BITS = 8;
    static constexpr Int32 L1_QUANT_BITS = 3;

    static constexpr Int32 SCALE = 253;

    static constexpr bool DUAL_ACTIVATION = false;
    static constexpr bool SKIP_L2 = false;

    using PSQFeatureSet = MergedMirroredKingBucketInputs<
        MirroredKingSide::ABCD,
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 8, 9, 9,
        10, 10, 11, 11,
        12, 12, 13, 13,
        12, 12, 13, 13,
        14, 14, 15, 15,
        14, 14, 15, 15
    >;

    using InputFeatureSet = ThreatInputs<PSQFeatureSet>;

    using Updates = InputFeatureSet::Updates;
    using BoardObserver = BoardObserver<InputFeatureSet>;

    using FeatureTransformer = FeatureTransformer<L1_SIZE, InputFeatureSet>;
    using Accumulator = FeatureTransformer::Accumulator;
    using RefreshTable = FeatureTransformer::RefreshTable;

    using UpdatableAccumulator = UpdatableAccumulator<InputFeatureSet, FeatureTransformer>;

    using Output = MaterialCountBucketOutput<8>;

    using Arch = PairwiseMultilayerArch<
        InputFeatureSet,
        L1_SIZE,
        L2_SIZE,
        L3_SIZE,
        FT_SCALE_BITS,
        FT_QUANT_BITS,
        L1_QUANT_BITS,
        DUAL_ACTIVATION,
        SKIP_L2,
        Output,
        SCALE
    >;

    using Network = PerspectiveNetwork<FeatureTransformer, Output, Arch>;

private:
    std::vector<UpdatableAccumulator> accStack_;
    UpdatableAccumulator *curr_;

    RefreshTable refreshTable_;

    const Network *network_;
};

using Network = NNUE::Network;

namespace NetLoader {

std::byte *loadedData = nullptr;

#if defined(USE_NUMA)
std::unique_ptr<NUMAUniqueAllocation<std::byte>> networkData = nullptr;
std::unique_ptr<NUMAUniqueAllocation<Network>> networks = nullptr;
#else
Network network;
#endif

bool loaded = false;

void init() noexcept {
    const USize networkSize = Network::byteSize();

    assert(EMBEDDED_NETWORK_size >= networkSize);

    loaded = false;
    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }

    const std::byte *ptr = EMBEDDED_NETWORK_data;

#if defined(USE_NUMA)
    networkData = std::make_unique<NUMAUniqueAllocation<std::byte>>(networkSize);
    networks = std::make_unique<NUMAUniqueAllocation<Network>>();

    for (USize node = 0; node < NUMA::nodeCount(); node++) {
        std::byte *target = networkData->getForNode(node);
        std::memcpy(target, ptr, networkSize);
        ByteLoader byteLoader = ByteLoader(target, networkSize);
        if (!networks->getForNode(node)->load(byteLoader)) {
            assert(false);
            return;
        }
    }

    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }
#else
    ByteLoader byteLoader = ByteLoader(ptr, networkSize);
    if (!network.load(byteLoader)) {
        assert(false);
        return;
    }
#endif

    loaded = true;
}

void cleanup() noexcept {
    if (loadedData != nullptr) {
        Utils::alignedFree(loadedData);
        loadedData = nullptr;
    }

#if defined(USE_NUMA)
    networkData = nullptr;
    networks = nullptr;
#endif

    loaded = false;
}

const Network *get([[maybe_unused]] USize threadID) noexcept {
#if defined(USE_NUMA)
    return networks->get(threadID);
#else
    return &network;
#endif
}

}

};
