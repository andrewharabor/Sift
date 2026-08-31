#pragma once

#include "types.hpp"


namespace Sift {

namespace Score {
static constexpr Int32 MAX_PLY = 251;

static constexpr Int32 MAX = 32700;
static constexpr Int32 MIN = -MAX;
static constexpr Int32 NONE = -32767;

static constexpr Int32 MATE = 32000;
static constexpr Int32 MATED = -MATE;
static constexpr Int32 MATE_IN_MAX = MATE - MAX_PLY;
static constexpr Int32 MATED_IN_MAX = -MATE_IN_MAX;

static constexpr Int32 WIN = 25000;
static constexpr Int32 LOSS = -WIN;

static constexpr Int32 DRAW_MAX = 2;
static constexpr Int32 DRAW_MIN = -DRAW_MAX;

static constexpr bool valid(Int32 score) noexcept { return score >= MIN && score <= MAX; }

static constexpr bool mate(Int32 score) noexcept { return score >= MATE_IN_MAX || score <= MATED_IN_MAX; }
static constexpr Int32 mateIn(Int32 ply) noexcept { return MATE - ply; }
static constexpr Int32 matedIn(Int32 ply) noexcept { return MATED + ply; }

static constexpr bool win(Int32 score) noexcept { return score >= WIN; }
static constexpr bool loss(Int32 score) noexcept { return score <= LOSS; }
static constexpr bool decisive(Int32 score) noexcept { return score >= WIN || score <= LOSS; }

static constexpr bool draw(Int32 score) noexcept { return score >= DRAW_MIN && score <= DRAW_MAX; }
static constexpr Int32 drawScore(UInt64 seed) noexcept { return 2 - static_cast<Int32>(seed % 4); }

}

}
