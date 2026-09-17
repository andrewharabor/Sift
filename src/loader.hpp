#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>

#include "nnue.hpp"
#include "numa.hpp"
#include "reader.hpp"
#include "types.hpp"
#include "utils.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_SILENCE_BITCODE_WARNING

#include "incbin/incbin.h"

INCBIN(std::byte, EMBEDDED_NETWORK, TOSTRING(NETWORK_FILE));

namespace Sift {
    namespace NetLoader {
        using Network = NNUE::Network;

        std::byte* loadedData = nullptr;

#if defined(USE_NUMA)
        std::unique_ptr<NUMAUniqueAllocation<std::byte>> networkData = nullptr;
        std::unique_ptr<NUMAUniqueAllocation<Network>> networks = nullptr;
#else
        Network network;
#endif

        bool loaded = false;

        void init() noexcept {
            const USize networkSize = Network::byteSize();

            assert(EMBEDDED_NETWORK_size >= networkSize);

            loaded = false;
            if (loadedData != nullptr) {
                Utils::alignedFree(loadedData);
                loadedData = nullptr;
            }

            const std::byte* ptr = EMBEDDED_NETWORK_data;

#if defined(USE_NUMA)
            networkData = std::make_unique<NUMAUniqueAllocation<std::byte>>(networkSize);
            networks = std::make_unique<NUMAUniqueAllocation<Network>>();

            for (USize node = 0; node < NUMA::nodeCount(); node++) {
                std::byte* target = networkData->getForNode(node);
                std::memcpy(target, ptr, networkSize);
                ByteReader reader = ByteReader(target, networkSize);
                if (!networks->getForNode(node)->load(reader)) {
                    assert(false);
                    return;
                }
            }

            if (loadedData != nullptr) {
                Utils::alignedFree(loadedData);
                loadedData = nullptr;
            }
#else
            ByteReader reader = ByteReader(ptr, networkSize);
            if (!network.load(reader)) {
                assert(false);
                return;
            }
#endif

            loaded = true;
        }

        void cleanup() noexcept {
            if (loadedData != nullptr) {
                Utils::alignedFree(loadedData);
                loadedData = nullptr;
            }

#if defined(USE_NUMA)
            networkData = nullptr;
            networks = nullptr;
#endif

            loaded = false;
        }

        const Network* get([[maybe_unused]] USize threadID) noexcept {
#if defined(USE_NUMA)
            return networks->get(threadID);
#else
            return &network;
#endif
        }
    }
}
