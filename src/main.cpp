
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
        std::string args = argv[1];

#if defined(OPEN_BENCH_TUNE)
        if (args == "obconfig") {
            TUNABLES.openBenchConfig();
            return 0;
    }
#endif

        for (int i = 2; i < argc; i++) {
            args += " ";
            args += argv[i];
        }

        uci.execute(args);
        while (uci.searching()) {
            std::this_thread::yield();
        }

        return 0;
}


    uci.run();

    return 0;
}
