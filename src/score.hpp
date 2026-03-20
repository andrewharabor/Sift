#pragma once

#include "types.hpp"


namespace Clownfish {

using Score = I32;

class ScoreLimits {
public:
    static constexpr I32 MAX_PLY = 128;

    static constexpr Score ZERO = 0;
    static constexpr Score MAX = 32001;
    static constexpr Score MIN = -MAX;
    static constexpr Score NONE = 32002;

    static constexpr Score MATE = 32000;
    static constexpr Score MATED = -MATE;
    static constexpr Score MATE_IN_MAX = MATE - MAX_PLY;
    static constexpr Score MATED_IN_MAX = -MATE_IN_MAX;

    static constexpr Score DRAW = 0;
    static constexpr Score WIN = 31000;
    static constexpr Score LOSS = -WIN;

    constexpr bool valid(Score score) const noexcept { return score >= MIN && score <= MAX; }

    constexpr bool win(Score score) const noexcept { return score >= WIN; }
    constexpr bool loss(Score score) const noexcept { return score <= LOSS; }
    constexpr bool draw(Score score) const noexcept { return score == DRAW; }
    constexpr bool mate(Score score) const noexcept { return score >= MATE_IN_MAX || score <= MATED_IN_MAX; }
};

}
