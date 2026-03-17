#pragma once

#include <cstdint>

#include "move.hpp"
#include "move-generator.hpp"
#include "position.hpp"


namespace Clownfish {
class Benchmark {
public:
    template<MoveGenerationType MGT = MoveGenerationType::ALL>
    static std::uint64_t perft(Position &position, int depth) {
        MoveList moveList;

        if (depth == 1) {
            MoveGenerator::legal<MGT>(position, moveList);
            return static_cast<std::uint64_t>(moveList.size());
        }

        std::uint64_t nodes = 0;
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
