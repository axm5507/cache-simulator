#include "Cache.h"
#include <cassert>
#include <sstream>

bool Cache::isPowerOfTwo(uint64_t x) {
    return x > 0 && (x & (x - 1)) == 0;
}

int Cache::log2i(uint64_t x) {
    int n = 0;
    while (x >>= 1) ++n;
    return n;
}

void Cache::validateConfig(const CacheConfig& cfg) {
    auto fail = [&](const std::string& msg) {
        throw std::invalid_argument("[CacheConfig \"" + cfg.name + "\"] " + msg);
    };

    if (cfg.associativity < 1)
        fail("associativity must be >= 1");
    if (!isPowerOfTwo(static_cast<uint64_t>(cfg.blockBytes)) || cfg.blockBytes < 1)
        fail("blockBytes must be a positive power of two");
    if (!isPowerOfTwo(cfg.capacityBytes) || cfg.capacityBytes < 1)
        fail("capacityBytes must be a positive power of two");

    uint64_t minCapacity = static_cast<uint64_t>(cfg.blockBytes) *
                           static_cast<uint64_t>(cfg.associativity);
    if (cfg.capacityBytes < minCapacity)
        fail("capacityBytes must be >= blockBytes * associativity");

    uint64_t numSets = cfg.capacityBytes / minCapacity;
    if (!isPowerOfTwo(numSets))
        fail("derived numSets (capacityBytes / (blockBytes * associativity)) must be a power of two");
}

//constructor
Cache::Cache(const CacheConfig& cfg)
    : m_cfg(cfg)
{
    validateConfig(cfg);

    m_numSets    = static_cast<int>(cfg.capacityBytes /
                   (static_cast<uint64_t>(cfg.blockBytes) * cfg.associativity));
    m_offsetBits = log2i(static_cast<uint64_t>(cfg.blockBytes));
    m_indexBits  = log2i(static_cast<uint64_t>(m_numSets));
    m_tagBits    = 64 - m_offsetBits - m_indexBits;

    m_sets.reserve(m_numSets);
    for (int i = 0; i < m_numSets; ++i)
        m_sets.emplace_back(cfg.associativity);
}

//address decomposition
uint64_t Cache::getOffset(uint64_t addr) const {
    //mask the lower offsetBits bits
    return addr & ((1ULL << m_offsetBits) - 1);
}

uint64_t Cache::getIndex(uint64_t addr) const {
    //shift past the offset, then mask indexBits bits
    return (addr >> m_offsetBits) & ((1ULL << m_indexBits) - 1);
}

uint64_t Cache::getTag(uint64_t addr) const {
    //shift past the offset and index bits
    return addr >> (m_offsetBits + m_indexBits);
}

//structural accessors
int Cache::numSets()       const { return m_numSets; }
int Cache::associativity() const { return m_cfg.associativity; }
int Cache::blockBytes()    const { return m_cfg.blockBytes; }
int Cache::offsetBits()    const { return m_offsetBits; }
int Cache::indexBits()     const { return m_indexBits; }
int Cache::tagBits()       const { return m_tagBits; }

const CacheConfig& Cache::config() const { return m_cfg; }

const CacheSet& Cache::getSet(int index) const {
    assert(index >= 0 && index < m_numSets);
    return m_sets[index];
}
