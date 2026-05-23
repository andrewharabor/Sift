#pragma once

#include "move.hpp"
#include "move-gen.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Syft {

struct PerftTest {
    std::string fen;
    UInt32 depth;
    UInt64 expectedNodes;
};

class Perft {
public:
    static inline PerftTest TEST_CASES[] = {
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860 },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 8, 3009794393 },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690 },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033 },
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 0 1", 5, 89941194 },
        { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1", 5, 164075551 },
        { "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888 },
        { "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1015133 },
        { "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467 },
        { "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072 },
        { "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711 },
        { "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206 },
        { "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476 },
        { "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001 },
        { "8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683 },
        { "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217 },
        { "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342 },
        { "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527 },
        { "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584 },
        { "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527 }
    };

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
            position.unmake();
        }

        return nodes;
    }
};

}
