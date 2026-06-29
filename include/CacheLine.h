#pragma once

#include <cstdint>

// A cache line is a single unit of storage in a cache set, holding a block of memory from main memory
class CacheLine {
public:
    // Constructs an invalid, clean line
    CacheLine();

    // Resets the line to its initial state
    void reset();

    bool isValid() const; // returns true if the line holds a valid address mapping
    bool isDirty() const; // returns true if the line has been written to 
    uint64_t getTag() const; // the tag is the upper bits of the cached address
    uint64_t getLastUsed() const; // LRU
    uint64_t getInsertionTime() const; // FIFO

    void setValid(bool valid);
    void setDirty(bool dirty);
    void setTag(uint64_t tag);
    void setLastUsed(uint64_t timestamp);
    void setInsertionTime(uint64_t timestamp);

private:
    bool m_valid;
    bool m_dirty;
    uint64_t m_tag;
    uint64_t m_lastUsed;
    uint64_t m_insertionTime;
};
