// Unit tests: DateTime and Validation (equivalence classes and boundaries).
#include "TestSupport.hpp"
#include "util/Validation.hpp"

using namespace lms;
using namespace lms::test;

TEST(DateTime, EpochZeroIsTheUnixEpoch) {
    EXPECT_EQ(DateTime::fromEpochSeconds(0).toIso(), "1970-01-01T00:00:00Z");
}

TEST(DateTime, NegativeEpochRoundsDownToPreviousDay) {
    EXPECT_EQ(DateTime::fromEpochSeconds(-1).toIso(), "1969-12-31T23:59:59Z");
}

TEST(DateTime, IsoRoundTrip) {
    for (const char* text : {"2026-11-07T23:59:00Z", "2028-02-29T00:00:00Z", "1970-01-01T00:00:01Z", "9999-12-31T23:59:59Z"}) {
        EXPECT_EQ(DateTime::parseIso(text).toIso(), text);
    }
}

TEST(DateTime, LeapDayIsValidOnlyInLeapYears) {
    EXPECT_NO_THROW(DateTime::parseIso("2028-02-29T10:00:00Z"));
    EXPECT_THROW(DateTime::parseIso("2026-02-29T10:00:00Z"), ValidationError);
    EXPECT_THROW(DateTime::parseIso("2100-02-29T10:00:00Z"), ValidationError);  // century, not leap
    EXPECT_NO_THROW(DateTime::parseIso("2000-02-29T10:00:00Z"));                // divisible by 400
}

TEST(DateTime, RejectsImpossibleOrMalformedIso) {
    for (const char* bad : {"", "2026-11-07", "2026-13-01T00:00:00Z", "2026-00-10T00:00:00Z", "2026-04-31T00:00:00Z",
                            "2026-11-07T24:00:00Z", "2026-11-07T23:60:00Z", "2026-11-07T23:59:60Z",
                            "2026/11/07T23:59:00Z", "2026-11-07 23:59:00Z", "2026-11-07T23:59:00", "abcd-ef-ghTij:kl:mmZ",
                            "1969-12-31T23:59:59Z", "2026-11-00T00:00:00Z"}) {
        EXPECT_THROW(DateTime::parseIso(bad), ValidationError) << bad;
    }
}

TEST(DateTime, ParseLocalConvertsUsingOffset) {
    // 23:59 local at UTC+2 is 21:59 UTC.
    EXPECT_EQ(DateTime::parseLocal("2026-11-07 23:59", 120).toIso(), "2026-11-07T21:59:00Z");
    EXPECT_EQ(DateTime::parseLocal("2026-11-07 00:30", 120).toIso(), "2026-11-06T22:30:00Z");  // crosses midnight
    EXPECT_EQ(DateTime::parseLocal("2026-11-07 10:00", 0).toIso(), "2026-11-07T10:00:00Z");
    EXPECT_EQ(DateTime::parseLocal("2026-11-07 10:00", -300).toIso(), "2026-11-07T15:00:00Z");
}

TEST(DateTime, ToLocalIsInverseOfParseLocal) {
    const DateTime t = DateTime::parseLocal("2026-12-31 23:45", 120);
    EXPECT_EQ(t.toLocal(120), "2026-12-31 23:45");
}

TEST(DateTime, ParseLocalRejectsBadInput) {
    for (const char* bad : {"", "2026-11-07", "2026-11-07T23:59", "2026-11-07 23:59:00", "07-11-2026 23:59", "2026-02-30 10:00",
                            "2026-11-07 25:00", "2026-11-07 12:60", "2026-1-7 12:00", "abcdefghijklmnop"}) {
        EXPECT_THROW_MSG(DateTime::parseLocal(bad, 120), ValidationError, "YYYY-MM-DD HH:MM");
    }
}

TEST(DateTime, ArithmeticAndComparison) {
    const DateTime a = DateTime::fromEpochSeconds(1000);
    const DateTime b = a.plusSeconds(60);
    EXPECT_EQ(b.secondsSince(a), 60);
    EXPECT_EQ(a.secondsSince(b), -60);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a <= a);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a == DateTime::fromEpochSeconds(1000));
    EXPECT_EQ(b.epochSeconds(), 1060);
}

TEST(Validation, TrimRemovesOuterWhitespaceOnly) {
    EXPECT_EQ(trim("  a b \t\n"), "a b");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
    EXPECT_EQ(trim("x"), "x");
}

TEST(Validation, RequireTextBoundaries) {
    EXPECT_EQ(requireText("  abc ", "Field", 3), "abc");               // exactly max after trimming
    EXPECT_THROW_MSG(requireText("abcd", "Field", 3), ValidationError, "at most 3");  // max + 1
    EXPECT_EQ(requireText("a", "Field", 3), "a");                       // min valid
    EXPECT_THROW_MSG(requireText("", "The name", 3), ValidationError, "The name must not be empty");
    EXPECT_THROW(requireText("   \t", "Field", 3), ValidationError);
}

TEST(Validation, RequireRangeBoundaries) {
    EXPECT_EQ(requireRange(0, "M", 0, 100), 0);
    EXPECT_EQ(requireRange(100, "M", 0, 100), 100);
    EXPECT_EQ(requireRange(50, "M", 0, 100), 50);
    EXPECT_THROW_MSG(requireRange(-1, "The mark", 0, 100), ValidationError, "between 0 and 100");
    EXPECT_THROW(requireRange(101, "M", 0, 100), ValidationError);
}
