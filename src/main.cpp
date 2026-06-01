
#include <string_view>

#include "attacks.hpp"
#include "cuckoo.hpp"
#include "tunable.hpp"
#include "uci.hpp"

using namespace Syft;

int main(int argc, const char *argv[]) {
    Attacks::init();
    CuckooTable::init();

#if defined(OPEN_BENCH_TUNE)
    TunableList::init();
#endif

    if (argc > 1) {
        const std::string_view mode = argv[1];

#if defined(OPEN_BENCH_TUNE)
        if (mode == "obconfig") {
            TUNABLES.openBenchConfig();
            return 0;
        }
#endif
    }

    UCI uci;
    uci.run();

    return 0;
}
