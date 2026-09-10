#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "simd.hpp"
#include "types.hpp"

#define NET_PARAM(Type, SIZE, name) \
    std::span<const Type, SIZE> name = std::span<const Type, SIZE>{static_cast<const Type *>(nullptr), SIZE}


namespace Sift {

class NetLoader {
public:
    explicit NetLoader(const std::byte *buffer, USize size) noexcept : buffer_(buffer), remaining_(size) {}

    template<typename Type, USize SIZE>
    bool load(std::span<const Type, SIZE> &dst) noexcept {
        const std::byte *ptr = get(dst.size_bytes());
        if (ptr == nullptr) {
            return false;
        }
        dst = std::span<const Type, SIZE>(reinterpret_cast<const Type *>(ptr), SIZE);
        return true;
    }

private:
    const std::byte *buffer_;
    USize remaining_;

    const std::byte *get(USize size) noexcept {
        if (size > remaining_) {
            return nullptr;
        }

        const std::byte *ptr = buffer_;
        buffer_ += size;
        remaining_ -= size;

        if ((reinterpret_cast<std::uintptr_t>(ptr) % SIMD::ALIGNMENT) != 0) {
            return nullptr;
        }

        return ptr;
    }
};

}
