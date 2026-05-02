#pragma once

#include <string_view>
#include <vector>

#include "types.hpp"


namespace Syft {

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

    static constexpr UInt64 murmurHash3(UInt64 value) noexcept {
        value ^= value >> 33;
        value *= 0xFF51AFD7ED558CCD;
        value ^= value >> 33;
        value *= 0xC4CEB9FE1A85EC53;
        value ^= value >> 33;
        return value;
    };
};

}
