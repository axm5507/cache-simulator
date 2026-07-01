#pragma once

#include "CacheSet.h"
#include "Config.h"
#include <cstdint>
#include <memory>
//This class is a version 3 addition. It was not in the original project structure.
//However, I found it necessary to add it to implement the replacement policies cleanly
//and seperate them from the cache itself


//During simulation, the cache calls 3 hooks: onLoad, onAccess, and selectVictim. 
class IReplacementPolicy {
public:
    virtual ~IReplacementPolicy() = default;

    //called when a block is loaded into `way` at logical time `timestamp`
    //implementations update whichever CacheLine field they track
    virtual void onLoad(CacheSet& set, int way, uint64_t timestamp) = 0;

    //called on a cache hit in `way` at logical time `timestamp`
    virtual void onAccess(CacheSet& set, int way, uint64_t timestamp) = 0;

    //return the way index to evict
    virtual int selectVictim(const CacheSet& set) const = 0;

    //constructs the correct concrete policy for `p`
    static std::unique_ptr<IReplacementPolicy> create(ReplacementPolicy p);
};
