#pragma once

#include "CacheLine.h"
#include <vector>
#include <cstdint>


//The cache divides its line count evenly across all sets, each set holds exactly 'associativity' lines
class CacheSet {
public:
    // Constructs a set with `associativity` invalid lines
    explicit CacheSet(int associativity);

    int associativity() const;

    // Returns the way index of the line matching tag, or -1 on a miss
    int findTag(uint64_t tag) const;

    // Returns the index of the first invalid way, or -1 if all ways are valid
    int findEmptyWay() const;

    // Returns true if at least one way is invalid
    bool hasEmptyWay() const;

    // Accessor for a specific way. Bounds-checked in debug builds via assert.
    CacheLine&       getLine(int way);
    const CacheLine& getLine(int way) const;

private:
    int                    m_associativity;
    std::vector<CacheLine> m_lines;
};
