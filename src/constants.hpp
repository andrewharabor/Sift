#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>


namespace Clownfish {

namespace Constants {
static constexpr const std::string_view FEN_STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr static std::size_t MAX_GAME_LENGTH = 1024;

constexpr static std::size_t MAX_MOVES = 256;

static constexpr std::size_t NNUE_INPUT_SIZE = 768;
static constexpr std::size_t NNUE_LAYER_SIZE = 1024;
static constexpr std::int32_t NNUE_SCALE = 400;
static constexpr std::int32_t NNUE_QA = 255;
static constexpr std::int32_t NNUE_QB = 64;

#if defined(USE_AVX2)
static constexpr std::size_t SIMD_LANES = 16;
#elif defined(USE_SSE) || defined(USE_NEON)
static constexpr std::size_t SIMD_LANES = 8;
#else
static constexpr std::size_t SIMD_LANES = 1;
#endif
};

}
