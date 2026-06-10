#pragma once

#include <array>

#include "types.hpp"


namespace Syft {

namespace Arch {

static constexpr USize INPUT_SIZE = 768;
static constexpr USize LAYER_SIZE = 64;

static constexpr USize KING_BUCKETS = 4;
static constexpr std::array<USize, 32> KING_BUCKET_LAYOUT = {
    0, 0, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
    3, 3, 3, 3,
};

static constexpr USize OUTPUT_BUCKETS = 8;
static constexpr USize OUTPUT_BUCKET_DIV = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;

static constexpr Int32 SCALE = 148;
static constexpr Int32 QUANT_A = 255;
static constexpr Int32 QUANT_B = 64;

};

using LayerVector = std::array<Int16, Arch::LAYER_SIZE>;
using FeatureMatrix = std::array<LayerVector, Arch::INPUT_SIZE>;
using DualLayerVector = std::array<LayerVector, 2>;

}
