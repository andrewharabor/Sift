#pragma once

#include "types.hpp"


namespace Syft {

namespace NNUEArch {

static constexpr USize INPUT_SIZE = 768;
static constexpr USize LAYER_SIZE = 1024;

static constexpr Int32 SCALE = 400;
static constexpr Int32 QA = 255;
static constexpr Int32 QB = 64;

};

using VecInt16 = std::array<Int16, NNUEArch::LAYER_SIZE>;

}
