#pragma once

#include "CacheSet.h"
#include "Config.h"
#include "IReplacementPolicy.h"
#include <iosfwd>
#include <memory>
#include <vector>
#include <cstdint>
#include <stdexcept>

//snapshot of simulation counters, returned by Cache::stats()
struct CacheStats {
    uint64_t reads = 0;  //total read accesses
    uint64_t writes = 0; //total write accesses
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    uint64_t writeBacks = 0;  //dirty lines flushed to memory on eviction
    uint64_t writeThroughs = 0; //writes propagated to memory immediately

    uint64_t totalAccesses() const { return reads + writes; }

    //returns the hit fraction [0,1] of all accesses; 0 when no accesses yet
    double hitRate()  const;
    double missRate() const;
};

//models a single level of a CPU cache hierarchy
class Cache {
public:
    explicit Cache(const CacheConfig& cfg);

    //address decomposition

    //byte offset within the cache line
    uint64_t getOffset(uint64_t addr) const;

    //set index used to locate the right CacheSet
    uint64_t getIndex(uint64_t addr) const;

    //tag stored in the matching CacheLine
    uint64_t getTag(uint64_t addr) const;

    //simulation

    //simulates one memory access; returns true on hit, false on miss
    bool access(uint64_t address, bool isWrite);

    //statistics
    const CacheStats& stats() const;
    void resetStats();
    void printStats() const;
    void printStats(std::ostream& out) const;

    //structural accessors
    int numSets()       const;
    int associativity() const;
    int blockBytes()    const;
    int offsetBits()    const;
    int indexBits()     const;
    int tagBits()       const;

    const CacheConfig& config() const;

    const CacheSet& getSet(int index) const;

private:
    CacheConfig m_cfg;
    std::vector<CacheSet>  m_sets;
    std::unique_ptr<IReplacementPolicy> m_policy; //version 3 update: replacement policy is now a separate class

    int m_numSets;
    int m_offsetBits;
    int m_indexBits;
    int m_tagBits;

    uint64_t   m_clock; //logical clock; incremented on each access
    CacheStats m_stats;

    //returns floor(log2(x)). x must be a positive power of two
    static int  log2i(uint64_t x);
    static bool isPowerOfTwo(uint64_t x);
    static void validateConfig(const CacheConfig& cfg);
};
