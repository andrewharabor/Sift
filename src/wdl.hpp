#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "piece.hpp"
#include "position.hpp"
#include "score.hpp"
#include "types.hpp"


namespace Sift {

class WDL {
public:
    static constexpr std::pair<Int32, Int32> model(Int32 score, Int32 materialScore) noexcept {
        const auto [a, b] = params(materialScore);
        return {std::round(winProb(a, b, score) * 1000.0), std::round(winProb(a, b, -score) * 1000.0)};
    }

    static constexpr Int32 normalize(Int32 score, Int32 materialScore) noexcept {
        if (Score::draw(score) || Score::decisive(score)) {
            return score;
        }

        const auto [a, b] = params(materialScore);
        return static_cast<Int32>(std::round((static_cast<Float64>(score) / a) * 100.0));
    }

    static constexpr Int32 unnormalize(Int32 score, Int32 materialScore) noexcept {
        const auto [a, b] = params(materialScore);
        return static_cast<Int32>(std::round((static_cast<Float64>(score) * a) / 100.0));
    }

private:
    static constexpr std::array<Float64, 4> A = {-56.68614451, 147.07269963, -183.22284031, 240.87662059};
    static constexpr std::array<Float64, 4> B = {11.58191170, -9.61969055, 5.92615161, 28.93615867};

    static constexpr Int32 MATERIAL_MIN = 17;
    static constexpr Int32 MATERIAL_MAX = 78;
    static constexpr Float64 MATERIAL_SCALE = 58.0;

    static constexpr std::pair<Float64, Float64> params(Int32 materialScore) noexcept {
        const Float64 material = static_cast<Float64>(std::clamp(materialScore, MATERIAL_MIN, MATERIAL_MAX)) / MATERIAL_SCALE;
        const Float64 a = ((A[0] * material + A[1]) * material + A[2]) * material + A[3];
        const Float64 b = ((B[0] * material + B[1]) * material + B[2]) * material + B[3];
        return {a, b};
    }

    static constexpr Float64 winProb(Float64 a, Float64 b, Int32 score) noexcept { return 1.0 / (1.0 + std::exp(-(static_cast<Float64>(score) - a) / b)); }

};

}
