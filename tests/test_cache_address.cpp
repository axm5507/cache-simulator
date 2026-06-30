#include <gtest/gtest.h>
#include "Cache.h"

static CacheConfig makeConfig(uint64_t capacityBytes, int blockBytes, int assoc) {
    CacheConfig cfg;
    cfg.capacityBytes = capacityBytes;
    cfg.blockBytes    = blockBytes;
    cfg.associativity = assoc;
    return cfg;
}

// ============================================================
// Fixture A: 32 KiB, 64-byte blocks, 4-way set-associative
//
//   numSets    = 32768 / (64 * 4) = 128
//   offsetBits = log2(64)  = 6
//   indexBits  = log2(128) = 7
//   tagBits    = 64 - 6 - 7 = 51
// ============================================================
class Cache32KB_64B_4Way : public ::testing::Test {
protected:
    Cache32KB_64B_4Way() : cache(makeConfig(32 * 1024, 64, 4)) {}
    Cache cache;
};

TEST_F(Cache32KB_64B_4Way, DerivedFieldsAreCorrect) {
    EXPECT_EQ(cache.numSets(),    128);
    EXPECT_EQ(cache.offsetBits(),   6);
    EXPECT_EQ(cache.indexBits(),    7);
    EXPECT_EQ(cache.tagBits(),     51);
}

TEST_F(Cache32KB_64B_4Way, ZeroAddress) {
    EXPECT_EQ(cache.getOffset(0x0), 0u);
    EXPECT_EQ(cache.getIndex(0x0),  0u);
    EXPECT_EQ(cache.getTag(0x0),    0u);
}

TEST_F(Cache32KB_64B_4Way, MaxOffsetDoesNotSpillIntoIndex) {
    // 0x3F fills all 6 offset bits; index and tag remain 0.
    constexpr uint64_t addr = 0x3Fu;
    EXPECT_EQ(cache.getOffset(addr), 63u);
    EXPECT_EQ(cache.getIndex(addr),   0u);
    EXPECT_EQ(cache.getTag(addr),     0u);
}

TEST_F(Cache32KB_64B_4Way, FirstByteOfSecondLine) {
    // Address 64 is the first byte of cache line 1.
    constexpr uint64_t addr = 0x40u;
    EXPECT_EQ(cache.getOffset(addr), 0u);
    EXPECT_EQ(cache.getIndex(addr),  1u);
    EXPECT_EQ(cache.getTag(addr),    0u);
}

TEST_F(Cache32KB_64B_4Way, LastSetFirstByte) {
    // Set 127 starts at byte 127 * 64 = 0x1FC0.
    constexpr uint64_t addr = 0x1FC0u;
    EXPECT_EQ(cache.getOffset(addr), 0u);
    EXPECT_EQ(cache.getIndex(addr),  127u);
    EXPECT_EQ(cache.getTag(addr),    0u);
}

TEST_F(Cache32KB_64B_4Way, SecondTagFirstSet) {
    // First address mapping to set 0 with tag 1: 128 * 64 = 0x2000.
    constexpr uint64_t addr = 0x2000u;
    EXPECT_EQ(cache.getOffset(addr), 0u);
    EXPECT_EQ(cache.getIndex(addr),  0u);
    EXPECT_EQ(cache.getTag(addr),    1u);
}

TEST_F(Cache32KB_64B_4Way, AllFieldsMaxAddress) {
    constexpr uint64_t addr = 0xFFFF'FFFF'FFFF'FFFFuLL;
    EXPECT_EQ(cache.getOffset(addr), 63u);
    EXPECT_EQ(cache.getIndex(addr),  127u);
    EXPECT_EQ(cache.getTag(addr),    (1uLL << 51) - 1);
}

TEST_F(Cache32KB_64B_4Way, OffsetDoesNotAffectIndexOrTag) {
    // All bytes within a line map to the same set and tag.
    uint64_t base = 0x2000u; // tag=1, index=0
    for (uint64_t off = 0; off < 64; ++off) {
        EXPECT_EQ(cache.getIndex(base + off), 0u)  << "offset=" << off;
        EXPECT_EQ(cache.getTag(base + off),   1u)  << "offset=" << off;
    }
}

// ============================================================
// Fixture B: 4 KiB, 32-byte blocks, direct-mapped (assoc = 1)
//
//   numSets    = 4096 / (32 * 1) = 128
//   offsetBits = log2(32) = 5
//   indexBits  = log2(128) = 7
//   tagBits    = 52
// ============================================================
class Cache4KB_32B_1Way : public ::testing::Test {
protected:
    Cache4KB_32B_1Way() : cache(makeConfig(4 * 1024, 32, 1)) {}
    Cache cache;
};

