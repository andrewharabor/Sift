
#include "attacks.hpp"
#include "cuckoo.hpp"
#include "uci.hpp"

using namespace Syft;

int main() {
    Attacks::init();
    CuckooTable::init();

    UCI uci;
    uci.run();

    return 0;
}
