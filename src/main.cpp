
#include <iostream>
#include <string>
#include "search.hpp"
#include "types.hpp"

using namespace Clownfish;
using namespace std;

void printInfo(const SearchInfo &info);

int main() {
    Attacks::init();
    CuckooTable::init();

    Position position = Position("8/3br3/4k3/8/8/4K3/8/8 b - - 0 1");
    SearchLimits limits;
    limits.time = MS(10000);

    Search search = Search(16, printInfo);
    search.newGame();
    auto [move, score] = search.run(position, limits);
    cout << "bestmove " << string(move) << endl;
}

void printInfo(const SearchInfo &info) {
    cout << "info depth " << info.depth;
    cout << " seldepth " << info.selDepth;
    cout << " time " << info.time.count();
    cout << " nodes " << info.nodes;
    const UInt64 elapsedMs = static_cast<UInt64>(info.time.count());
    UInt64 nps = (info.nodes * 1000ULL) / (elapsedMs + 1ULL);
    cout << " nps " << nps;
    cout << " hashfull " << info.hashfull;
    cout << " score ";
    if (Score::mate(info.score)) {
        if (info.score > 0) {
            cout << "mate " << ((Score::MATE - info.score) + 1) / 2;
        } else {
            cout << "mate -" << (info.score + Score::MATE) / 2;
        }
    } else {
        cout << "cp " << info.score;
    }

    if (info.lowerBound) {
        cout << " lowerbound";
    } else if (info.upperBound) {
        cout << " upperbound";
    }

    cout << " pv ";
    for (Move move : info.pv) {
        cout << string(move) << " ";
    }
    cout << std::endl;
}
