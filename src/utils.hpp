#pragma once

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#include <mmintrin.h>
#endif

#include "types.hpp"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


namespace Sift {

class Utils {
public:
    static std::vector<std::string_view> splitStringView(std::string_view string, char delimiter = ' ') {
        std::vector<std::string_view> result;
        std::size_t start = 0;
        while (start < string.size()) {
            std::size_t end = string.find(delimiter, start);
            if (end == std::string_view::npos) {
                end = string.size();
            }
            result.push_back(string.substr(start, end - start));
            start = end + 1;
        }
        return result;
    }

    static void stringToLower(std::string &string) { std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::tolower(c); }); }

    static constexpr UInt64 murmurHash3(UInt64 value) noexcept {
        value ^= value >> 33;
        value *= 0xFF51AFD7ED558CCD;
        value ^= value >> 33;
        value *= 0xC4CEB9FE1A85EC53;
        value ^= value >> 33;
        return value;
    };

    template<std::integral auto K>
    static constexpr auto linInterp(auto a, auto b, auto t) { return (a * (K - t) + b * t) / K; }

#if defined(__GNUC__) || defined(__clang__)

    static void prefetchPtr(const void *ptr) { __builtin_prefetch(ptr); }

    static constexpr UInt64 mulHi64(UInt64 a, UInt64 b) { return __uint128_t(a) * __uint128_t(b) >> 64; }

#elif defined(_MSC_VER) && !defined(__clang__)

    static void prefetchPtr(const void *ptr) { _mm_prefetch(static_cast<const char *>(ptr), _MM_HINT_T0); }

    static constexpr U64 mulHi64(U64 a, U64 b) { return __umulh(a, b); }

#else

    static void prefetchPtr(const void *ptr) {}

    static constexpr U64 mulHi64(U64 a, U64 b) {
        U64 aLo = a & 0xFFFFFFFF;
        U64 aHi = a >> 32;
        U64 bLo = b & 0xFFFFFFFF;
        U64 bHi = b >> 32;
        U64 c1 = (aLo * bLo) >> 32;
        U64 c2 = aHi * bLo + c1;
        U64 c3 = aLo * bHi + (c2 & 0xFFFFFFFF);
        return aHi * bHi + (c2 >> 32) + (c3 >> 32);
    }

#endif

#if defined(_WIN32)

    static void *alignedAlloc(USize size, USize alignment) { return _aligned_malloc(size, alignment); }

    static void alignedFree(void *ptr) {
        if (ptr == nullptr) {
            return;
        }
        _aligned_free(ptr);
    }

#else

    static void *alignedAlloc(USize size, USize alignment) { return std::aligned_alloc(alignment, size); }

    static void alignedFree(void *ptr) {
        if (ptr == nullptr) {
            return;
        }
        std::free(ptr);
    }

#endif

};

}
