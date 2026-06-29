#include <gtest/gtest.h>
#include "CacheSet.h"

// ---- construction -----------------------------------------------------------

TEST(CacheSetTest, AssociativityIsPreserved) {
    CacheSet s4(4);
    EXPECT_EQ(s4.associativity(), 4);

    CacheSet s1(1);
    EXPECT_EQ(s1.associativity(), 1);
}

TEST(CacheSetTest, AllWaysInvalidAfterConstruction) {
    CacheSet set(4);
    for (int w = 0; w < 4; ++w)
        EXPECT_FALSE(set.getLine(w).isValid());
}

// ---- findTag ----------------------------------------------------------------

TEST(CacheSetTest, FindTagReturnsMissWhenAllInvalid) {
    CacheSet set(4);
    EXPECT_EQ(set.findTag(0xABCu), -1);
}

TEST(CacheSetTest, FindTagReturnsMissWhenTagNotPresent) {
    CacheSet set(4);
    set.getLine(0).setValid(true);
    set.getLine(0).setTag(0x1u);
    EXPECT_EQ(set.findTag(0x2u), -1);
}

TEST(CacheSetTest, FindTagIgnoresInvalidLinesEvenIfTagMatches) {
    CacheSet set(4);
    // Line 2 has matching tag but is invalid — must not be reported as a hit.
    set.getLine(2).setTag(0x42u);
    set.getLine(2).setValid(false);
    EXPECT_EQ(set.findTag(0x42u), -1);
}

TEST(CacheSetTest, FindTagReturnsCorrectWayForSingleHit) {
    CacheSet set(4);
    set.getLine(3).setValid(true);
    set.getLine(3).setTag(0xBEEFu);
    EXPECT_EQ(set.findTag(0xBEEFu), 3);
}

TEST(CacheSetTest, FindTagDistinguishesMultipleValidLines) {
    CacheSet set(4);
    set.getLine(0).setValid(true); set.getLine(0).setTag(0x10u);
    set.getLine(1).setValid(true); set.getLine(1).setTag(0x20u);
    set.getLine(2).setValid(true); set.getLine(2).setTag(0x30u);
    set.getLine(3).setValid(true); set.getLine(3).setTag(0x40u);

    EXPECT_EQ(set.findTag(0x10u), 0);
    EXPECT_EQ(set.findTag(0x20u), 1);
    EXPECT_EQ(set.findTag(0x30u), 2);
    EXPECT_EQ(set.findTag(0x40u), 3);
    EXPECT_EQ(set.findTag(0x50u), -1);
}

// ---- findEmptyWay / hasEmptyWay ---------------------------------------------

TEST(CacheSetTest, FullyEmptySetHasEmptyWayAtZero) {
    CacheSet set(4);
    EXPECT_EQ(set.findEmptyWay(), 0);
    EXPECT_TRUE(set.hasEmptyWay());
}

TEST(CacheSetTest, FindEmptyWayReturnsFirstInvalid) {
    CacheSet set(4);
    set.getLine(0).setValid(true);
    set.getLine(1).setValid(true);
    // Ways 2 and 3 are invalid; first empty should be 2.
    EXPECT_EQ(set.findEmptyWay(), 2);
}

TEST(CacheSetTest, FindEmptyWayReturnsNegativeOneWhenFull) {
    CacheSet set(4);
    for (int w = 0; w < 4; ++w)
        set.getLine(w).setValid(true);
    EXPECT_EQ(set.findEmptyWay(), -1);
    EXPECT_FALSE(set.hasEmptyWay());
}

TEST(CacheSetTest, HasEmptyWayTrueWhenOnlyOneSlotFree) {
    CacheSet set(4);
    set.getLine(0).setValid(true);
    set.getLine(1).setValid(true);
    set.getLine(2).setValid(true);
    EXPECT_TRUE(set.hasEmptyWay());
    EXPECT_EQ(set.findEmptyWay(), 3);
}

TEST(CacheSetTest, DirectMappedSingleWay) {
    CacheSet set(1);
    EXPECT_TRUE(set.hasEmptyWay());
    set.getLine(0).setValid(true);
    set.getLine(0).setTag(0x7u);
    EXPECT_EQ(set.findTag(0x7u), 0);
    EXPECT_FALSE(set.hasEmptyWay());
}