TEST_F(Cache4KB_32B_1Way, DerivedFieldsAreCorrect) {
    EXPECT_EQ(cache.numSets(),    128);
    EXPECT_EQ(cache.offsetBits(),   5);
    EXPECT_EQ(cache.indexBits(),    7);
    EXPECT_EQ(cache.tagBits(),     52);
}

TEST_F(Cache4KB_32B_1Way, AddressDecomposition) {
    // 0xA0 = 1010_0000: offset=0 (bits[4:0]), index=5 (bits[11:5])
    EXPECT_EQ(cache.getOffset(0xA0u), 0u);
    EXPECT_EQ(cache.getIndex(0xA0u),  5u);
    EXPECT_EQ(cache.getTag(0xA0u),    0u);
}

TEST_F(Cache4KB_32B_1Way, SecondTagOffset) {
    // First address with tag=1: 128 * 32 = 0x1000.
    EXPECT_EQ(cache.getOffset(0x1000u), 0u);
    EXPECT_EQ(cache.getIndex(0x1000u),  0u);
    EXPECT_EQ(cache.getTag(0x1000u),    1u);
}

// ============================================================
// Fixture C: Minimal — 2-byte capacity, 1-byte blocks, 1-way
//
//   numSets    = 2
//   offsetBits = 0
//   indexBits  = 1
//   tagBits    = 63
// ============================================================
class CacheMinimal : public ::testing::Test {
protected:
    CacheMinimal() : cache(makeConfig(2, 1, 1)) {}
    Cache cache;
};

TEST_F(CacheMinimal, DerivedFieldsAreCorrect) {
    EXPECT_EQ(cache.numSets(),     2);
    EXPECT_EQ(cache.offsetBits(),  0);
    EXPECT_EQ(cache.indexBits(),   1);
    EXPECT_EQ(cache.tagBits(),    63);
}

TEST_F(CacheMinimal, EvenAddressMapsToSetZero) {
    EXPECT_EQ(cache.getIndex(0u), 0u);
    EXPECT_EQ(cache.getIndex(2u), 0u);
    EXPECT_EQ(cache.getIndex(4u), 0u);
}

TEST_F(CacheMinimal, OddAddressMapsToSetOne) {
    EXPECT_EQ(cache.getIndex(1u), 1u);
    EXPECT_EQ(cache.getIndex(3u), 1u);
}

// ============================================================
// Config validation
// ============================================================
TEST(CacheConfigValidation, ThrowsOnNonPowerOfTwoBlockSize) {
    CacheConfig cfg;
    cfg.blockBytes = 48;
    EXPECT_THROW(Cache c(cfg), std::invalid_argument);
}

TEST(CacheConfigValidation, ThrowsOnNonPowerOfTwoCapacity) {
    CacheConfig cfg;
    cfg.capacityBytes = 3000;
    EXPECT_THROW(Cache c(cfg), std::invalid_argument);
}

TEST(CacheConfigValidation, ThrowsWhenAssociativityIsZero) {
    CacheConfig cfg;
    cfg.associativity = 0;
    EXPECT_THROW(Cache c(cfg), std::invalid_argument);
}

TEST(CacheConfigValidation, ThrowsWhenCapacityTooSmallForGeometry) {
    CacheConfig cfg;
    cfg.capacityBytes = 1024;
    cfg.blockBytes    = 64;
    cfg.associativity = 32; // needs 2048 bytes minimum
    EXPECT_THROW(Cache c(cfg), std::invalid_argument);
}

TEST(CacheConfigValidation, AcceptsValidDefaultConfig) {
    CacheConfig cfg;
    EXPECT_NO_THROW(Cache c(cfg));
}

// ============================================================
// Step 5 — Hex address decomposition across multiple configs
//
// Three configs with different block/assoc but the same total
// offsetBits + indexBits = 13, so the tag is always identical
// for a given address across all three. This demonstrates that
// the tag captures the same physical "page-like" region.
//
// Manually verified values (see comments for derivation):
// ============================================================

