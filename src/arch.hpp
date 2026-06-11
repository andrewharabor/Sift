#pragma once

#include <array>

#include "types.hpp"


namespace Syft {

namespace Arch {

static constexpr USize INPUT_SIZE = 704;
static constexpr USize LAYER_SIZE = 1024;

static constexpr USize KING_BUCKETS = 16;
static constexpr std::array<USize, 32> KING_BUCKET_LAYOUT = {
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  8,  9,  9,
    10, 10, 11, 11,
    12, 12, 13, 13,
    12, 12, 13, 13,
    14, 14, 15, 15,
    14, 14, 15, 15,
};

static constexpr USize OUTPUT_BUCKETS = 8;
static constexpr USize OUTPUT_BUCKET_DIV = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;

static constexpr Int32 SCALE = 139;
static constexpr Int32 QUANT_A = 255;
static constexpr Int32 QUANT_B = 64;

};

using LayerVector = std::array<Int16, Arch::LAYER_SIZE>;
using FeatureMatrix = std::array<LayerVector, Arch::INPUT_SIZE>;
using DualLayerVector = std::array<LayerVector, 2>;

}
