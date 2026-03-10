#pragma once

#include <cstdint>

#include "move.hpp"
#include "move-generator.hpp"
#include "position.hpp"


namespace Clownfish {
class Perft {
public:
    static std::uint64_t perft(Position &position, int depth) {
        MoveList moveList;
        MoveGenerator::legal(position, moveList);

        if (depth == 1) {
            return static_cast<std::uint64_t>(moveList.size());
        }

        std::uint64_t nodes = 0;
        for (const Move move : moveList) {
            position.make(move);
            nodes += perft(position, depth - 1);
            position.unmake(move);
        }

        return nodes;
    }
};

}
