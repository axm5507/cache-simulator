#include "Cache.h"
#include <cassert>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>


namespace {

constexpr int kLabelWidth = 20;

std::string fmtInt(uint64_t n) {
    std::string s = std::to_string(n);
    int i = static_cast<int>(s.size()) - 3;
    while (i > 0) { s.insert(static_cast<size_t>(i), ","); i -= 3; }
    return s;
}

std::string fmtPct(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%%", v * 100.0);
    return buf;
}

std::string fmtBytes(uint64_t bytes) {
    char buf[32];
    if      (bytes >= 1024ULL * 1024) std::snprintf(buf, sizeof(buf), "%g MiB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)           std::snprintf(buf, sizeof(buf), "%g KiB", bytes / 1024.0);
    else                              return std::to_string(bytes) + " B";
    return buf;
}

std::string policyName(ReplacementPolicy p) {
    switch (p) {
        case ReplacementPolicy::LRU:    return "LRU";
        case ReplacementPolicy::FIFO:   return "FIFO";
        case ReplacementPolicy::Random: return "Random";
    }
    return "?";
}

void reportRow(std::ostream& out, const std::string& label, const std::string& value) {
    out << std::left << std::setw(kLabelWidth) << (label + ":") << value << "\n";
}

} // namespace

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

void Cache::printStats(std::ostream& out) const {
    const std::string border(41, '=');
    const std::string wp  = m_cfg.writePolicy    == WritePolicy::WriteBack
                          ? "write-back" : "write-through";
    const std::string wmp = m_cfg.writeMissPolicy == WriteMissPolicy::WriteAllocate
                          ? "write-allocate" : "no-write-allocate";

    out << border << "\n"
        << " Cache Simulation Report: " << m_cfg.name << "\n"
        << border << "\n\n";

    out << " Configuration\n";
    reportRow(out, "  Size",          fmtBytes(m_cfg.capacityBytes));
    reportRow(out, "  Block Size",    std::to_string(m_cfg.blockBytes) + " B");
    reportRow(out, "  Associativity", std::to_string(m_cfg.associativity) + "-way");
    reportRow(out, "  Sets",          std::to_string(m_numSets));
    reportRow(out, "  Replacement",   policyName(m_cfg.replacementPolicy));
    reportRow(out, "  Write Policy",  wp);
    reportRow(out, "  Write Miss",    wmp);
    out << "\n";

    out << " Simulation Results\n";
    reportRow(out, "  Total Accesses", fmtInt(m_stats.totalAccesses()));
    reportRow(out, "  Reads",          fmtInt(m_stats.reads));
    reportRow(out, "  Writes",         fmtInt(m_stats.writes));
    out << "\n";
    reportRow(out, "  Hits",           fmtInt(m_stats.hits));
    reportRow(out, "  Misses",         fmtInt(m_stats.misses));
    out << "\n";
    reportRow(out, "  Hit Rate",       fmtPct(m_stats.hitRate()));
    reportRow(out, "  Miss Rate",      fmtPct(m_stats.missRate()));
    out << "\n";
    reportRow(out, "  Evictions",      fmtInt(m_stats.evictions));
    reportRow(out, "  Write-backs",    fmtInt(m_stats.writeBacks));
    reportRow(out, "  Write-throughs", fmtInt(m_stats.writeThroughs));
    out << "\n" << border << "\n";
}

void Cache::printStats() const { printStats(std::cout); }

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
