# Cache Simulator

This project is a CPU cache simulator written in C++17.

## Overview

This simulator models how a CPU cache behaves when replaying memory access traces. It will support configurable cache geometry, replacement policies, and write policies.

## Project Structure

```
cache-simulator/
├── include/          # Public headers
│   ├── Cache.h       # Single cache level
│   ├── CacheSet.h    # One set
│   ├── CacheLine.h   # One cache line
│   ├── TraceReader.h # Streaming trace file parser
│   └── Config.h      # Configuration structs and policy enums
├── src/              # Implementation files + main entry point
├── configs/          # Example JSON/INI cache configuration files
├── traces/           # Memory access trace files
├── benchmarks/       # Benchmark scripts and workloads
├── results/          # Simulation output files
└── tests/            # Unit and integration tests
```

## Progress Updates

**Update 1:** 
I created a CacheLine class that contains: valid bit, dirty bit, tag, last used timestamp, and insertion timestamp. I then implemented a CacheSet class that stores multiple CacheLines, knows its associativity, can search for matching tags, determine whether an empty line exists, and returns indices of matching lines. Then, I implemented a Cache class that stores cache size, block size, associativity, number of sets, and a vector of CacheSets. It calculates number of offset bits, number of index bits, and tag extraction.

**Update 2:**
I added to the Cache class so it now decomposes any 64 bit address into 3 bit fields: block offset, set index, and tag. The widths of these fields are automatically derived from the cache geometry at construction time. Unit tests verify the correctness for hex addresses and confirm that all reconstructing is lossless. Furthermore, I added Cache::access() which implements the cache reads. It extracts the set index and tag, searches the set for a matching valid line, and returns hit/miss. On a hit the LRU timestamp of the line is refreshed, and on a miss either the block is loaded into an empty way or another line is chosen and evicted first. Three replacement policies are supported, which are the standard LRU(evict least recently used), FIFO(evict first inserted), and Random(splitmix64). The tests verify everything works as intended. Finally, write behavior is controlled by two write policies: write back and write through. Write back marks a line dirty and defers the memory write until eviction, while write through adds every write to memory immediately and never sets the dirty bit. The write miss policy determines whether a missing line is fetched into the cache before writing. 
