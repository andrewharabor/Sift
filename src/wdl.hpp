#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Syft {

class WDL {
public:
    static constexpr std::pair<Float64, Float64> winLoss(Int32 materialScore, Int32 score) noexcept {
        const Float64 material = static_cast<Float64>(std::clamp(materialScore, MATERIAL_MIN, MATERIAL_MAX)) / MATERIAL_SCALE;
        const Float64 a = ((A[0] * material + A[1]) * material + A[2]) * material + A[3];
        const Float64 b = ((B[0] * material + B[1]) * material + B[2]) * material + B[3];
        return {winRate(a, b, score), winRate(a, b, -score)};
    }

private:
    static constexpr std::array<Float64, 4> A = {-420.12493420, 1155.97899531, -1074.82879152, 441.44515956};
    static constexpr std::array<Float64, 4> B = {-106.59407292, 300.77010455, -267.28444080, 114.30281819};

    static constexpr Int32 MATERIAL_MIN = 17;
    static constexpr Int32 MATERIAL_MAX = 78;
    static constexpr Float64 MATERIAL_SCALE = 58.0;

    static constexpr Float64 winRate(Float64 a, Float64 b, Int32 score) noexcept {
        return 1.0 / (1.0 + std::exp(-(static_cast<Float64>(score) - a) / b));
    }
};

}