// --- Config table used across Step 5 tests ---
//
//  A: 32 KiB, 64 B, 4-way  → offsetBits=6, indexBits=7  (total 13)
//  B: 16 KiB, 32 B, 2-way  → offsetBits=5, indexBits=8  (total 13)
//  C: 64 KiB, 128 B, 8-way → offsetBits=7, indexBits=6  (total 13)
//
// For 0x12345678 = 305 419 896:
//   A: offset=56,  index=89,  tag=37282
//   B: offset=24,  index=179, tag=37282
//   C: offset=120, index=44,  tag=37282
//
// For 0xDEADBEEF = 3 735 928 559:
//   A: offset=47,  index=123, tag=456045
//   B: offset=15,  index=247, tag=456045
//   C: offset=111, index=61,  tag=456045

TEST(HexAddress_0x12345678, Config_32KB_64B_4Way) {
    Cache c(makeConfig(32 * 1024, 64, 4));
    EXPECT_EQ(c.getOffset(0x12345678u), 56u);
    EXPECT_EQ(c.getIndex( 0x12345678u), 89u);
    EXPECT_EQ(c.getTag(   0x12345678u), 37282u);
}

TEST(HexAddress_0x12345678, Config_16KB_32B_2Way) {
    Cache c(makeConfig(16 * 1024, 32, 2));
    EXPECT_EQ(c.getOffset(0x12345678u), 24u);
    EXPECT_EQ(c.getIndex( 0x12345678u), 179u);
    EXPECT_EQ(c.getTag(   0x12345678u), 37282u);
}

TEST(HexAddress_0x12345678, Config_64KB_128B_8Way) {
    Cache c(makeConfig(64 * 1024, 128, 8));
    EXPECT_EQ(c.getOffset(0x12345678u), 120u);
    EXPECT_EQ(c.getIndex( 0x12345678u), 44u);
    EXPECT_EQ(c.getTag(   0x12345678u), 37282u);
}

TEST(HexAddress_0xDEADBEEF, Config_32KB_64B_4Way) {
    Cache c(makeConfig(32 * 1024, 64, 4));
    EXPECT_EQ(c.getOffset(0xDEADBEEFu), 47u);
    EXPECT_EQ(c.getIndex( 0xDEADBEEFu), 123u);
    EXPECT_EQ(c.getTag(   0xDEADBEEFu), 456045u);
}

TEST(HexAddress_0xDEADBEEF, Config_16KB_32B_2Way) {
    Cache c(makeConfig(16 * 1024, 32, 2));
    EXPECT_EQ(c.getOffset(0xDEADBEEFu), 15u);
    EXPECT_EQ(c.getIndex( 0xDEADBEEFu), 247u);
    EXPECT_EQ(c.getTag(   0xDEADBEEFu), 456045u);
}

TEST(HexAddress_0xDEADBEEF, Config_64KB_128B_8Way) {
    Cache c(makeConfig(64 * 1024, 128, 8));
    EXPECT_EQ(c.getOffset(0xDEADBEEFu), 111u);
    EXPECT_EQ(c.getIndex( 0xDEADBEEFu), 61u);
    EXPECT_EQ(c.getTag(   0xDEADBEEFu), 456045u);
}

// Verify the tag invariant explicitly: when offsetBits+indexBits is held
// constant, tag(addr) is the same regardless of how the lower 13 bits are split.
TEST(HexAddress_TagInvariant, SameTagAcrossAllThreeConfigs) {
    Cache a(makeConfig(32 * 1024, 64,  4));
    Cache b(makeConfig(16 * 1024, 32,  2));
    Cache c(makeConfig(64 * 1024, 128, 8));

    for (uint64_t addr : {0x12345678u, 0xDEADBEEFu, 0x00000000u, 0xFFFFFFFFu}) {
        uint64_t tagA = a.getTag(addr);
        uint64_t tagB = b.getTag(addr);
        uint64_t tagC = c.getTag(addr);
        EXPECT_EQ(tagA, tagB) << "addr=0x" << std::hex << addr;
        EXPECT_EQ(tagB, tagC) << "addr=0x" << std::hex << addr;
    }
}

// Reconstruct the original address from its components and verify identity.
TEST(HexAddress_Reconstruction, RoundTripDefaultConfig) {
    Cache c(makeConfig(32 * 1024, 64, 4));

    for (uint64_t addr : {0x12345678u, 0xDEADBEEFu, 0x00000000u,
                          0xCAFEBABEu, 0x0FFF0FFFu}) {
        uint64_t reconstructed =
              (c.getTag(addr)    << (c.offsetBits() + c.indexBits()))
            | (c.getIndex(addr)  <<  c.offsetBits())
            |  c.getOffset(addr);
        EXPECT_EQ(reconstructed, addr) << "addr=0x" << std::hex << addr;
    }
}
