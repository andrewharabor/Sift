#pragma once

#include "types.hpp"


namespace Syft {

class Score {
public:
    static constexpr Int32 MAX_PLY = 251;

    static constexpr Int32 ZERO = 0;
    static constexpr Int32 MAX = 32767;
    static constexpr Int32 MIN = -MAX;
    static constexpr Int32 NONE = -33000;

    static constexpr Int32 MATE = 32700;
    static constexpr Int32 MATED = -MATE;
    static constexpr Int32 MATE_IN_MAX = MATE - MAX_PLY;
    static constexpr Int32 MATED_IN_MAX = -MATE_IN_MAX;

    static constexpr Int32 DRAW = 0;
    static constexpr Int32 DRAW_MAX = 2;
    static constexpr Int32 DRAW_MIN = -DRAW_MAX;
    static constexpr Int32 WIN = 31000;
    static constexpr Int32 LOSS = -WIN;
    static constexpr Int32 KNOWN_WIN = 10000;
    static constexpr Int32 KNOWN_LOSS = -KNOWN_WIN;

    static constexpr bool valid(Int32 score) noexcept { return score >= MIN && score <= MAX; }

    static constexpr bool mate(Int32 score) noexcept { return score >= MATE_IN_MAX || score <= MATED_IN_MAX; }
    static constexpr Int32 mateIn(Int32 ply) noexcept { return MATE - ply; }
    static constexpr Int32 matedIn(Int32 ply) noexcept { return MATED + ply; }

    static constexpr Int32 draw(UInt64 seed) noexcept { return 2 - static_cast<Int32>(seed % 4); }
};

class MoveScore {
public:
    static constexpr Int32 HASH = 10000000;

    static constexpr Int32 GOOD_NOISY = 400000;
    static constexpr Int32 BAD_NOISY = GOOD_NOISY - 50001;

    static constexpr Int32 PROMOTION_BONUS = 10000;
    static constexpr Int32 QSEARCH_PROMOTION_BONUS = 1000000;

    static constexpr Int32 KILLER1 = 300001;
    static constexpr Int32 KILLER2 = 300000;

    static constexpr Int32 NONE = -8000000;
};

}
