#pragma once

#include <string_view>

#include "types.hpp"


namespace Clownfish {

namespace Constants {
static constexpr const std::string_view FEN_STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr static USize MAX_POSITION_DEPTH = 1024;
constexpr static USize MAX_MOVES = 256;
};

}
