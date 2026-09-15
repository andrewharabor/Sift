
#include <algorithm>
#include <array>
#include <fstream>
#include <memory>
#include <type_traits>

#include "../src/input.hpp"
#include "../src/nnue.hpp"
#include "../src/types.hpp"

using namespace Sift;

struct FTWeights { std::array<Int16, NNUE::InputFeatureSet::BUCKET_COUNT * NNUE::InputFeatureSet::PSQ_FEATURES * NNUE::L1_SIZE> psq; };

int main(void) { return 0; }
