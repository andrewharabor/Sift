#pragma once

#include "move.hpp"
#include "move-gen.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Clownfish {
class Benchmark {
public:
    template<MoveGenType MGT = MoveGenType::ALL>
    static U64 perft(Position &position, int depth) {
        MoveList moveList;

        if (depth == 1) {
            MoveGen::legal<MGT>(position, moveList);
            return static_cast<U64>(moveList.size());
        }

        U64 nodes = 0;
        MoveGen::legal(position, moveList);
        for (const Move move : moveList) {
            position.make(move);
            nodes += perft<MGT>(position, depth - 1);
            position.unmake(move);
        }

        return nodes;
    }
};

}
