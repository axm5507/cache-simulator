#include <gtest/gtest.h>
#include "Cache.h"

// ============================================================
// Test fixture
//
// Small 1 KiB, 64-byte-block, 4-way LRU WriteBack cache:
//   numSets    = 1024 / (64 * 4) = 4
//   offsetBits = 6, indexBits = 2
//   stride     = numSets * blockBytes = 4 * 64 = 256
//
// Addresses that map to set 0 with sequential tags 0, 1, 2, ...:
//   addrTag(n) = n * 256
//
// Addresses that map to set 1 with tag 0: 64
// ============================================================

static CacheConfig smallConfig(
    ReplacementPolicy rp  = ReplacementPolicy::LRU,
    WritePolicy       wp  = WritePolicy::WriteBack,
    WriteMissPolicy   wmp = WriteMissPolicy::WriteAllocate)
{
    CacheConfig cfg;
    cfg.name             = "test";
    cfg.capacityBytes    = 1024;
    cfg.blockBytes       = 64;
    cfg.associativity    = 4;
    cfg.replacementPolicy = rp;
    cfg.writePolicy       = wp;
    cfg.writeMissPolicy   = wmp;
    return cfg;
}

// Returns an address mapping to set 0 with the given tag.
static uint64_t addrTag(int tag) {
    // stride = 4 sets * 64 bytes = 256
    return static_cast<uint64_t>(tag) * 256;
}

// ============================================================
// Step 5 verification — run decomposition on specific hex addresses
// directly via the simulation fixture to confirm tag/index/offset
// are used correctly during lookup.
// ============================================================

TEST(SimAddressLookup, AccessUsesCorrectSetAndTag) {
    Cache c(smallConfig());
    // 0x12345678: with this config (offsetBits=6, indexBits=2)
    //   offset = 0x12345678 & 0x3F  = 56
    //   index  = (0x12345678 >> 6) & 0x3 = (4772185) & 3 = 1
    //   tag    = 0x12345678 >> 8 = 1193046
    constexpr uint64_t addr = 0x12345678u;
    EXPECT_EQ(c.getIndex(addr), 1u);
    c.access(addr, false);
    EXPECT_EQ(c.stats().misses, 1u);
    // Second access to the same address hits.
    c.access(addr, false);
    EXPECT_EQ(c.stats().hits, 1u);
    // The loaded line is in set 1.
    EXPECT_NE(c.getSet(1).findTag(c.getTag(addr)), -1);
}

// ============================================================
// Step 6 — Read operations
// ============================================================

TEST(ReadOps, ColdCacheReadIsMiss) {
    Cache c(smallConfig());
    bool hit = c.access(addrTag(0), false);
    EXPECT_FALSE(hit);
    EXPECT_EQ(c.stats().misses, 1u);
    EXPECT_EQ(c.stats().hits,   0u);
}

TEST(ReadOps, SecondReadToSameAddressIsHit) {
    Cache c(smallConfig());
    c.access(addrTag(0), false);
    bool hit = c.access(addrTag(0), false);
    EXPECT_TRUE(hit);
    EXPECT_EQ(c.stats().hits,   1u);
    EXPECT_EQ(c.stats().misses, 1u);
}

TEST(ReadOps, LineLoadedIntoCorrectSet) {
    Cache c(smallConfig());
    c.access(addrTag(0), false);
    // addrTag(0) = 0, maps to set 0, tag 0.
    EXPECT_NE(c.getSet(0).findTag(0), -1);
}

TEST(ReadOps, DifferentSetsAreIndependent) {
    Cache c(smallConfig());
    c.access(addrTag(0), false); // set 0, tag 0
    c.access(64,         false); // set 1, tag 0
    // Both are misses; each appears only in its own set.
    EXPECT_EQ(c.stats().misses, 2u);
    EXPECT_NE(c.getSet(0).findTag(0), -1);
    EXPECT_NE(c.getSet(1).findTag(0), -1);
    // Set 0 does not contain a line that was loaded for set 1.
    EXPECT_EQ(c.getSet(0).findTag(c.getTag(64)), -1);
}

TEST(ReadOps, FillAllFourWays) {
    Cache c(smallConfig());
    for (int t = 0; t < 4; ++t)
        c.access(addrTag(t), false);
    EXPECT_EQ(c.stats().misses, 4u);
    for (int t = 0; t < 4; ++t)
        EXPECT_TRUE(c.access(addrTag(t), false)) << "tag=" << t;
    EXPECT_EQ(c.stats().hits, 4u);
    EXPECT_EQ(c.stats().evictions, 0u);
}

