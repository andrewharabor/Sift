#pragma once

#include <array>

#include "types.hpp"


namespace Syft {

namespace Arch {

static constexpr USize INPUT_SIZE = 768;
static constexpr USize LAYER_SIZE = 64;

static constexpr Int32 SCALE = 400;
static constexpr Int32 QA = 255;
static constexpr Int32 QB = 64;

};

using LayerArray = std::array<Int16, Arch::LAYER_SIZE>;
using LayerMultiArray = MultiArray<Int16, 2, Arch::LAYER_SIZE>;

}
