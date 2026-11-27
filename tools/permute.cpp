
#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <type_traits>

#include "../src/arch.hpp"
#include "../src/nnue.hpp"
#include "../src/types.hpp"

using namespace Sift;

constexpr USize L2_FULL_SIZE = Arch::L2_SIZE * (1 + Arch::DUAL_ACTIVATION);

struct Weights {
    std::array<Int16, Arch::BUCKET_COUNT * Arch::PSQ_FEATURES * Arch::L1_SIZE> ftPSQWeights;
    std::array<Int8, Arch::THREAT_FEATURES * Arch::L1_SIZE> ftThreatWeights;
    std::array<Int16, Arch::L1_SIZE> ftBiases;
    std::array<Int8, NNUE::Output::BUCKET_COUNT * Arch::L1_SIZE * Arch::L2_SIZE> l1Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * Arch::L2_SIZE> l1Biases;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * L2_FULL_SIZE * Arch::L3_SIZE> l2Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * Arch::L3_SIZE> l2Biases;
    std::array<Int32, NNUE::Output::BUCKET_COUNT * Arch::L3_SIZE> l3Weights;
    std::array<Int32, NNUE::Output::BUCKET_COUNT> l3Biases;
};

int main(int argc, char* argv[]) {
    if (argc >= 3) {
        std::ifstream in = std::ifstream(argv[1], std::ios::binary);
        assert(in.is_open());

        std::ofstream out = std::ofstream(argv[2], std::ios::binary);
        assert(out.is_open());

        if constexpr (!SIMD::PACK_REORDER) {
            std::copy(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}, std::ostreambuf_iterator<char>{out});
            return 0;
        }

        std::unique_ptr<Weights> weights = std::make_unique<Weights>();
        in.read(reinterpret_cast<char*>(weights.get()), sizeof(Weights));

        NNUE::Arch::permuteParam<Int16>(weights->ftPSQWeights);
        NNUE::Arch::permuteParam<Int16>(weights->ftBiases);
        NNUE::Arch::permuteParam<Int8>(weights->ftThreatWeights);

        out.write(reinterpret_cast<const char*>(weights.get()), sizeof(Weights));

        in.close();
        out.close();
    }

    return 0;
}
