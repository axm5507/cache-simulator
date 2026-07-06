# Cache Simulator

This project is a CPU cache simulator written in C++17.

## Overview

This simulator models how a CPU cache behaves when replaying memory access traces. It will support configurable cache geometry, replacement policies, and write policies.

## Project Structure

```
cache-simulator/
├── include/                 # Public headers
│   ├── Cache.h              # Single cache level
│   ├── CacheSet.h           # One set
│   ├── CacheLine.h          # One cache line
│   ├── TraceReader.h        # Streaming trace file parser
│   ├── Config.h             # Configuration structs and policy enums
|   ├── IReplacementPolicy.h # Methods cache calls during simulation
|   └── ConfigParser.h       # Loads cache parameters from files
├── src/                     # Implementation files + main entry point
├── configs/                 # Example JSON/INI cache configuration files
├── traces/                  # Memory access trace files
├── benchmarks/              # Benchmark scripts and workloads
├── results/                 # Simulation output files
└── tests/                   # Unit and integration tests
```


## How to Run:

**Prerequisites:**

`Cmake`

**Building:**

```bash
cmake -S . -B build
cmake --build build
```

(On Windows the executables land in `build\Debug\` by default, to build an optimized release, use this:)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Running a Simulation**

```bash
cache_sim <trace-file> [options]
```

The simulator reads a trace file line by line and replays each memory access against the configured cache. When it's done, it prints a full report.

To run default config(32 KiB, 64B blocks, 4 way LRU write back)

```bash
cache_sum traces/sample.trace
```

To load a config file:

```bash
cache_sim traces/sample.trace --config configs/default.conf
```

For two level L1 + L2 simulation:

```bash
cache_sim traces/sample.trace --config configs/default/conf --l2-config configs/l2_cache.conf
```

**All Options**:

Load L1 settings from a config file:

`--config F`

Load L2 settings and enable a two level sim:

`--l2-config F`

Cache size in bytes:

`--capacity N`

Bytes per cache line:

`--block-size N`

Ways per set:

`--assoc N`

Policy(lru, fifo, random):

`--policy P`

Write policy(writeback, writethrough):

`--write-policy P`

Write allocation(allocate, no-allocate):

`--write-alloc P`

Label shown in report:

`--name S`

**Config File Format**

```text
#Any comments begin with a '#'
name = L1
cache_size = 32K    # K and M suffixes are supported
block_size = 64
associativity = 4
replacement = lru
write_policy = write_back
write_miss = write_allocate
```

There are 4 ready made configs in `configs/`.

**Generating Benchmark Traces:**

```bash
gen_traces traces/
```

This creates 4 trace files in the given directory. They are:

`sequential.trace` - Two passes over 512 lines where stride = block size, cold start misses on pass 1 and all hits on pass 2.

`random.trace` - 1024 pseudo random accesses in 1 MiB i=window, shows the effect of poor temporal locality

`matrix_row_major.trace` - 128x128 int matrix, row major, shows low miss rate with 16 ints sharing one cache line

`matrix_col_major.trace` - 128x128 int matrix, column major, shows near 100% miss rate with stride = 512 B(one new line per access)

**Running Tests:**

```bash
cd build
ctest --output-on-failure
```

OR, run the test binary directly to get more verbose output.

```bash
.\build\Debug\cache_sim_tests.exe  #Windows
./build/cache_sim_tests    #Linux/mac
```

This covers address decomposition, hit/miss detection, dirty bit behavior, LRU/FIFO eviction ordering, random replacement validity, fully associative and direct mapped configs, writeback/writethrough/writeallocate/nowriteallocate policies, trace parsing edge cases, and config file loading.


## Progress Updates

**Update 1:** 
I created a CacheLine class that contains: valid bit, dirty bit, tag, last used timestamp, and insertion timestamp. I then implemented a CacheSet class that stores multiple CacheLines, knows its associativity, can search for matching tags, determine whether an empty line exists, and returns indices of matching lines. Then, I implemented a Cache class that stores cache size, block size, associativity, number of sets, and a vector of CacheSets. It calculates number of offset bits, number of index bits, and tag extraction.

**Update 2:**
I added to the Cache class so it now decomposes any 64 bit address into 3 bit fields: block offset, set index, and tag. The widths of these fields are automatically derived from the cache geometry at construction time. Unit tests verify the correctness for hex addresses and confirm that all reconstructing is lossless. Furthermore, I added Cache::access() which implements the cache reads. It extracts the set index and tag, searches the set for a matching valid line, and returns hit/miss. On a hit the LRU timestamp of the line is refreshed, and on a miss either the block is loaded into an empty way or another line is chosen and evicted first. Three replacement policies are supported, which are the standard LRU(evict least recently used), FIFO(evict first inserted), and Random(splitmix64). The tests verify everything works as intended. Finally, write behavior is controlled by two write policies: write back and write through. Write back marks a line dirty and defers the memory write until eviction, while write through adds every write to memory immediately and never sets the dirty bit. The write miss policy determines whether a missing line is fetched into the cache before writing. 

**Update 3:**
Before this step, the cache contained a big clunky switch statement that handled LRU, FIFO, and Random eviction directly inside the simulation code. This meant that adding a new policy/changing an existing one required editing the core Cache class, which was something I wanted to fix. Because of this, I introduced an IReplacementPolicy class with 3 methods the cache calls during simulation. onLoad is called with a new block is installed, onAccess is called on a cache hit, and selectVictim is called when a set needs to be evicted. Each replacement policy implements these 3 methods with their own logic. The cache class now holds a pointer to the interface and calls it without knowing which policy is active.

**Update 4:**
I implemented the trace reader. A trace file is a record of every memory access a program makes: its address, and whether it was a read or a write. The simulator needs to replay these accesses through the cache to measure the cache's performance on real workloads. The TraceReader class does this by reading one access per line in the format R 0x12345678 or W 0xABCD0000. It skips blank lines and comment lines, normalises lowercase r/w to uppercase, accepts hex addresses with/without a 0x prefix, and reports warnings with a line number if a line is malformed instead of crashing. A second constructor that accepts any std::istream instead of just a filepath was added so me and you can be driven directly from in memory strings during unit tests without needing real files. 

**Update 5:**
Now that my cache and tracereader both work, I used claude to wire them together in main.cpp and add command line configuration. The entry point now accepts a trace file path as well as some optional files so that the same binary can simulate different cache geometries/policies without having to recompile. It prints the resolved configuration before running, processes every line of the trace file using cache.access(), and prints a full statistics report at the end covering the following: total accesses, reads, writes, hits, misses, hit rate, miss rate, evictions, dirty write backs, adn write throughs. The statistics struct was also extended with separate read and write counters and a missRate() method.

**Update 6:**
This update was to support loading cache parameters from a text configuration file. I added a ConfigParser class that does just that, so the user can swap between different cache configurations without having to recompile or type long commands. The format is simple key = value pairs, with one per line, and K/M size suffixes(ex. 32K). There are 7 keys that are supported: name, cache_size, block_size, associativity, replacement, write_policy, and write_miss. If there is an unknown key, a warning is produced but the program doesn't crash. I also added 3 example configs to make common cache architectures easy to produce.

**Update 7:**
I added a formatted report that prints at the end of every simulation run, removing the previous key value dump from the printStats() function. The output shows cache configuration and simulation results side by side, and the formatting helpers live in Cache.cpp's anonymous namespace. Furthermore, I added a gen_traces tool that generates 4 benchmark trace files for comparing different cache configurations. sequential.trace runs two full passes over 512 lines. The first pass is all cold misses while the second is all hits. This demonstrates how a warm cache eliminates misses entirely. random.trace scatters 1024 accesses across a 1MiB window, purposely producing a high miss rate. matrix_row_major.trace and matrix_col_major.trace both traverse a 128x128 int matriz in opposite orders. Row major has a 4 byte stride so 16 ints share each cache line, which leads to a low miss rate, while column major has a 512 byte stride so every access touches a new cache line, which is a very high miss rate. This allows the spatial locality difference to be made measurable.

**Update 8:**
I added additional unit tests. For random replacement, it now verifies that exactly one way is evicted when a set overflows, that the set stays full across many evictions and that  stats stay consistent. For fully associative caches, it verifies that config is accepted, all addresses map to set 0 and no set conflicts are possible. For direct mapped, it verifies that two addresses aliasing to the same set cause an eviction, that non conflicting addresses don't cause an eviction, and that repeated aliasing thrashes. Furthermore, I added a two level L1 + L2 simulation via --l2-config. Each level is an independent Cache with its own config and stats. L2 only sees accesses that L1 missed. I also added fully_associative.conf to document the correct geometry that a fully associative cache has. 
