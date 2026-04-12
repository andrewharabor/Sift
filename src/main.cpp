
#include "attacks.hpp"
#include "cuckoo.hpp"
#include "uci.hpp"

using namespace Clownfish;

int main() {
    Attacks::init();
    CuckooTable::init();
    UCI uci;
    uci.run();

    return 0;
}
