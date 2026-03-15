#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>


namespace Clownfish {

class Utils {
public:
    static std::vector<std::uint8_t> readBytes(std::string_view path) {
        std::vector<std::uint8_t> bytes;

        if (path.empty()) {
            return bytes;
        }

        std::ifstream file(std::string(path), std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return bytes;
        }

        const std::streamoff sizeSigned = file.tellg();
        if (sizeSigned < 0) {
            return bytes;
        }

        const std::size_t size = static_cast<std::size_t>(sizeSigned);
        bytes.resize(size);

        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))) {
            return {};
        }

        return bytes;
    };

    static constexpr std::int16_t readInt16LE(const std::vector<std::uint8_t> &bytes, std::size_t offset) noexcept {
        const std::uint16_t low = static_cast<std::uint16_t>(bytes[offset]);
        const std::uint16_t high = static_cast<std::uint16_t>(bytes[offset + 1]);
        const std::uint16_t value = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8));
        return static_cast<std::int16_t>(value);
    }
};

}
