#pragma once

#include <string>
#include <cstdint>

enum class ReplacementPolicy {
    LRU,    // Least Recently Used
    FIFO,   // First In, First Out
    Random, // Random eviction
};

//what happens when writing to an address already in cache
enum class WritePolicy {
    WriteBack,    // Update cache only
    WriteThrough, // Update cache and main memory
};

//what happens when writing to an address not in cache
enum class WriteMissPolicy {
    WriteAllocate,   // Load the missing line into cache, then write
    NoWriteAllocate, // Write directly to memory
};

//all parameters that define a single cache level
struct CacheConfig {
    std::string name          = "L1";
    uint64_t    capacityBytes = 32 * 1024; // 32 KiB
    int         blockBytes    = 64;        // 64-byte cache lines
    int         associativity = 4;         // 4-way set-associative

    ReplacementPolicy replacementPolicy = ReplacementPolicy::LRU;
    WritePolicy       writePolicy       = WritePolicy::WriteBack;
    WriteMissPolicy   writeMissPolicy   = WriteMissPolicy::WriteAllocate;
};