TEST(ReadOps, ReadTagNotPresentInOtherSet) {
    Cache c(smallConfig());
    c.access(addrTag(0), false);
    // Tag 0 should not appear in set 1.
    EXPECT_EQ(c.getSet(1).findTag(0), -1);
}

// ---- LRU eviction -----------------------------------------------------------

TEST(ReadOps_LRU, EvictsLeastRecentlyUsedWay) {
    Cache c(smallConfig(ReplacementPolicy::LRU));
    // Load tags 0–3 into set 0 (misses in order 0,1,2,3).
    for (int t = 0; t < 4; ++t)
        c.access(addrTag(t), false);
    // Re-access tag 0 → it becomes the MRU; tag 1 is now the LRU.
    c.access(addrTag(0), false);
    // Load tag 4 → must evict tag 1 (LRU).
    c.access(addrTag(4), false);

    EXPECT_EQ(c.stats().evictions, 1u);
    EXPECT_EQ(c.getSet(0).findTag(c.getTag(addrTag(1))), -1) << "tag 1 should be evicted";
    EXPECT_NE(c.getSet(0).findTag(c.getTag(addrTag(0))), -1) << "tag 0 must survive";
    EXPECT_NE(c.getSet(0).findTag(c.getTag(addrTag(4))), -1) << "tag 4 must be loaded";
}

TEST(ReadOps_LRU, MRULineNotEvictedFirst) {
    Cache c(smallConfig(ReplacementPolicy::LRU));
    for (int t = 0; t < 4; ++t)
        c.access(addrTag(t), false);
    // Keep accessing tag 3 to make it perpetually MRU.
    c.access(addrTag(3), false);
    c.access(addrTag(4), false); // evicts tag 0 (LRU)
    EXPECT_NE(c.getSet(0).findTag(c.getTag(addrTag(3))), -1);
}

// ---- FIFO eviction ----------------------------------------------------------

TEST(ReadOps_FIFO, EvictsFirstInsertedWay) {
    Cache c(smallConfig(ReplacementPolicy::FIFO));
    // Load tags 0–3 in order: insertionTime 0 < 1 < 2 < 3.
    for (int t = 0; t < 4; ++t)
        c.access(addrTag(t), false);
    // Re-access tag 0 — FIFO does NOT refresh insertionTime, only lastUsed.
    c.access(addrTag(0), false); // hit, insertionTime of tag 0 stays at 0
    // Load tag 4 → FIFO evicts tag 0 (insertionTime = 0, the oldest).
    c.access(addrTag(4), false);

    EXPECT_EQ(c.stats().evictions, 1u);
    EXPECT_EQ(c.getSet(0).findTag(c.getTag(addrTag(0))), -1) << "tag 0 should be evicted by FIFO";
    EXPECT_NE(c.getSet(0).findTag(c.getTag(addrTag(4))), -1) << "tag 4 must be loaded";
}

TEST(ReadOps_FIFO, FIFODiffersFromLRUAfterReuse) {
    // If LRU were in use, tag 1 would be evicted (because tag 0 was re-accessed).
    // Under FIFO, tag 0 is still evicted (insertion order is unchanged by reads).
    Cache lru(smallConfig(ReplacementPolicy::LRU));
    Cache fifo(smallConfig(ReplacementPolicy::FIFO));

    for (auto* c : {&lru, &fifo}) {
        for (int t = 0; t < 4; ++t) c->access(addrTag(t), false);
        c->access(addrTag(0), false); // re-access tag 0
        c->access(addrTag(4), false); // trigger eviction
    }

    // LRU evicts tag 1 (least recently used after the re-access of tag 0).
    EXPECT_EQ(lru.getSet(0).findTag(lru.getTag(addrTag(1))), -1);

    // FIFO evicts tag 0 (first inserted, regardless of re-access).
    EXPECT_EQ(fifo.getSet(0).findTag(fifo.getTag(addrTag(0))), -1);
}

// ============================================================
// Step 7 — Write operations
// ============================================================

// ---- Write-back, write-hit --------------------------------------------------

TEST(WriteOps_WriteBack, WriteHitMarksDirty) {
    Cache c(smallConfig());
    c.access(addrTag(0), false); // read miss — loads clean
    c.access(addrTag(0), true);  // write hit — should mark dirty
    int way = c.getSet(0).findTag(c.getTag(addrTag(0)));
    ASSERT_NE(way, -1);
    EXPECT_TRUE(c.getSet(0).getLine(way).isDirty());
}

