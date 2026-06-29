#pragma once

#include "CacheSet.h"
#include "Config.h"
#include <vector>
#include <cstdint>
#include <stdexcept>

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

    //returns floor(log2(x)). x must be a positive power of two
    static int  log2i(uint64_t x);
    static bool isPowerOfTwo(uint64_t x);
    static void validateConfig(const CacheConfig& cfg);
};
