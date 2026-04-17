#pragma once

#include "move.hpp"
#include "move-gen.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Clownfish {

class Perft {
public:
    template<MoveGenType MOVE_GEN_TYPE = MoveGenType::ALL>
    static UInt64 run(Position &position, UInt32 depth) {
        MoveList moveList;

        if (depth == 1) {
            MoveGen::legal<MOVE_GEN_TYPE>(position, moveList);
            return static_cast<UInt64>(moveList.size());
        }

        UInt64 nodes = 0;
        MoveGen::legal<MOVE_GEN_TYPE>(position, moveList);
        for (const Move move : moveList) {
            position.make(move);
            nodes += run<MOVE_GEN_TYPE>(position, depth - 1);
            position.unmake(move);
        }

        return nodes;
    }
};

}
