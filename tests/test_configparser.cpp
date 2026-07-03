#include <gtest/gtest.h>
#include "ConfigParser.h"
#include <fstream>
#include <cstdio>

// ---- blank / comment lines --------------------------------------------------

TEST(ConfigParser, SkipsBlankLine) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("", cfg));
}

TEST(ConfigParser, SkipsWhitespaceOnlyLine) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("   \t  ", cfg));
}

TEST(ConfigParser, SkipsCommentLine) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("# this is a comment", cfg));
}

TEST(ConfigParser, SkipsMissingEquals) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("block_size 64", cfg));
}

TEST(ConfigParser, UnknownKeyReturnsFalse) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("invalid_key=something", cfg));
}

// ---- name -------------------------------------------------------------------

TEST(ConfigParser, ParseName) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("name=MyL1Cache", cfg));
    EXPECT_EQ(cfg.name, "MyL1Cache");
}

TEST(ConfigParser, NamePreservesCase) {
    CacheConfig cfg;
    ConfigParser::applyLine("name=L2-Unified", cfg);
    EXPECT_EQ(cfg.name, "L2-Unified");
}

// ---- cache_size / block_size ------------------------------------------------

TEST(ConfigParser, ParseCacheSizePlain) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("cache_size=32768", cfg));
    EXPECT_EQ(cfg.capacityBytes, 32768u);
}

TEST(ConfigParser, ParseCacheSizeKSuffix) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("cache_size=32K", cfg));
    EXPECT_EQ(cfg.capacityBytes, 32768u);
}

TEST(ConfigParser, ParseCacheSizeMSuffix) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("cache_size=1M", cfg));
    EXPECT_EQ(cfg.capacityBytes, 1048576u);
}

TEST(ConfigParser, ParseCapacityAlias) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("capacity=64K", cfg));
    EXPECT_EQ(cfg.capacityBytes, 65536u);
}

TEST(ConfigParser, ParseBlockSize) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("block_size=64", cfg));
    EXPECT_EQ(cfg.blockBytes, 64);
}

// ---- associativity ----------------------------------------------------------

TEST(ConfigParser, ParseAssociativity) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("associativity=8", cfg));
    EXPECT_EQ(cfg.associativity, 8);
}

TEST(ConfigParser, ParseAssocAlias) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("assoc=1", cfg));
    EXPECT_EQ(cfg.associativity, 1);
}

// ---- replacement policy -----------------------------------------------------

TEST(ConfigParser, ParseReplacementLRU) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("replacement=lru", cfg));
    EXPECT_EQ(cfg.replacementPolicy, ReplacementPolicy::LRU);
}

TEST(ConfigParser, ParseReplacementFIFO) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("replacement=fifo", cfg));
    EXPECT_EQ(cfg.replacementPolicy, ReplacementPolicy::FIFO);
}

TEST(ConfigParser, ParseReplacementRandom) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("replacement=random", cfg));
    EXPECT_EQ(cfg.replacementPolicy, ReplacementPolicy::Random);
}

TEST(ConfigParser, UnknownReplacementReturnsFalse) {
    CacheConfig cfg;
    EXPECT_FALSE(ConfigParser::applyLine("replacement=clock", cfg));
}

// ---- write policy -----------------------------------------------------------

TEST(ConfigParser, ParseWritePolicyWriteBack) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_policy=write_back", cfg));
    EXPECT_EQ(cfg.writePolicy, WritePolicy::WriteBack);
}

TEST(ConfigParser, ParseWritePolicyWriteBackAlias) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_policy=writeback", cfg));
    EXPECT_EQ(cfg.writePolicy, WritePolicy::WriteBack);
}

TEST(ConfigParser, ParseWritePolicyWriteThrough) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_policy=write_through", cfg));
    EXPECT_EQ(cfg.writePolicy, WritePolicy::WriteThrough);
}

TEST(ConfigParser, ParseWritePolicyWriteThroughAlias) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_policy=writethrough", cfg));
    EXPECT_EQ(cfg.writePolicy, WritePolicy::WriteThrough);
}

// ---- write-miss policy ------------------------------------------------------

TEST(ConfigParser, ParseWriteMissAllocate) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_miss=write_allocate", cfg));
    EXPECT_EQ(cfg.writeMissPolicy, WriteMissPolicy::WriteAllocate);
}

TEST(ConfigParser, ParseWriteMissNoAllocate) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_miss=no_write_allocate", cfg));
    EXPECT_EQ(cfg.writeMissPolicy, WriteMissPolicy::NoWriteAllocate);
}

