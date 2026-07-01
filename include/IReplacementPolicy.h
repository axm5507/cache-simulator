#pragma once

#include "CacheSet.h"
#include "Config.h"
#include <cstdint>
#include <memory>

// Abstract interface for cache replacement policies.
//
// The Cache calls exactly three hooks during simulation:
//
//   onLoad(set, way, ts)   — a new block was just installed in `way`
//   onAccess(set, way, ts) — an existing line in `way` was hit (read or write)
//   selectVictim(set)      — all ways are full; return the way to evict
//
// Concrete implementations are hidden inside IReplacementPolicy.cpp.
// Use IReplacementPolicy::create() to obtain a policy by enum value.
class IReplacementPolicy {
public:
    virtual ~IReplacementPolicy() = default;

    // Called when a block is loaded into `way` at logical time `timestamp`.
    // Implementations update whichever CacheLine field they track
    // (e.g. insertionTime for FIFO, lastUsed for LRU).
    virtual void onLoad(CacheSet& set, int way, uint64_t timestamp) = 0;

    // Called on a cache hit in `way` at logical time `timestamp`.
    // LRU refreshes lastUsed; FIFO and Random are no-ops.
    virtual void onAccess(CacheSet& set, int way, uint64_t timestamp) = 0;

    // Return the way index to evict. Precondition: all ways are valid.
    virtual int selectVictim(const CacheSet& set) const = 0;

    // Factory — constructs the correct concrete policy for `p`.
    static std::unique_ptr<IReplacementPolicy> create(ReplacementPolicy p);
};
