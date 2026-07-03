#include "Cache.h"
#include "Config.h"
#include "ConfigParser.h"
#include "TraceReader.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

static void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " <trace-file> [options]\n"
        "\n"
        "Options:\n"
        "  --config F        load settings from a key=value config file\n"
        "  --capacity N      total cache size in bytes        (default 32768)\n"
        "  --block-size N    bytes per cache line             (default 64)\n"
        "  --assoc N         ways per set                     (default 4)\n"
        "  --policy P        lru | fifo | random              (default lru)\n"
        "  --write-policy P  writeback | writethrough         (default writeback)\n"
        "  --write-alloc P   allocate | no-allocate           (default allocate)\n"
        "  --name S          label shown in output            (default L1)\n"
        "  --help            show this message\n"
        "\n"
        "Precedence: CLI flags override values loaded from --config.\n"
        "\n"
        "Trace file format (one access per line):\n"
        "  R <hex-address>   read\n"
        "  W <hex-address>   write\n"
        "  Lines beginning with '#' are treated as comments.\n"
        "\n"
        "Config file format (key = value, one per line):\n"
        "  cache_size    = 32K\n"
        "  block_size    = 64\n"
        "  associativity = 4\n"
        "  replacement   = lru\n"
        "  write_policy  = write_back\n"
        "  write_miss    = write_allocate\n";
}

// Apply CLI flags (argv[2..]) to `cfg`. Skips --config / its argument since
// the config file is handled in a separate pass before this is called.
static void applyCliArgs(int argc, char* argv[], CacheConfig& cfg) {
    for (int i = 2; i < argc; ++i) {
        // Skip --config and its path argument — already handled.
        if (std::strcmp(argv[i], "--config") == 0) { ++i; continue; }

        auto need = [&]() -> std::string {
            if (i + 1 >= argc)
                throw std::invalid_argument(std::string(argv[i]) + " requires a value");
            return argv[++i];
        };

        if (std::strcmp(argv[i], "--capacity") == 0) {
            cfg.capacityBytes = std::stoull(need());
        } else if (std::strcmp(argv[i], "--block-size") == 0) {
            cfg.blockBytes = std::stoi(need());
        } else if (std::strcmp(argv[i], "--assoc") == 0) {
            cfg.associativity = std::stoi(need());
        } else if (std::strcmp(argv[i], "--policy") == 0) {
            const std::string v = need();
            if      (v == "fifo")   cfg.replacementPolicy = ReplacementPolicy::FIFO;
            else if (v == "random") cfg.replacementPolicy = ReplacementPolicy::Random;
            else                    cfg.replacementPolicy = ReplacementPolicy::LRU;
        } else if (std::strcmp(argv[i], "--write-policy") == 0) {
            const std::string v = need();
            cfg.writePolicy = (v == "writethrough")
                            ? WritePolicy::WriteThrough
                            : WritePolicy::WriteBack;
        } else if (std::strcmp(argv[i], "--write-alloc") == 0) {
            const std::string v = need();
            cfg.writeMissPolicy = (v == "no-allocate")
                                ? WriteMissPolicy::NoWriteAllocate
                                : WriteMissPolicy::WriteAllocate;
        } else if (std::strcmp(argv[i], "--name") == 0) {
            cfg.name = need();
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument(std::string("unknown option: ") + argv[i]);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0) {
        printUsage(argv[0]);
        return argc < 2 ? 1 : 0;
    }

    // Phase 1 — load config file if --config is present.
    // This happens before CLI flags so that subsequent flags can override it.
    CacheConfig cfg;
    for (int i = 2; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--config") == 0) {
            try {
                cfg = ConfigParser::fromFile(argv[i + 1], cfg);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                return 1;
            }
            break; // only the first --config is applied
        }
    }

    // Phase 2 — apply CLI flags (overrides config file values).
    try {
        applyCliArgs(argc, argv, cfg);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        printUsage(argv[0]);
        return 1;
    }

    // Open the trace file first so we fail fast before constructing the cache.
    TraceReader reader(argv[1]);
    if (!reader.isOpen()) {
        std::cerr << "Error: cannot open trace file '" << argv[1] << "'\n";
        return 1;
    }

    // Construct and validate the cache (throws on bad geometry).
    Cache cache(cfg);

    // Print the resolved configuration.
    std::cout << "Cache configuration:\n"
              << "  name          : " << cfg.name                        << "\n"
              << "  capacity      : " << cfg.capacityBytes << " B\n"
              << "  block size    : " << cfg.blockBytes    << " B\n"
              << "  associativity : " << cfg.associativity << "-way\n"
              << "  sets          : " << cache.numSets()   << "\n"
              << "  replacement   : "
              << (cfg.replacementPolicy == ReplacementPolicy::LRU    ? "LRU"
                : cfg.replacementPolicy == ReplacementPolicy::FIFO   ? "FIFO"
                                                                     : "Random") << "\n"
              << "  write policy  : "
              << (cfg.writePolicy == WritePolicy::WriteBack ? "write-back"
                                                           : "write-through") << "\n"
              << "  write-miss    : "
              << (cfg.writeMissPolicy == WriteMissPolicy::WriteAllocate ? "write-allocate"
                                                                        : "no-write-allocate") << "\n"
              << "\n";

    // Main simulation loop.
    while (auto entry = reader.next())
        cache.access(entry->address, entry->op == 'W');

    std::cout << "Simulation complete (" << reader.lineNumber() << " lines read).\n\n";
    cache.printStats();
    return 0;
}
