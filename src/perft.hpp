#pragma once

#include <string_view>

#include "move.hpp"
#include "move-gen.hpp"
#include "position.hpp"
#include "types.hpp"


namespace Sift {

struct PerftTest {
    std::string_view fen;
    Int32 depth;
    UInt64 expectedNodes;
};

class Perft {
public:
    static constexpr USize TEST_COUNT = 128;

    static constexpr std::array<PerftTest, TEST_COUNT> TEST_CASES = {
        PerftTest("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324),
        PerftTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690),
        PerftTest("4k3/8/8/8/8/8/8/4K2R w K - 0 1", 6, 764643),
        PerftTest("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 846648),
        PerftTest("4k2r/8/8/8/8/8/8/4K3 w k - 0 1", 6, 899442),
        PerftTest("r3k3/8/8/8/8/8/8/4K3 w q - 0 1", 6, 1001523),
        PerftTest("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1", 6, 2788982),
        PerftTest("r3k2r/8/8/8/8/8/8/4K3 w kq - 0 1", 6, 3517770),
        PerftTest("8/8/8/8/8/8/6k1/4K2R w K - 0 1", 6, 185867),
        PerftTest("8/8/8/8/8/8/1k6/R3K3 w Q - 0 1", 6, 413018),
        PerftTest("4k2r/6K1/8/8/8/8/8/8 w k - 0 1", 6, 179869),
        PerftTest("r3k3/1K6/8/8/8/8/8/8 w q - 0 1", 6, 367724),
        PerftTest("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 6, 179862938),
        PerftTest("r3k2r/8/8/8/8/8/8/1R2K2R w Kkq - 0 1", 6, 195629489),
        PerftTest("r3k2r/8/8/8/8/8/8/2R1K2R w Kkq - 0 1", 6, 184411439),
        PerftTest("r3k2r/8/8/8/8/8/8/R3K1R1 w Qkq - 0 1", 6, 189224276),
        PerftTest("1r2k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1", 6, 198328929),
        PerftTest("2r1k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1", 6, 185959088),
        PerftTest("r3k1r1/8/8/8/8/8/8/R3K2R w KQq - 0 1", 6, 190755813),
        PerftTest("4k3/8/8/8/8/8/8/4K2R b K - 0 1", 6, 899442),
        PerftTest("4k3/8/8/8/8/8/8/R3K3 b Q - 0 1", 6, 1001523),
        PerftTest("4k2r/8/8/8/8/8/8/4K3 b k - 0 1", 6, 764643),
        PerftTest("r3k3/8/8/8/8/8/8/4K3 b q - 0 1", 6, 846648),
        PerftTest("4k3/8/8/8/8/8/8/R3K2R b KQ - 0 1", 6, 3517770),
        PerftTest("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1", 6, 2788982),
        PerftTest("8/8/8/8/8/8/6k1/4K2R b K - 0 1", 6, 179869),
        PerftTest("8/8/8/8/8/8/1k6/R3K3 b Q - 0 1", 6, 367724),
        PerftTest("4k2r/6K1/8/8/8/8/8/8 b k - 0 1", 6, 185867),
        PerftTest("r3k3/1K6/8/8/8/8/8/8 b q - 0 1", 6, 413018),
        PerftTest("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", 6, 179862938),
        PerftTest("r3k2r/8/8/8/8/8/8/1R2K2R b Kkq - 0 1", 6, 198328929),
        PerftTest("r3k2r/8/8/8/8/8/8/2R1K2R b Kkq - 0 1", 6, 185959088),
        PerftTest("r3k2r/8/8/8/8/8/8/R3K1R1 b Qkq - 0 1", 6, 190755813),
        PerftTest("1r2k2r/8/8/8/8/8/8/R3K2R b KQk - 0 1", 6, 195629489),
        PerftTest("2r1k2r/8/8/8/8/8/8/R3K2R b KQk - 0 1", 6, 184411439),
        PerftTest("r3k1r1/8/8/8/8/8/8/R3K2R b KQq - 0 1", 6, 189224276),
        PerftTest("8/1n4N1/2k5/8/8/5K2/1N4n1/8 w - - 0 1", 6, 8107539),
        PerftTest("8/1k6/8/5N2/8/4n3/8/2K5 w - - 0 1", 6, 2594412),
        PerftTest("8/8/4k3/3Nn3/3nN3/4K3/8/8 w - - 0 1", 6, 19870403),
        PerftTest("K7/8/2n5/1n6/8/8/8/k6N w - - 0 1", 6, 588695),
        PerftTest("k7/8/2N5/1N6/8/8/8/K6n w - - 0 1", 6, 688780),
        PerftTest("8/1n4N1/2k5/8/8/5K2/1N4n1/8 b - - 0 1", 6, 8503277),
        PerftTest("8/1k6/8/5N2/8/4n3/8/2K5 b - - 0 1", 6, 3147566),
        PerftTest("8/8/3K4/3Nn3/3nN3/4k3/8/8 b - - 0 1", 6, 4405103),
        PerftTest("K7/8/2n5/1n6/8/8/8/k6N b - - 0 1", 6, 688780),
        PerftTest("k7/8/2N5/1N6/8/8/8/K6n b - - 0 1", 6, 588695),
        PerftTest("B6b/8/8/8/2K5/4k3/8/b6B w - - 0 1", 6, 22823890),
        PerftTest("8/8/1B6/7b/7k/8/2B1b3/7K w - - 0 1", 6, 28861171),
        PerftTest("k7/B7/1B6/1B6/8/8/8/K6b w - - 0 1", 6, 7881673),
        PerftTest("K7/b7/1b6/1b6/8/8/8/k6B w - - 0 1", 6, 7382896),
        PerftTest("B6b/8/8/8/2K5/5k2/8/b6B b - - 0 1", 6, 9250746),
        PerftTest("8/8/1B6/7b/7k/8/2B1b3/7K b - - 0 1", 6, 29027891),
        PerftTest("k7/B7/1B6/1B6/8/8/8/K6b b - - 0 1", 6, 7382896),
        PerftTest("K7/b7/1b6/1b6/8/8/8/k6B b - - 0 1", 6, 7881673),
        PerftTest("7k/RR6/8/8/8/8/rr6/7K w - - 0 1", 6, 44956585),
        PerftTest("R6r/8/8/2K5/5k2/8/8/r6R w - - 0 1", 6, 525169084),
        PerftTest("7k/RR6/8/8/8/8/rr6/7K b - - 0 1", 6, 44956585),
        PerftTest("R6r/8/8/2K5/5k2/8/8/r6R b - - 0 1", 6, 524966748),
        PerftTest("6kq/8/8/8/8/8/8/7K w - - 0 1", 6, 391507),
        PerftTest("6KQ/8/8/8/8/8/8/7k b - - 0 1", 6, 391507),
        PerftTest("K7/8/8/3Q4/4q3/8/8/7k w - - 0 1", 6, 3370175),
        PerftTest("6qk/8/8/8/8/8/8/7K b - - 0 1", 6, 419369),
        PerftTest("6KQ/8/8/8/8/8/8/7k b - - 0 1", 6, 391507),
        PerftTest("K7/8/8/3Q4/4q3/8/8/7k b - - 0 1", 6, 3370175),
        PerftTest("8/8/8/8/8/K7/P7/k7 w - - 0 1", 6, 6249),
        PerftTest("8/8/8/8/8/7K/7P/7k w - - 0 1", 6, 6249),
        PerftTest("K7/p7/k7/8/8/8/8/8 w - - 0 1", 6, 2343),
        PerftTest("7K/7p/7k/8/8/8/8/8 w - - 0 1", 6, 2343),
        PerftTest("8/2k1p3/3pP3/3P2K1/8/8/8/8 w - - 0 1", 6, 34834),
        PerftTest("8/8/8/8/8/K7/P7/k7 b - - 0 1", 6, 2343),
        PerftTest("8/8/8/8/8/7K/7P/7k b - - 0 1", 6, 2343),
        PerftTest("K7/p7/k7/8/8/8/8/8 b - - 0 1", 6, 6249),
        PerftTest("7K/7p/7k/8/8/8/8/8 b - - 0 1", 6, 6249),
        PerftTest("8/2k1p3/3pP3/3P2K1/8/8/8/8 b - - 0 1", 6, 34822),
        PerftTest("8/8/8/8/8/4k3/4P3/4K3 w - - 0 1", 6, 11848),
        PerftTest("4k3/4p3/4K3/8/8/8/8/8 b - - 0 1", 6, 11848),
        PerftTest("8/8/7k/7p/7P/7K/8/8 w - - 0 1", 6, 10724),
        PerftTest("8/8/k7/p7/P7/K7/8/8 w - - 0 1", 6, 10724),
        PerftTest("8/8/3k4/3p4/3P4/3K4/8/8 w - - 0 1", 6, 53138),
        PerftTest("8/3k4/3p4/8/3P4/3K4/8/8 w - - 0 1", 6, 157093),
        PerftTest("8/8/3k4/3p4/8/3P4/3K4/8 w - - 0 1", 6, 158065),
        PerftTest("k7/8/3p4/8/3P4/8/8/7K w - - 0 1", 6, 20960),
        PerftTest("8/8/7k/7p/7P/7K/8/8 b - - 0 1", 6, 10724),
        PerftTest("8/8/k7/p7/P7/K7/8/8 b - - 0 1", 6, 10724),
        PerftTest("8/8/3k4/3p4/3P4/3K4/8/8 b - - 0 1", 6, 53138),
        PerftTest("8/3k4/3p4/8/3P4/3K4/8/8 b - - 0 1", 6, 158065),
        PerftTest("8/8/3k4/3p4/8/3P4/3K4/8 b - - 0 1", 6, 157093),
        PerftTest("k7/8/3p4/8/3P4/8/8/7K b - - 0 1", 6, 21104),
        PerftTest("7k/3p4/8/8/3P4/8/8/K7 w - - 0 1", 6, 32191),
        PerftTest("7k/8/8/3p4/8/8/3P4/K7 w - - 0 1", 6, 30980),
        PerftTest("k7/8/8/7p/6P1/8/8/K7 w - - 0 1", 6, 41874),
        PerftTest("k7/8/7p/8/8/6P1/8/K7 w - - 0 1", 6, 29679),
        PerftTest("k7/8/8/6p1/7P/8/8/K7 w - - 0 1", 6, 41874),
        PerftTest("k7/8/6p1/8/8/7P/8/K7 w - - 0 1", 6, 29679),
        PerftTest("k7/8/8/3p4/4p3/8/8/7K w - - 0 1", 6, 22886),
        PerftTest("k7/8/3p4/8/8/4P3/8/7K w - - 0 1", 6, 28662),
        PerftTest("7k/3p4/8/8/3P4/8/8/K7 b - - 0 1", 6, 32167),
        PerftTest("7k/8/8/3p4/8/8/3P4/K7 b - - 0 1", 6, 30749),
        PerftTest("k7/8/8/7p/6P1/8/8/K7 b - - 0 1", 6, 41874),
        PerftTest("k7/8/7p/8/8/6P1/8/K7 b - - 0 1", 6, 29679),
        PerftTest("k7/8/8/6p1/7P/8/8/K7 b - - 0 1", 6, 41874),
        PerftTest("k7/8/6p1/8/8/7P/8/K7 b - - 0 1", 6, 29679),
        PerftTest("k7/8/8/3p4/4p3/8/8/7K b - - 0 1", 6, 22579),
        PerftTest("k7/8/3p4/8/8/4P3/8/7K b - - 0 1", 6, 28662),
        PerftTest("7k/8/8/p7/1P6/8/8/7K w - - 0 1", 6, 41874),
        PerftTest("7k/8/p7/8/8/1P6/8/7K w - - 0 1", 6, 29679),
        PerftTest("7k/8/8/1p6/P7/8/8/7K w - - 0 1", 6, 41874),
        PerftTest("7k/8/1p6/8/8/P7/8/7K w - - 0 1", 6, 29679),
        PerftTest("k7/7p/8/8/8/8/6P1/K7 w - - 0 1", 6, 55338),
        PerftTest("k7/6p1/8/8/8/8/7P/K7 w - - 0 1", 6, 55338),
        PerftTest("3k4/3pp3/8/8/8/8/3PP3/3K4 w - - 0 1", 6, 199002),
        PerftTest("7k/8/8/p7/1P6/8/8/7K b - - 0 1", 6, 41874),
        PerftTest("7k/8/p7/8/8/1P6/8/7K b - - 0 1", 6, 29679),
        PerftTest("7k/8/8/1p6/P7/8/8/7K b - - 0 1", 6, 41874),
        PerftTest("7k/8/1p6/8/8/P7/8/7K b - - 0 1", 6, 29679),
        PerftTest("k7/7p/8/8/8/8/6P1/K7 b - - 0 1", 6, 55338),
        PerftTest("k7/6p1/8/8/8/8/7P/K7 b - - 0 1", 6, 55338),
        PerftTest("3k4/3pp3/8/8/8/8/3PP3/3K4 b - - 0 1", 6, 199002),
        PerftTest("8/Pk6/8/8/8/8/6Kp/8 w - - 0 1", 6, 1030499),
        PerftTest("n1n5/1Pk5/8/8/8/8/5Kp1/5N1N w - - 0 1", 6, 37665329),
        PerftTest("8/PPPk4/8/8/8/8/4Kppp/8 w - - 0 1", 6, 28859283),
        PerftTest("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N w - - 0 1", 6, 71179139),
        PerftTest("8/Pk6/8/8/8/8/6Kp/8 b - - 0 1", 6, 1030499),
        PerftTest("n1n5/1Pk5/8/8/8/8/5Kp1/5N1N b - - 0 1", 6, 37665329),
        PerftTest("8/PPPk4/8/8/8/8/4Kppp/8 b - - 0 1", 6, 28859283),
        PerftTest("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 6, 71179139),
        PerftTest("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11030083),
        PerftTest("rnbqkb1r/ppppp1pp/7n/4Pp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3", 5, 11139762)
    };


    template<MoveGenType MOVE_GEN_TYPE = MoveGenType::ALL>
    static UInt64 run(Position &position, Int32 depth) {
        if (depth <= 0) {
            return 1;
        }

        MoveList moveList;
        if (depth == 1) {
            MoveGen::legal<MOVE_GEN_TYPE>(position, moveList);
            return static_cast<UInt64>(moveList.size());
        }

        UInt64 nodes = 0;
        MoveGen::legal<MOVE_GEN_TYPE>(position, moveList);
        for (const Move move : moveList) {
            position.makeMove(move);
            nodes += run<MOVE_GEN_TYPE>(position, depth - 1);
            position.unmakeMove();
        }

        return nodes;
    }
};

}
