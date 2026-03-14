
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "../src/attacks.hpp"
#include "../src/perft.hpp"
#include "../src/position.hpp"


struct TestCase {
    std::string fen;
    std::uint64_t expectedNodes;
    int depth;
};

void benchmark(const TestCase &testCase) {
    Clownfish::Position position = Clownfish::Position(testCase.fen);

    const auto start = std::chrono::high_resolution_clock::now();
    const std::uint64_t nodes = Clownfish::Perft::perft(position, testCase.depth);
    const auto end = std::chrono::high_resolution_clock::now();
    const std::uint64_t duration = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    std::cout << "depth " << std::left << std::setw(2) << testCase.depth;
    std::cout << " time " << std::setw(5) << duration;
    std::cout << " nodes " << std::setw(12) << nodes;
    std::cout << " nps " << std::setw(9) << (nodes * 1000) / (duration + 1);
    std::cout << " fen " << std::setw(87) << testCase.fen;
    std::cout << std::endl;

    if (nodes != testCase.expectedNodes) {
        std::cerr << "expected " << testCase.expectedNodes << " nodes, got " << nodes << std::endl;
    }
}
int main() {
    Clownfish::Attacks::init();

    const TestCase testCases[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3195901860, 7},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 193690690, 5},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 178633661, 7},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 706045033, 6},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 89941194, 5},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1", 164075551, 5}
    };

    for (const TestCase &testCase : testCases) {
        benchmark(testCase);
    }
}
