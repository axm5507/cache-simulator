#include <gtest/gtest.h>
#include "Cache.h"

// 1 KiB / 64 B / 4-way: numSets=4, stride=256 bytes
static CacheConfig cfg4Way(ReplacementPolicy rp) {
    CacheConfig cfg;
    cfg.name              = "test";
    cfg.capacityBytes     = 1024;
    cfg.blockBytes        = 64;
    cfg.associativity     = 4;
    cfg.replacementPolicy = rp;
    return cfg;
}

// Address mapping to set 0 with the given tag (stride = 4 sets * 64 B = 256 B)
static uint64_t addr(int tag) { return static_cast<uint64_t>(tag) * 256; }

// ---- Random replacement policy ------------------------------------------

TEST(RandomPolicy, EvictsExactlyOneWhenSetIsFull) {
    Cache c(cfg4Way(ReplacementPolicy::Random));
    for (int t = 0; t < 4; ++t)
        c.access(addr(t), false);   // fill all 4 ways

    c.access(addr(4), false);       // trigger eviction

    EXPECT_EQ(c.stats().evictions, 1u);
    EXPECT_NE(c.getSet(0).findTag(c.getTag(addr(4))), -1) << "tag 4 must be loaded";

    int remaining = 0;
    for (int t = 0; t < 4; ++t)
        if (c.getSet(0).findTag(c.getTag(addr(t))) != -1) ++remaining;
    EXPECT_EQ(remaining, 3) << "exactly one of the original 4 lines evicted";
}

TEST(RandomPolicy, StatsConsistentOverManyEvictions) {
    Cache c(cfg4Way(ReplacementPolicy::Random));
    constexpr int N = 50;
    for (int t = 0; t < N; ++t)
        c.access(addr(t), false);
    EXPECT_EQ(c.stats().misses,    static_cast<uint64_t>(N));
    EXPECT_EQ(c.stats().evictions, static_cast<uint64_t>(N - 4));
    EXPECT_EQ(c.stats().hits,      0u);
}

TEST(RandomPolicy, HitWithoutEviction) {
    Cache c(cfg4Way(ReplacementPolicy::Random));
    c.access(addr(0), false);
    bool hit = c.access(addr(0), false);
    EXPECT_TRUE(hit);
    EXPECT_EQ(c.stats().evictions, 0u);
}

TEST(RandomPolicy, SetStaysFullAfterManyEvictions) {
    Cache c(cfg4Way(ReplacementPolicy::Random));
    for (int t = 0; t < 30; ++t)
        c.access(addr(t), false);
    int valid = 0;
    for (int w = 0; w < c.associativity(); ++w)
        if (c.getSet(0).getLine(w).isValid()) ++valid;
    EXPECT_EQ(valid, c.associativity());
}

// ---- Fully-associative cache (assoc = capacity / blockBytes, numSets = 1) --

TEST(FullyAssociative, ConfigIsValid) {
    CacheConfig cfg;
    cfg.capacityBytes = 256;   // 4 lines
    cfg.blockBytes    = 64;
    cfg.associativity = 4;     // numSets = 256/(64*4) = 1 → fully associative
    EXPECT_NO_THROW(Cache c(cfg));
}

TEST(FullyAssociative, ExactlyOnSet) {
    CacheConfig cfg;
    cfg.capacityBytes = 256;
    cfg.blockBytes    = 64;
    cfg.associativity = 4;
    Cache c(cfg);
    EXPECT_EQ(c.numSets(), 1);
}

TEST(FullyAssociative, AllAddressesMapToSetZero) {
    CacheConfig cfg;
    cfg.capacityBytes = 256;
    cfg.blockBytes    = 64;
    cfg.associativity = 4;
    Cache c(cfg);
    EXPECT_EQ(c.getIndex(0x00000000u), 0u);
    EXPECT_EQ(c.getIndex(0x00100000u), 0u);
    EXPECT_EQ(c.getIndex(0xDEADBEEFu), 0u);
}

TEST(FullyAssociative, NoConflictsForDistantAddresses) {
    CacheConfig cfg;
    cfg.capacityBytes = 256;
    cfg.blockBytes    = 64;
    cfg.associativity = 4;
    Cache c(cfg);
    c.access(0x00000000u, false);
    c.access(0x00100000u, false);
    c.access(0x00200000u, false);
    c.access(0x00300000u, false);
    EXPECT_EQ(c.stats().evictions, 0u);  // 4 ways, 4 unique lines → no eviction
    c.access(0x00400000u, false);
    EXPECT_EQ(c.stats().evictions, 1u);  // 5th line evicts one
}

// ---- Direct-mapped cache (assoc = 1) ------------------------------------

TEST(DirectMapped, ConflictCausesEviction) {
    CacheConfig cfg;
    cfg.capacityBytes = 1024; // 16 sets × 1 way × 64 B
    cfg.blockBytes    = 64;
    cfg.associativity = 1;
    Cache c(cfg);
    // stride = 16 * 64 = 1024 → two addresses alias to set 0
    c.access(0u,    false); // set 0, tag 0
    c.access(1024u, false); // set 0, tag 1 → eviction
    EXPECT_EQ(c.stats().evictions, 1u);
    EXPECT_EQ(c.stats().misses,    2u);
}

TEST(DirectMapped, NoEvictionForDifferentSets) {
    CacheConfig cfg;
    cfg.capacityBytes = 1024;
    cfg.blockBytes    = 64;
    cfg.associativity = 1;
    Cache c(cfg);
    for (int s = 0; s < 16; ++s)   // one address per set
        c.access(static_cast<uint64_t>(s) * 64, false);
    EXPECT_EQ(c.stats().evictions, 0u);
    EXPECT_EQ(c.stats().misses,   16u);
}

TEST(DirectMapped, RepeatedConflictThrashes) {
    CacheConfig cfg;
    cfg.capacityBytes = 1024;
    cfg.blockBytes    = 64;
    cfg.associativity = 1;
    Cache c(cfg);
    // Alternate between two aliasing addresses: every access is a miss
    for (int i = 0; i < 10; ++i) {
        c.access(0u,    false);
        c.access(1024u, false);
    }
    EXPECT_EQ(c.stats().hits,      0u);
    EXPECT_EQ(c.stats().misses,   20u);
    EXPECT_EQ(c.stats().evictions,18u); // first load of each not an eviction
}
