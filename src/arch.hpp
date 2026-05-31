#pragma once

#include <array>

#include "types.hpp"


namespace Syft {

namespace Arch {

static constexpr USize INPUT_SIZE = 768;
static constexpr USize LAYER_SIZE = 64;

static constexpr Int32 SCALE = 400;
static constexpr Int32 QUANT_A = 255;
static constexpr Int32 QUANT_B = 64;

};

using InputMatrix = MultiArray<Int16, Arch::INPUT_SIZE, Arch::LAYER_SIZE>;
using LayerVector = std::array<Int16, Arch::LAYER_SIZE>;
using DualLayerVector = std::array<LayerVector, 2>;

}
