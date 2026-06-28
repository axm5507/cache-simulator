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
