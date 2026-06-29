#include <gtest/gtest.h>
#include "CacheLine.h"

// ---- construction -----------------------------------------------------------

TEST(CacheLineTest, DefaultConstructorProducesInvalidCleanLine) {
    CacheLine line;
    EXPECT_FALSE(line.isValid());
    EXPECT_FALSE(line.isDirty());
    EXPECT_EQ(line.getTag(), 0u);
    EXPECT_EQ(line.getLastUsed(), 0u);
    EXPECT_EQ(line.getInsertionTime(), 0u);
}

// ---- setters / getters ------------------------------------------------------

TEST(CacheLineTest, SetValidToggle) {
    CacheLine line;
    line.setValid(true);
    EXPECT_TRUE(line.isValid());
    line.setValid(false);
    EXPECT_FALSE(line.isValid());
}

TEST(CacheLineTest, SetDirtyToggle) {
    CacheLine line;
    line.setDirty(true);
    EXPECT_TRUE(line.isDirty());
    line.setDirty(false);
    EXPECT_FALSE(line.isDirty());
}

TEST(CacheLineTest, SetTagRoundTrip) {
    CacheLine line;
    constexpr uint64_t tag = 0xDEAD'BEEF'1234'5678ULL;
    line.setTag(tag);
    EXPECT_EQ(line.getTag(), tag);
}

TEST(CacheLineTest, SetLastUsedRoundTrip) {
    CacheLine line;
    line.setLastUsed(42u);
    EXPECT_EQ(line.getLastUsed(), 42u);
}

TEST(CacheLineTest, SetInsertionTimeRoundTrip) {
    CacheLine line;
    line.setInsertionTime(999u);
    EXPECT_EQ(line.getInsertionTime(), 999u);
}

// ---- reset ------------------------------------------------------------------

TEST(CacheLineTest, ResetClearsAllFields) {
    CacheLine line;
    line.setValid(true);
    line.setDirty(true);
    line.setTag(0xABCDu);
    line.setLastUsed(100u);
    line.setInsertionTime(200u);

    line.reset();

    EXPECT_FALSE(line.isValid());
    EXPECT_FALSE(line.isDirty());
    EXPECT_EQ(line.getTag(), 0u);
    EXPECT_EQ(line.getLastUsed(), 0u);
    EXPECT_EQ(line.getInsertionTime(), 0u);
}

TEST(CacheLineTest, MultipleFieldsSetIndependently) {
    CacheLine line;
    line.setValid(true);
    line.setTag(7u);
    // dirty and timestamps remain at default
    EXPECT_TRUE(line.isValid());
    EXPECT_FALSE(line.isDirty());
    EXPECT_EQ(line.getTag(), 7u);
    EXPECT_EQ(line.getLastUsed(), 0u);
}
