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
|   └── IReplacementPolicy.h # Methods cache calls during simulation
├── src/                     # Implementation files + main entry point
├── configs/                 # Example JSON/INI cache configuration files
├── traces/                  # Memory access trace files
├── benchmarks/              # Benchmark scripts and workloads
├── results/                 # Simulation output files
└── tests/                   # Unit and integration tests
```

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
