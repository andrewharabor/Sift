
#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <type_traits>

#include "../src/input.hpp"
#include "../src/nnue.hpp"
#include "../src/types.hpp"

using namespace Sift;

constexpr USize L2_FULL_SIZE = NNUE::L2_SIZE * (1 + NNUE::DUAL_ACTIVATION);

struct Weights {
    std::array<Int16, NNUE::InputFeatureSet::BUCKET_COUNT * NNUE::InputFeatureSet::PSQ_FEATURES * NNUE::L1_SIZE> ftPSQWeights;
    std::array<Int8, NNUE::InputFeatureSet::THREAT_FEATURES * NNUE::L1_SIZE> ftThreatWeights;
    std::array<Int16, NNUE::L1_SIZE> ftBiases;
    std::array<Int8, NNUE::Output::BUCKET_COUNT * NNUE::L1_SIZE * NNUE::L2_SIZE> l1Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * NNUE::L2_SIZE> l1Biases;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * L2_FULL_SIZE * NNUE::L3_SIZE> l2Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * NNUE::L3_SIZE> l2Biases;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * NNUE::L3_SIZE> l3Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT> l3Biases;
};

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        if constexpr (!NNUE::Arch::NEEDS_FT_PERMUTE) { return 0; }

        std::unique_ptr<Weights> weights = std::make_unique<Weights>();

        std::ifstream in = std::ifstream(argv[1], std::ios::binary);
        assert(in.is_open());

        in.read(reinterpret_cast<char*>(weights.get()), sizeof(Weights));

        NNUE::Arch::permuteParam<Int16>(weights->ftPSQWeights);
        NNUE::Arch::permuteParam<Int16>(weights->ftBiases);

        if constexpr (NNUE::InputFeatureSet::THREAT_INPUTS) { NNUE::Arch::permuteParam<Int8>(weights->ftThreatWeights); }

        std::ofstream out = std::ofstream(argv[1], std::ios::binary);
        assert(out.is_open());

        out.write(reinterpret_cast<const char*>(weights.get()), sizeof(Weights));

        in.close();
        out.close();
    }

    return 0;
}
