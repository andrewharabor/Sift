
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "../src/attacks.hpp"
#include "../src/benchmark.hpp"
#include "../src/move-generator.hpp"
#include "../src/position.hpp"

using namespace Clownfish;


struct TestCase {
    std::string fen;
    int depth;
    std::uint64_t expectedNodes;
};

template<MoveGenerationType MGT = MoveGenerationType::ALL>
void benchmark(const TestCase &testCase) {
    Position position = Position(testCase.fen);

    const auto start = std::chrono::high_resolution_clock::now();
    const std::uint64_t nodes = Benchmark::perft<MGT>(position, testCase.depth);
    const auto end = std::chrono::high_resolution_clock::now();
    const std::uint64_t duration = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    std::string nodeLabel = "";
    if constexpr (MGT == MoveGenerationType::ALL) {
        nodeLabel = "nodes";
    } else if constexpr (MGT == MoveGenerationType::CAPTURES) {
        nodeLabel = "captures";
    } else if constexpr (MGT == MoveGenerationType::QUIET) {
        nodeLabel = "quiets";
    } else if constexpr (MGT == MoveGenerationType::CHECKS) {
        nodeLabel = "checks";
    } else {
        static_assert(false);
    }

    std::cout << "fen " << testCase.fen;
    std::cout << " depth " << testCase.depth << std::endl;
    std::cout << "\t" << nodeLabel << " " << nodes << std::endl;
    std::cout << "\ttime " << duration << " ms" << std::endl;
    std::cout << "\tnps " << (nodes * 1000) / (duration + 1) << std::endl;

    if (nodes != testCase.expectedNodes) {
        std::cerr << "\t\texpected " << testCase.expectedNodes << " " << nodeLabel << ", got " << nodes << std::endl;
    }
}

int main() {
    Attacks::init();

    const TestCase testCasesAll[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 8, 3009794393},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 0 1", 5, 89941194},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1", 5, 164075551}
    };

    for (const TestCase &testCase : testCasesAll) {
        benchmark<MoveGenerationType::ALL>(testCase);
    }

    const TestCase testCasesCaptures[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 108329926 },
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 35043416},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 8, 267586558},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 210369132}
    };

    for (const TestCase &testCase : testCasesCaptures) {
        benchmark<MoveGenerationType::CAPTURES>(testCase);
    }

    const TestCase testCasesChecks[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 33103848 },
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 3309887},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 ", 7, 12797406},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 26973664},
    };

    for (const TestCase &testCase : testCasesChecks) {
        benchmark<MoveGenerationType::CHECKS>(testCase);
    }

}
