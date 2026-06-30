#pragma once

#include "CacheSet.h"
#include "Config.h"
#include <vector>
#include <cstdint>
#include <stdexcept>

//snapshot of simulation counters, Returned by Cache::stats()
struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    //dirty lines flushed to memory when evicted
    uint64_t writeBacks = 0;
    //writes propagated to memory immediately
    uint64_t writeThroughs = 0;

    //returns the hit fraction [0,1] of all accesses
    double hitRate() const;
};

//models a single level of a CPU cache hierarchy

class Cache {
public:
    explicit Cache(const CacheConfig& cfg);
    //Address decomposition methods

    //byte offset within the cache line
    uint64_t getOffset(uint64_t addr) const;

    //set index used to locate the right CacheSet
    uint64_t getIndex(uint64_t addr) const;

    //tag stored in the matching CacheLine
    uint64_t getTag(uint64_t addr) const;


    //simulates one memory access
    //returns true on hit, false on miss
    bool access(uint64_t address, bool isWrite);

    const CacheStats& stats() const;
    void resetStats();
    void printStats() const;

    int numSets() const;
    int associativity() const;
    int blockBytes() const;
    int offsetBits() const;
    int indexBits() const;
    int tagBits() const;

    const CacheConfig& config() const;

    const CacheSet& getSet(int index) const;

private:
    CacheConfig            m_cfg;
    std::vector<CacheSet>  m_sets;

    int m_numSets;
    int m_offsetBits;
    int m_indexBits;
    int m_tagBits;

    uint64_t m_clock; //logical clock
    CacheStats m_stats;
    mutable uint64_t m_rngState; //splitmix64 state for Random replacement policy

    //selects which way to evict; all ways in set must be valid
    int selectEvictionWay(const CacheSet& set) const;

    //returns floor(log2(x)). x must be a positive power of two
    static int  log2i(uint64_t x);
    static bool isPowerOfTwo(uint64_t x);
    static void validateConfig(const CacheConfig& cfg);
};