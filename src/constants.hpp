#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>


namespace Clownfish {

namespace Constants {
static constexpr const std::string_view FEN_STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr static std::size_t MAX_POSITION_DEPTH = 1024;
constexpr static std::size_t MAX_MOVES = 256;
};

}