TEST(ConfigParser, ParseWriteMissNoAllocateAlias) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("write_miss=no_allocate", cfg));
    EXPECT_EQ(cfg.writeMissPolicy, WriteMissPolicy::NoWriteAllocate);
}

// ---- formatting tolerance ---------------------------------------------------

TEST(ConfigParser, TrimsWhitespaceAroundKeyAndValue) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("  block_size  =  128  ", cfg));
    EXPECT_EQ(cfg.blockBytes, 128);
}

TEST(ConfigParser, CaseInsensitiveKey) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("CACHE_SIZE=1024", cfg));
    EXPECT_EQ(cfg.capacityBytes, 1024u);
}

TEST(ConfigParser, MixedCaseKey) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("Block_Size=32", cfg));
    EXPECT_EQ(cfg.blockBytes, 32);
}

TEST(ConfigParser, InlineCommentIsStripped) {
    CacheConfig cfg;
    EXPECT_TRUE(ConfigParser::applyLine("cache_size=32768 # L1 cache", cfg));
    EXPECT_EQ(cfg.capacityBytes, 32768u);
}

// ---- defaults are preserved -------------------------------------------------

TEST(ConfigParser, UnrelatedFieldsAreUntouched) {
    CacheConfig cfg;
    cfg.associativity = 8;
    ConfigParser::applyLine("block_size=32", cfg);
    EXPECT_EQ(cfg.associativity, 8); // unchanged
    EXPECT_EQ(cfg.blockBytes,   32); // updated
}

// ---- multiple lines ---------------------------------------------------------

TEST(ConfigParser, MultipleLinesBuildConfig) {
    CacheConfig cfg;
    ConfigParser::applyLine("cache_size=16K",   cfg);
    ConfigParser::applyLine("block_size=32",    cfg);
    ConfigParser::applyLine("associativity=2",  cfg);
    ConfigParser::applyLine("replacement=fifo", cfg);
    EXPECT_EQ(cfg.capacityBytes,     16384u);
    EXPECT_EQ(cfg.blockBytes,        32);
    EXPECT_EQ(cfg.associativity,     2);
    EXPECT_EQ(cfg.replacementPolicy, ReplacementPolicy::FIFO);
}

// ---- fromFile ---------------------------------------------------------------

TEST(ConfigParser, FromFileLoadsAllKeys) {
    const std::string path = "test_tmp_all_keys.conf";
    {
        std::ofstream f(path);
        f << "# test config\n"
          << "name          = TestL1\n"
          << "cache_size    = 16K\n"
          << "block_size    = 32\n"
          << "associativity = 2\n"
          << "replacement   = fifo\n"
          << "write_policy  = write_through\n"
          << "write_miss    = no_write_allocate\n";
    }
    CacheConfig cfg = ConfigParser::fromFile(path);
    EXPECT_EQ(cfg.name,              "TestL1");
    EXPECT_EQ(cfg.capacityBytes,     16 * 1024u);
    EXPECT_EQ(cfg.blockBytes,        32);
    EXPECT_EQ(cfg.associativity,     2);
    EXPECT_EQ(cfg.replacementPolicy, ReplacementPolicy::FIFO);
    EXPECT_EQ(cfg.writePolicy,       WritePolicy::WriteThrough);
    EXPECT_EQ(cfg.writeMissPolicy,   WriteMissPolicy::NoWriteAllocate);
    std::remove(path.c_str());
}

TEST(ConfigParser, FromFileDefaultsPreservedForMissingKeys) {
    const std::string path = "test_tmp_partial.conf";
    {
        std::ofstream f(path);
        f << "associativity=8\n"; // only one key
    }
    CacheConfig base;
    base.capacityBytes = 65536;
    CacheConfig cfg = ConfigParser::fromFile(path, base);
    EXPECT_EQ(cfg.capacityBytes, 65536u); // kept from base
    EXPECT_EQ(cfg.associativity, 8);      // from file
    std::remove(path.c_str());
}

TEST(ConfigParser, FromFileSkipsBlankAndCommentLines) {
    const std::string path = "test_tmp_comments.conf";
    {
        std::ofstream f(path);
        f << "\n"
          << "# header comment\n"
          << "\n"
          << "block_size=128 # cache line\n"
          << "\n";
    }
    CacheConfig cfg = ConfigParser::fromFile(path);
    EXPECT_EQ(cfg.blockBytes, 128);
    std::remove(path.c_str());
}

TEST(ConfigParser, FromFileNonexistentThrows) {
    EXPECT_THROW(ConfigParser::fromFile("/nonexistent/path/config.conf"),
                 std::runtime_error);
}
