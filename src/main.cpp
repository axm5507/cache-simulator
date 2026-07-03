#include "Cache.h"
#include "Config.h"
#include "ConfigParser.h"
#include "TraceReader.h"
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

static void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " <trace-file> [options]\n"
        "\n"
        "Options:\n"
        "  --config F        load L1 settings from a key=value config file\n"
        "  --l2-config F     load L2 settings and enable two-level simulation\n"
        "  --capacity N      L1 cache size in bytes               (default 32768)\n"
        "  --block-size N    bytes per cache line                 (default 64)\n"
        "  --assoc N         ways per set                         (default 4)\n"
        "  --policy P        lru | fifo | random                  (default lru)\n"
        "  --write-policy P  writeback | writethrough             (default writeback)\n"
        "  --write-alloc P   allocate | no-allocate               (default allocate)\n"
        "  --name S          label shown in the report            (default L1)\n"
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
        "  write_miss    = write_allocate\n"
        "\n"
        "Run ./gen_traces [dir] to generate benchmark trace files.\n";
}

// Apply CLI flags (argv[2..]) to cfg.
// Skips --config and --l2-config (handled in a separate pass).
static void applyCliArgs(int argc, char* argv[], CacheConfig& cfg) {
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config")    == 0 ||
            std::strcmp(argv[i], "--l2-config") == 0) { ++i; continue; }

        auto need = [&]() -> std::string {
            if (i + 1 >= argc)
                throw std::invalid_argument(std::string(argv[i]) + " requires a value");
            return argv[++i];
        };

        if      (std::strcmp(argv[i], "--capacity")    == 0) cfg.capacityBytes = std::stoull(need());
        else if (std::strcmp(argv[i], "--block-size")  == 0) cfg.blockBytes    = std::stoi(need());
        else if (std::strcmp(argv[i], "--assoc")       == 0) cfg.associativity = std::stoi(need());
        else if (std::strcmp(argv[i], "--policy")      == 0) {
            const std::string v = need();
            if      (v == "fifo")   cfg.replacementPolicy = ReplacementPolicy::FIFO;
            else if (v == "random") cfg.replacementPolicy = ReplacementPolicy::Random;
            else                    cfg.replacementPolicy = ReplacementPolicy::LRU;
        }
        else if (std::strcmp(argv[i], "--write-policy") == 0) {
            const std::string v = need();
            cfg.writePolicy = (v == "writethrough") ? WritePolicy::WriteThrough
                                                    : WritePolicy::WriteBack;
        }
        else if (std::strcmp(argv[i], "--write-alloc") == 0) {
            const std::string v = need();
            cfg.writeMissPolicy = (v == "no-allocate") ? WriteMissPolicy::NoWriteAllocate
                                                       : WriteMissPolicy::WriteAllocate;
        }
        else if (std::strcmp(argv[i], "--name") == 0) cfg.name = need();
        else if (std::strcmp(argv[i], "--help") == 0) { printUsage(argv[0]); std::exit(0); }
        else throw std::invalid_argument(std::string("unknown option: ") + argv[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0) {
        printUsage(argv[0]);
        return argc < 2 ? 1 : 0;
    }

    // Phase 1: load config files.
    // --config sets L1 parameters; --l2-config enables two-level simulation.
    CacheConfig l1cfg;
    std::optional<CacheConfig> l2cfg;

    for (int i = 2; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--config") == 0) {
            try { l1cfg = ConfigParser::fromFile(argv[i + 1], l1cfg); }
            catch (const std::exception& e) { std::cerr << "Error: " << e.what() << "\n"; return 1; }
            break;
        }
    }
    for (int i = 2; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--l2-config") == 0) {
            CacheConfig l2defaults;
            l2defaults.name          = "L2";
            l2defaults.capacityBytes = 256 * 1024;
            l2defaults.blockBytes    = 64;
            l2defaults.associativity = 8;
            try { l2cfg = ConfigParser::fromFile(argv[i + 1], l2defaults); }
            catch (const std::exception& e) { std::cerr << "Error: " << e.what() << "\n"; return 1; }
            break;
        }
    }

    // Phase 2: apply CLI flags on top of any config file values.
    try { applyCliArgs(argc, argv, l1cfg); }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        printUsage(argv[0]);
        return 1;
    }

    // Open trace file early so we fail fast before constructing the cache.
    TraceReader reader(argv[1]);
    if (!reader.isOpen()) {
        std::cerr << "Error: cannot open trace file '" << argv[1] << "'\n";
        return 1;
    }

    // Run simulation and print report.
    try {
        Cache l1(l1cfg);
        std::optional<Cache> l2;
        if (l2cfg) l2.emplace(*l2cfg);

        while (auto entry = reader.next()) {
            if (!l1.access(entry->address, entry->op == 'W') && l2)
                l2->access(entry->address, entry->op == 'W');
        }

        std::cout << "Simulation complete (" << reader.lineNumber() << " accesses).\n\n";
        l1.printStats();
        if (l2) { std::cout << "\n"; l2->printStats(); }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
