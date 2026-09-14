#pragma once

#include <span>

#include "position.hpp"
#include "reader.hpp"
#include "types.hpp"


namespace Sift {

template<typename FeatureTransformer, typename Output, typename Arch>
class PerspectiveNetwork {
private:
    using InputType = std::span<const Int16, FeatureTransformer::OUTPUT_SIZE>;

public:
    constexpr const FeatureTransformer &ft() const noexcept { return ft_; }

    inline Int32 forward(const Position &position, InputType friendlyPSQInputs, InputType enemyPSQInputs, InputType friendlyThreatInputs, InputType enemyThreatInputs) const noexcept { return arch_.forward(Output::bucket(position), friendlyPSQInputs, enemyPSQInputs, friendlyThreatInputs, enemyThreatInputs); }

    inline bool load(ByteReader &reader) noexcept {
        if (!ft_.load(reader) || !arch_.load(reader)) {
            return false;
        }

        if (Arch::NEEDS_FT_PERMUTE) {
            Arch::permuteFTParams(ft_.psqWeights, ft_.threatWeights, ft_.biases);
        }
        return true;
    }

    static constexpr USize byteSize() noexcept { return FeatureTransformer::byteSize() + Arch::byteSize(); }

private:
    FeatureTransformer ft_;
    Arch arch_;
};

}
