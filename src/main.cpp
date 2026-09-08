
#include <string_view>
#include <thread>

#include "attacks.hpp"
#include "cuckoo.hpp"
#include "numa.hpp"
#include "tunable.hpp"
#include "uci.hpp"


using namespace Sift;

int main(int argc, const char *argv[]) {
    Attacks::init();
    CuckooTable::init();
    TunableList::init();
    NUMA::init();

    UCI uci = UCI();

    if (argc > 1) {
#if defined(OPEN_BENCH_TUNE)
        if (std::string_view(argv[1]) == "obconfig") {
            TUNABLES.openBenchConfig();

            return 0;
        }
#endif

        for (int i = 1; i < argc; i++) {
            uci.execute(argv[i]);
            while (uci.searching()) {
                std::this_thread::yield();
            }
        }

        return 0;
    }


    uci.run();

    return 0;
}
