#pragma once

#include <array>

#include "types.hpp"


namespace Syft {

namespace Arch {

static constexpr USize INPUT_SIZE = 768;
static constexpr USize LAYER_SIZE = 64;
static constexpr USize OUTPUT_BUCKETS = 8;
static constexpr USize OUTPUT_BUCKET_DIV = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;

static constexpr Int32 SCALE = 153;
static constexpr Int32 QUANT_A = 255;
static constexpr Int32 QUANT_B = 64;

};

using LayerVector = std::array<Int16, Arch::LAYER_SIZE>;
using InputMatrix = std::array<LayerVector, Arch::INPUT_SIZE>;
using DualLayerVector = std::array<LayerVector, 2>;

}
