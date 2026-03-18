#pragma once

#include "move.hpp"
#include "move-generator.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Clownfish {
class Benchmark {
public:
    template<MoveGenerationType MGT = MoveGenerationType::ALL>
    static U64 perft(Position &position, int depth) {
        MoveList moveList;

        if (depth == 1) {
            MoveGenerator::legal<MGT>(position, moveList);
            return static_cast<U64>(moveList.size());
        }

        U64 nodes = 0;
        MoveGenerator::legal(position, moveList);
        for (const Move move : moveList) {
            position.make(move);
            nodes += perft<MGT>(position, depth - 1);
            position.unmake(move);
        }

        return nodes;
    }
};

}
