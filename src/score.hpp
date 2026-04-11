#pragma once

#include "types.hpp"


namespace Clownfish {

namespace Score {

constexpr Int32 MAX_PLY = 251;

constexpr Int32 ZERO = 0;
constexpr Int32 MAX = 32767;
constexpr Int32 MIN = -MAX;
constexpr Int32 NONE = -33000;

constexpr Int32 MATE = 32700;
constexpr Int32 MATED = -MATE;
constexpr Int32 MATE_IN_MAX = MATE - MAX_PLY;
constexpr Int32 MATED_IN_MAX = -MATE_IN_MAX;

constexpr Int32 DRAW = 0;
constexpr Int32 WIN = 31000;
constexpr Int32 LOSS = -WIN;
constexpr Int32 KNOWN_WIN = 10000;
constexpr Int32 KNOWN_LOSS = -KNOWN_WIN;

constexpr bool valid(Int32 score) noexcept { return score >= MIN && score <= MAX; }

constexpr bool mate(Int32 score) noexcept { return score >= MATE_IN_MAX || score <= MATED_IN_MAX; }
constexpr Int32 mateIn(Int32 ply) noexcept { return MATE - ply; }
constexpr Int32 matedIn(Int32 ply) noexcept { return MATED + ply; }

}

namespace MoveScore {

constexpr Int32 HASH = 10000000;
constexpr Int32 GOOD_NOISY = 400000;
constexpr Int32 BAD_NOISY = GOOD_NOISY - 50001;
constexpr Int32 KILLER1 = 300001;
constexpr Int32 KILLER2 = 300000;
constexpr Int32 NONE = -8000000;

}

}
