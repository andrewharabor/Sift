#pragma once

#include "types.hpp"


namespace Clownfish {

class Score {
public:
    static constexpr I32 MAX_PLY = 128;

    static constexpr I32 ZERO = 0;
    static constexpr I32 MAX = 32001;
    static constexpr I32 MIN = -MAX;
    static constexpr I32 NONE = 32002;

    static constexpr I32 MATE = 32000;
    static constexpr I32 MATED = -MATE;
    static constexpr I32 MATE_IN_MAX = MATE - MAX_PLY;
    static constexpr I32 MATED_IN_MAX = -MATE_IN_MAX;

    static constexpr I32 DRAW = 0;
    static constexpr I32 WIN = 31000;
    static constexpr I32 LOSS = -WIN;

    static constexpr bool valid(I32 score) noexcept { return score >= MIN && score <= MAX; }

    static constexpr bool win(I32 score) noexcept { return score >= WIN; }
    static constexpr bool loss(I32 score) noexcept { return score <= LOSS; }
    static constexpr bool draw(I32 score) noexcept { return score == DRAW; }
    static constexpr bool mate(I32 score) noexcept { return score >= MATE_IN_MAX || score <= MATED_IN_MAX; }

    static constexpr I32 mateIn(I32 ply) noexcept { return MATE - ply; }
    static constexpr I32 matedIn(I32 ply) noexcept { return MATED + ply; }
};

}
