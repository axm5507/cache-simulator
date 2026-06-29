# Cache Simulator

This project is a CPU cache simulator written in C++17.

## Overview

This simulator models how a CPU cache behaves when replaying memory access traces. It will supports configurable cache geometry, replacement policies, and write policies.

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