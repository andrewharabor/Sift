
#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <string_view>

#include "arch.hpp"
#include "types.hpp"
#include "utils.hpp"


using namespace Sift;

#if defined(USE_AVX512)
constexpr NetPerm TARGET_PERM = NetPerm::AVX512;
#elif defined(USE_AVX2)
constexpr NetPerm TARGET_PERM = NetPerm::AVX2;
#else
constexpr NetPerm TARGET_PERM = NetPerm::DEFAULT;
#endif

constexpr MultiArray<USize, 3, 8> ARRANGEMENTS = {{
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 2, 1, 3, 4, 6, 5, 7},
    {0, 4, 1, 5, 2, 6, 3, 7}
}};

int main(void) {
    std::unique_ptr<NetParams> originalParams = std::make_unique<NetParams>();
    std::unique_ptr<NetParams> permutedParams = std::make_unique<NetParams>();

    std::string_view path = TOSTRING(NETWORK_FILE);

    std::ifstream file = std::ifstream(path.data(), std::ios::binary);
    assert(file.is_open());

    file.seekg(0, std::ios::end);
    assert(64 * ((sizeof(NetParams) + 63) / 64) == static_cast<USize>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(originalParams.get()), sizeof(NetParams));
    file.close();

    *permutedParams = *originalParams;

    NetPerm perm = originalParams->perm;
    if (perm == TARGET_PERM) {
        return 0;
    }

    std::array<USize, 8> inversePermIdx;
    for (USize i = 0; i < 8; i++) {
        inversePermIdx[ARRANGEMENTS[static_cast<USize>(perm)][i]] = i;
    }

    std::array<USize, 8> permIdx;
    for (USize i = 0; i < 8; i++) {
        permIdx[i] = ARRANGEMENTS[static_cast<USize>(TARGET_PERM)][inversePermIdx[i]];
    }

    const auto permuteArray = [permIdx]<typename TYPE>(const TYPE * src, TYPE * dst) {
        for (USize i = 0; i < Arch::L1_SIZE; i += 64) {
            for (USize j = 0; j < 64; j++) {
                const USize srcChunk = j / 8;
                const USize dstChunk = permIdx[srcChunk];
                const USize offset = j % 8;
                dst[i + 8 * dstChunk + offset] = src[i + j];
            }
        }
    };

    for (USize b = 0; b < Arch::KING_BUCKETS; b++) {
        for (USize i = 0; i < Arch::PSQ_SIZE; i++) {
            permuteArray(originalParams->ftWeights[b][i].data(), permutedParams->ftWeights[b][i].data());
        }
    }
    permuteArray(originalParams->ftBiases.data(), permutedParams->ftBiases.data());

    permutedParams->perm = TARGET_PERM;

    std::ofstream outputFile = std::ofstream(path.data(), std::ios::binary);
    assert(outputFile.is_open());

    outputFile.write(reinterpret_cast<const char *>(permutedParams.get()), sizeof(NetParams));
    outputFile.close();

    return 0;
}
