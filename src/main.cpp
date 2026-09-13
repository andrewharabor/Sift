
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "attacks.hpp"
#include "cuckoo.hpp"
#include "nnue.hpp"
#include "numa.hpp"
#include "tunable.hpp"
#include "uci.hpp"


using namespace Sift;

int run(std::span<const std::string_view> args) {
    UCI uci = UCI();

    if (args.size() > 1) {
#if defined(EXTERNAL_TUNE)
        if (std::string_view(args[1]) == "obconfig") {
            TUNABLES.openBenchConfig();
            return 0;
        } else if (std::string_view(args[1]) == "wfconfig") {
            TUNABLES.weatherFactoryConfig();
            return 0;
        }
#endif

        for (USize i = 1; i < args.size(); i++) {
            uci.execute(std::string(args[i]));
            while (uci.searching()) {
                std::this_thread::yield();
            }
        }

        return 0;
    }

    uci.run();

    return 0;
}

int main(int argc, const char *argv[]) {
    Attacks::init();
    CuckooTable::init();
    TunableList::init();
    NUMA::init();
    NetLoader::init();

    std::vector<std::string_view> args;
    args.reserve(argc);
    for (int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    const int exitCode = run(args);

    NetLoader::cleanup();

    return exitCode;
}
