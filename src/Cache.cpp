#include "Cache.h"
#include <cassert>
#include <iostream>
#include <iomanip>


double CacheStats::hitRate() const {
    uint64_t total = hits + misses;
    return total ? (static_cast<double>(hits) / static_cast<double>(total)) : 0.0;
}

double CacheStats::missRate() const {
    uint64_t total = hits + misses;
    return total ? (static_cast<double>(misses) / static_cast<double>(total)) : 0.0;
}


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

//constructs cache
Cache::Cache(const CacheConfig& cfg)
    : m_cfg(cfg)
    , m_policy(IReplacementPolicy::create(cfg.replacementPolicy))
    , m_clock(0)
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



//simulation
bool Cache::access(uint64_t address, bool isWrite) {
    if (isWrite) m_stats.writes++; else m_stats.reads++;

    const uint64_t idx = getIndex(address);
    const uint64_t tag = getTag(address);
    CacheSet& set = m_sets[static_cast<size_t>(idx)];

    const int wayHit = set.findTag(tag);

    if (wayHit != -1) {
        //hit
        m_stats.hits++;
        m_policy->onAccess(set, wayHit, m_clock++);

        if (isWrite) {
            if (m_cfg.writePolicy == WritePolicy::WriteBack) {
                //mark dirty; flushed to memory on eviction
                set.getLine(wayHit).setDirty(true);
            } else {
                //propagate every write to memory immediately
                m_stats.writeThroughs++;
            }
        }
        return true;
    }

    //miss
    m_stats.misses++;

    //on a write miss with NoWriteAllocate, skip cache allocation
    if (isWrite && m_cfg.writeMissPolicy == WriteMissPolicy::NoWriteAllocate) {
        if (m_cfg.writePolicy == WritePolicy::WriteThrough)
            m_stats.writeThroughs++;
        return false;
    }

    //find an empty way, or select and evict a victim
    int way = set.findEmptyWay();
    if (way == -1) {
        way = m_policy->selectVictim(set);
        CacheLine& victim = set.getLine(way);
        //a dirty victim must be written back to main memory before reuse
        if (victim.isDirty())
            m_stats.writeBacks++;
        m_stats.evictions++;
        victim.reset();
    }

    //fill the selected way with the incoming block
    CacheLine& line = set.getLine(way);
    line.setValid(true);
    line.setTag(tag);
    m_policy->onLoad(set, way, m_clock++);

    if (isWrite) {
        if (m_cfg.writePolicy == WritePolicy::WriteBack) {
            line.setDirty(true);
        } else {
            //write to memory at fill time too
            m_stats.writeThroughs++;
        }
    }

    return false;
}

//statistics
const CacheStats& Cache::stats() const { return m_stats; }

void Cache::resetStats() { m_stats = CacheStats{}; }

void Cache::printStats() const {
    uint64_t total = m_stats.totalAccesses();
    std::cout << "[" << m_cfg.name << "]\n"
              << "  total accesses    : " << total              << "\n"
              << "  reads             : " << m_stats.reads      << "\n"
              << "  writes            : " << m_stats.writes     << "\n"
              << "  hits              : " << m_stats.hits       << "\n"
              << "  misses            : " << m_stats.misses     << "\n"
              << "  hit rate          : " << std::fixed << std::setprecision(2)
              << (m_stats.hitRate()  * 100.0) << "%\n"
              << "  miss rate         : "
              << (m_stats.missRate() * 100.0) << "%\n"
              << "  evictions         : " << m_stats.evictions         << "\n"
              << "  dirty write-backs : " << m_stats.writeBacks        << "\n"
              << "  write-throughs    : " << m_stats.writeThroughs     << "\n";
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
    return m_sets[static_cast<size_t>(index)];
}