TEST(WriteOps_WriteBack, ReadHitDoesNotMarkDirty) {
    Cache c(smallConfig());
    c.access(addrTag(0), false); // miss
    c.access(addrTag(0), false); // hit (read)
    int way = c.getSet(0).findTag(c.getTag(addrTag(0)));
    ASSERT_NE(way, -1);
    EXPECT_FALSE(c.getSet(0).getLine(way).isDirty());
}

TEST(WriteOps_WriteBack, MultipleWriteHitsKeepLineDirty) {
    Cache c(smallConfig());
    c.access(addrTag(0), false);
    c.access(addrTag(0), true);
    c.access(addrTag(0), true); // second write hit
    int way = c.getSet(0).findTag(c.getTag(addrTag(0)));
    ASSERT_NE(way, -1);
    EXPECT_TRUE(c.getSet(0).getLine(way).isDirty());
    EXPECT_EQ(c.stats().writeBacks, 0u); // no eviction yet
}

// ---- Write-back, eviction ---------------------------------------------------

// Use a direct-mapped cache so eviction is deterministic.
// numSets = 1024/(64*1) = 16, stride = 16*64 = 1024
static CacheConfig directConfig(WritePolicy wp = WritePolicy::WriteBack,
                                WriteMissPolicy wmp = WriteMissPolicy::WriteAllocate) {
    CacheConfig cfg;
    cfg.name             = "dm";
    cfg.capacityBytes    = 1024;
    cfg.blockBytes       = 64;
    cfg.associativity    = 1; // direct-mapped
    cfg.replacementPolicy = ReplacementPolicy::LRU; // doesn't matter for 1-way
    cfg.writePolicy       = wp;
    cfg.writeMissPolicy   = wmp;
    return cfg;
}

static uint64_t dmAddr(int tag) { return static_cast<uint64_t>(tag) * 1024; }

TEST(WriteOps_WriteBack, EvictingDirtyLineCountsWriteBack) {
    Cache c(directConfig());
    c.access(dmAddr(0), false); // miss, loads clean
    c.access(dmAddr(0), true);  // hit,  marks dirty
    c.access(dmAddr(1), false); // miss, evicts dmAddr(0) which is dirty → writeback
    EXPECT_EQ(c.stats().writeBacks, 1u);
    EXPECT_EQ(c.stats().evictions,  1u);
}

TEST(WriteOps_WriteBack, EvictingCleanLineNoWriteBack) {
    Cache c(directConfig());
    c.access(dmAddr(0), false); // miss, loads clean (no write)
    c.access(dmAddr(1), false); // miss, evicts dmAddr(0) which is clean
    EXPECT_EQ(c.stats().writeBacks, 0u);
    EXPECT_EQ(c.stats().evictions,  1u);
}

TEST(WriteOps_WriteBack, DirtyBitClearedAfterEviction) {
    Cache c(directConfig());
    c.access(dmAddr(0), false);
    c.access(dmAddr(0), true);  // dirty
    c.access(dmAddr(1), false); // evicts dmAddr(0), loads dmAddr(1) clean
    // The way now holds dmAddr(1), which must be clean.
    int way = c.getSet(0).findTag(c.getTag(dmAddr(1)));
    ASSERT_NE(way, -1);
    EXPECT_FALSE(c.getSet(0).getLine(way).isDirty());
}

// ---- WriteAllocate write-miss -----------------------------------------------

TEST(WriteOps_WriteAllocate, WriteMissLoadsLineIntoCache) {
    Cache c(smallConfig());
    // Write to an address not yet in cache — WriteAllocate should bring it in.
    bool hit = c.access(addrTag(0), true);
    EXPECT_FALSE(hit); // was a miss
    EXPECT_EQ(c.stats().misses, 1u);
    // The line is now in cache and dirty.
    int way = c.getSet(0).findTag(c.getTag(addrTag(0)));
    EXPECT_NE(way, -1);
    EXPECT_TRUE(c.getSet(0).getLine(way).isDirty());
}

TEST(WriteOps_WriteAllocate, SubsequentReadAfterWriteMissIsHit) {
    Cache c(smallConfig());
    c.access(addrTag(0), true);  // write miss → allocate
    bool hit = c.access(addrTag(0), false); // read → should be a hit
    EXPECT_TRUE(hit);
}

// ---- Write-through ----------------------------------------------------------

