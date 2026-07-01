#include "IReplacementPolicy.h"

namespace {

//LRU, evicts way that was accessed least recently. CacheLine::lastUsed is updated on every hit and fill
class LRUPolicy final : public IReplacementPolicy {
public:
    void onLoad(CacheSet& set, int way, uint64_t ts) override {
        set.getLine(way).setLastUsed(ts);
    }

    void onAccess(CacheSet& set, int way, uint64_t ts) override {
        set.getLine(way).setLastUsed(ts);
    }

    int selectVictim(const CacheSet& set) const override {
        int victim = 0;
        uint64_t oldest = set.getLine(0).getLastUsed();
        for (int w = 1; w < set.associativity(); ++w) {
            uint64_t t = set.getLine(w).getLastUsed();
            if (t < oldest) { oldest = t; victim = w; }
        }
        return victim;
    }
};

//FIFO, evicts way that was filled earliest. CacheLine::insertionTime is set on fill and never updated on access
class FIFOPolicy final : public IReplacementPolicy {
public:
    void onLoad(CacheSet& set, int way, uint64_t ts) override {
        set.getLine(way).setInsertionTime(ts);
    }

    void onAccess(CacheSet&, int, uint64_t) override {} // no-op

    int selectVictim(const CacheSet& set) const override {
        int victim = 0;
        uint64_t oldest = set.getLine(0).getInsertionTime();
        for (int w = 1; w < set.associativity(); ++w) {
            uint64_t t = set.getLine(w).getInsertionTime();
            if (t < oldest) { oldest = t; victim = w; }
        }
        return victim;
    }
};

//Random, evicts a random way. Uses splitmix64 LCG for speed/reproducibility across platforms
class RandomPolicy final : public IReplacementPolicy {
public:
    void onLoad(CacheSet&, int, uint64_t)   override {} // no-op
    void onAccess(CacheSet&, int, uint64_t) override {} // no-op

    int selectVictim(const CacheSet& set) const override {
        m_state += 0x9e3779b97f4a7c15ULL;
        uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= (z >> 31);
        return static_cast<int>(z % static_cast<uint64_t>(set.associativity()));
    }

private:
    mutable uint64_t m_state = 0xDEADBEEFCAFEBABEULL;
};

} // namespace

//factory

std::unique_ptr<IReplacementPolicy> IReplacementPolicy::create(ReplacementPolicy p) {
    switch (p) {
    case ReplacementPolicy::LRU:    return std::make_unique<LRUPolicy>();
    case ReplacementPolicy::FIFO:   return std::make_unique<FIFOPolicy>();
    case ReplacementPolicy::Random: return std::make_unique<RandomPolicy>();
    }
    return std::make_unique<LRUPolicy>(); // unreachable, satisfies compilers
}
