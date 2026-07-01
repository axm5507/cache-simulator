#include <gtest/gtest.h>
#include "Cache.h"
#include "TraceReader.h"
#include <sstream>

// ---- isOpen -----------------------------------------------------------------

TEST(TraceReader, StreamConstructorIsOpen) {
    std::istringstream ss("R 0x1000\n");
    TraceReader reader(ss);
    EXPECT_TRUE(reader.isOpen());
}

TEST(TraceReader, FileConstructorMissingFileIsNotOpen) {
    TraceReader reader("/nonexistent/path/to/trace.txt");
    EXPECT_FALSE(reader.isOpen());
}

// ---- basic parsing ----------------------------------------------------------

TEST(TraceReader, ParsesSingleRead) {
    std::istringstream ss("R 0x12345678\n");
    TraceReader reader(ss);
    auto e = reader.next();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->op, 'R');
    EXPECT_EQ(e->address, 0x12345678u);
}

TEST(TraceReader, ParsesSingleWrite) {
    std::istringstream ss("W 0xABCD0000\n");
    TraceReader reader(ss);
    auto e = reader.next();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->op, 'W');
    EXPECT_EQ(e->address, 0xABCD0000u);
}

TEST(TraceReader, ParsesSequenceOfEntries) {
    std::istringstream ss("R 0x12345678\nW 0xABCD0000\nR 0x12345000\n");
    TraceReader reader(ss);

    auto e1 = reader.next();
    ASSERT_TRUE(e1.has_value());
    EXPECT_EQ(e1->op, 'R');
    EXPECT_EQ(e1->address, 0x12345678u);

    auto e2 = reader.next();
    ASSERT_TRUE(e2.has_value());
    EXPECT_EQ(e2->op, 'W');
    EXPECT_EQ(e2->address, 0xABCD0000u);

    auto e3 = reader.next();
    ASSERT_TRUE(e3.has_value());
    EXPECT_EQ(e3->op, 'R');
    EXPECT_EQ(e3->address, 0x12345000u);

    EXPECT_FALSE(reader.next().has_value());
}

// ---- skipping ---------------------------------------------------------------

TEST(TraceReader, SkipsBlankLines) {
    std::istringstream ss("\nR 0x1000\n\nW 0x2000\n\n");
    TraceReader reader(ss);
    auto e1 = reader.next();
    ASSERT_TRUE(e1.has_value());
    EXPECT_EQ(e1->address, 0x1000u);
    auto e2 = reader.next();
    ASSERT_TRUE(e2.has_value());
    EXPECT_EQ(e2->address, 0x2000u);
    EXPECT_FALSE(reader.next().has_value());
}

TEST(TraceReader, SkipsCommentLines) {
    std::istringstream ss("# this is a comment\nR 0x1000\n# another\nW 0x2000\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.next()->address, 0x1000u);
    EXPECT_EQ(reader.next()->address, 0x2000u);
    EXPECT_FALSE(reader.next().has_value());
}

TEST(TraceReader, SkipsLeadingWhitespaceBeforeOp) {
    std::istringstream ss("   R 0x1000\n\tW 0x2000\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.next()->address, 0x1000u);
    EXPECT_EQ(reader.next()->address, 0x2000u);
}

// ---- case normalisation -----------------------------------------------------

TEST(TraceReader, LowercaseReadNormalisedToUppercase) {
    std::istringstream ss("r 0x1000\n");
    TraceReader reader(ss);
    auto e = reader.next();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->op, 'R');
}

TEST(TraceReader, LowercaseWriteNormalisedToUppercase) {
    std::istringstream ss("w 0x2000\n");
    TraceReader reader(ss);
    auto e = reader.next();
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->op, 'W');
}

// ---- address formats --------------------------------------------------------

TEST(TraceReader, AddressWithHexPrefix) {
    std::istringstream ss("R 0xDEADBEEF\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.next()->address, 0xDEADBEEFu);
}

TEST(TraceReader, ZeroAddress) {
    std::istringstream ss("R 0x00000000\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.next()->address, 0u);
}

TEST(TraceReader, MaxUint32Address) {
    std::istringstream ss("W 0xFFFFFFFF\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.next()->address, 0xFFFFFFFFu);
}

// ---- end-of-file behaviour --------------------------------------------------

TEST(TraceReader, EmptyStreamReturnsNullopt) {
    std::istringstream ss("");
    TraceReader reader(ss);
    EXPECT_FALSE(reader.next().has_value());
}

TEST(TraceReader, StreamOfOnlyCommentsReturnsNullopt) {
    std::istringstream ss("# line 1\n# line 2\n");
    TraceReader reader(ss);
    EXPECT_FALSE(reader.next().has_value());
}

TEST(TraceReader, ReturnsNulloptAfterLastEntry) {
    std::istringstream ss("R 0x1000\n");
    TraceReader reader(ss);
    reader.next();
    EXPECT_FALSE(reader.next().has_value());
}

// ---- line number tracking ---------------------------------------------------

TEST(TraceReader, LineNumberStartsAtZero) {
    std::istringstream ss("R 0x1000\n");
    TraceReader reader(ss);
    EXPECT_EQ(reader.lineNumber(), 0u);
}

TEST(TraceReader, LineNumberIncrementsForEveryRawLine) {
    // comment(1) + blank(2) + R(3) + W(4)
    std::istringstream ss("# comment\n\nR 0x1000\nW 0x2000\n");
    TraceReader reader(ss);
    reader.next(); // reads through lines 1-3 to find the first valid entry
    EXPECT_EQ(reader.lineNumber(), 3u);
    reader.next(); // reads line 4
    EXPECT_EQ(reader.lineNumber(), 4u);
}

// ---- integration: trace reader driving a Cache ------------------------------

TEST(TraceReader, IntegrationWithCache) {
    CacheConfig cfg;
    cfg.capacityBytes = 1024;
    cfg.blockBytes    = 64;
    cfg.associativity = 4;
    Cache cache(cfg);

    std::istringstream ss(
        "R 0x00000000\n"  // miss
        "R 0x00000000\n"  // hit
        "W 0x00000100\n"  // miss
        "R 0x00000100\n"  // hit
    );
    TraceReader reader(ss);
    while (auto e = reader.next())
        cache.access(e->address, e->op == 'W');

    EXPECT_EQ(cache.stats().misses, 2u);
    EXPECT_EQ(cache.stats().hits,   2u);
    EXPECT_EQ(cache.stats().reads,  3u);
    EXPECT_EQ(cache.stats().writes, 1u);
}