TEST(WriteOps_WriteThrough, WriteHitIncrementsWriteThroughs) {
    Cache c(smallConfig(ReplacementPolicy::LRU,
                        WritePolicy::WriteThrough,
                        WriteMissPolicy::WriteAllocate));
    c.access(addrTag(0), false); // read miss, load clean
    c.access(addrTag(0), true);  // write hit → write-through to memory
    EXPECT_EQ(c.stats().writeThroughs, 1u);
    EXPECT_EQ(c.stats().writeBacks,    0u);
}

TEST(WriteOps_WriteThrough, WriteHitDoesNotMarkDirty) {
    Cache c(smallConfig(ReplacementPolicy::LRU,
                        WritePolicy::WriteThrough,
                        WriteMissPolicy::WriteAllocate));
    c.access(addrTag(0), false);
    c.access(addrTag(0), true);
    int way = c.getSet(0).findTag(c.getTag(addrTag(0)));
    ASSERT_NE(way, -1);
    EXPECT_FALSE(c.getSet(0).getLine(way).isDirty());
}

TEST(WriteOps_WriteThrough, EvictingWriteThroughLineNoWriteBack) {
    Cache c(directConfig(WritePolicy::WriteThrough, WriteMissPolicy::WriteAllocate));
    c.access(dmAddr(0), false);
    c.access(dmAddr(0), true);  // write-through: data already in memory
    c.access(dmAddr(1), false); // evicts dmAddr(0) — it is NOT dirty → no writeback
    EXPECT_EQ(c.stats().writeBacks, 0u);
    EXPECT_GT(c.stats().writeThroughs, 0u);
}

// ---- NoWriteAllocate --------------------------------------------------------

TEST(WriteOps_NoWriteAllocate, WriteMissDoesNotPopulateCache) {
    Cache c(smallConfig(ReplacementPolicy::LRU,
                        WritePolicy::WriteBack,
                        WriteMissPolicy::NoWriteAllocate));
    c.access(addrTag(0), true); // write miss — should bypass cache
    EXPECT_EQ(c.stats().misses, 1u);
    // The line must NOT be in cache.
    EXPECT_EQ(c.getSet(0).findTag(c.getTag(addrTag(0))), -1);
}

TEST(WriteOps_NoWriteAllocate, SubsequentReadAfterBypassedWriteIsMiss) {
    Cache c(smallConfig(ReplacementPolicy::LRU,
                        WritePolicy::WriteBack,
                        WriteMissPolicy::NoWriteAllocate));
    c.access(addrTag(0), true);  // write miss — bypass
    bool hit = c.access(addrTag(0), false); // read — cold miss again
    EXPECT_FALSE(hit);
    EXPECT_EQ(c.stats().misses, 2u);
}

TEST(WriteOps_NoWriteAllocate, WriteThroughMissCountsWriteThrough) {
    Cache c(smallConfig(ReplacementPolicy::LRU,
                        WritePolicy::WriteThrough,
                        WriteMissPolicy::NoWriteAllocate));
    c.access(addrTag(0), true); // write miss, no-allocate, write-through
    EXPECT_EQ(c.stats().writeThroughs, 1u);
    EXPECT_EQ(c.getSet(0).findTag(c.getTag(addrTag(0))), -1);
}

// ---- Stats and reset --------------------------------------------------------

TEST(StatsOps, ResetStatsClearsCounters) {
    Cache c(smallConfig());
    for (int t = 0; t < 4; ++t) c.access(addrTag(t), false);
    c.access(addrTag(0), false); // hit
    EXPECT_GT(c.stats().hits,   0u);
    EXPECT_GT(c.stats().misses, 0u);
    c.resetStats();
    EXPECT_EQ(c.stats().hits,        0u);
    EXPECT_EQ(c.stats().misses,      0u);
    EXPECT_EQ(c.stats().evictions,   0u);
    EXPECT_EQ(c.stats().writeBacks,  0u);
    EXPECT_EQ(c.stats().writeThroughs, 0u);
}

TEST(StatsOps, HitRateIsCorrect) {
    Cache c(smallConfig());
    c.access(addrTag(0), false); // miss
    c.access(addrTag(0), false); // hit
    c.access(addrTag(1), false); // miss
    // 1 hit / 3 total = 0.333...
    EXPECT_NEAR(c.stats().hitRate(), 1.0 / 3.0, 1e-9);
}

TEST(StatsOps, HitRateZeroWithNoAccesses) {
    Cache c(smallConfig());
    EXPECT_EQ(c.stats().hitRate(), 0.0);
}
