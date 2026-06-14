
#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <string_view>

#include "arch.hpp"
#include "types.hpp"
#include "utils.hpp"


using namespace Syft;

#if defined(USE_AVX512)
constexpr WeightsPerm TARGET_PERM = WeightsPerm::AVX512;
#elif defined(USE_AVX2)
constexpr WeightsPerm TARGET_PERM = WeightsPerm::AVX2;
#else
constexpr WeightsPerm TARGET_PERM = WeightsPerm::DEFAULT;
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

    WeightsPerm perm = originalParams->perm;
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

    for (USize b = 0; b < Arch::KING_BUCKETS; b++) {
        for (USize i = 0; i < Arch::INPUT_SIZE; i++) {
            for (USize j = 0; j < Arch::L1_SIZE; j += 64) {
                const Int16 *src = &originalParams->ftWeights[b][i][j];
                Int16 *dst = &permutedParams->ftWeights[b][i][j];

                for (USize jj = 0; jj < 64; jj++) {
                    const USize srcChunk = jj / 8;
                    const USize dstChunk = permIdx[srcChunk];
                    const USize offset = jj % 8;
                    dst[dstChunk * 8 + offset] = src[jj];
                }
            }
        }
    }

    for (USize j = 0; j < Arch::L1_SIZE; j += 64) {
        const Int16 *src = &originalParams->ftBiases[j];
        Int16 *dst = &permutedParams->ftBiases[j];

        for (USize jj = 0; jj < 64; jj++) {
            const USize srcChunk = jj / 8;
            const USize dstChunk = permIdx[srcChunk];
            const USize offset = jj % 8;
            dst[dstChunk * 8 + offset] = src[jj];
        }
    }

    permutedParams->perm = TARGET_PERM;

    std::ofstream outputFile = std::ofstream(path.data(), std::ios::binary);
    assert(outputFile.is_open());

    outputFile.write(reinterpret_cast<const char *>(permutedParams.get()), sizeof(NetParams));
    outputFile.close();

    return 0;
}
