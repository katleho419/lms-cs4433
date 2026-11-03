// Unit tests: late-penalty strategy (boundary values) and the submission state machine.
#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

// ---- Late penalty: max mark 100, 10% per day for up to 3 days -----------------------------

struct PenaltyCase { int raw, max, lateDays, expected; const char* why; };

class PenaltyBoundary : public ::testing::TestWithParam<PenaltyCase> {};

TEST_P(PenaltyBoundary, DailyPercentPenalty) {
    const auto& c = GetParam();
    DailyPercentPenalty policy(10, 3);
    EXPECT_EQ(policy.calculate(c.raw, c.max, c.lateDays), c.expected) << c.why;
}

INSTANTIATE_TEST_SUITE_P(Boundaries, PenaltyBoundary, ::testing::Values(
    PenaltyCase{80, 100, 0, 80, "on time: no deduction"},
    PenaltyCase{80, 100, 1, 70, "first late day"},
    PenaltyCase{80, 100, 2, 60, "second late day"},
    PenaltyCase{80, 100, 3, 50, "last day inside the late window"},
    PenaltyCase{80, 100, 4, 0, "one day beyond the window: zero"},
    PenaltyCase{80, 100, 400, 0, "far beyond the window"},
    PenaltyCase{100, 100, 0, 100, "maximum raw mark, on time"},
    PenaltyCase{100, 100, 1, 90, "maximum raw mark, late"},
    PenaltyCase{0, 100, 0, 0, "minimum raw mark, on time"},
    PenaltyCase{0, 100, 2, 0, "minimum raw mark, late (never negative)"},
    PenaltyCase{5, 100, 1, 0, "deduction larger than the mark clamps to zero"},
    PenaltyCase{10, 100, 1, 0, "deduction equal to the mark"},
    PenaltyCase{11, 100, 1, 1, "deduction one less than the mark"},
    PenaltyCase{15, 15, 1, 13, "1.5 point deduction rounds up to 2"},
    PenaltyCase{4, 4, 1, 4, "0.4 point deduction rounds down to 0"},
    PenaltyCase{50, 50, 3, 35, "other maximum (50): 15 deducted"},
    PenaltyCase{1000, 1000, 2, 800, "largest maximum mark"}));

TEST(Penalty, InvalidInputsAreRejected) {
    DailyPercentPenalty p;
    EXPECT_THROW_MSG(p.calculate(-1, 100, 0), ValidationError, "between 0 and 100");
    EXPECT_THROW(p.calculate(101, 100, 0), ValidationError);
    EXPECT_THROW_MSG(p.calculate(5, 0, 0), ValidationError, "positive");
    EXPECT_THROW(p.calculate(5, -3, 0), ValidationError);
    EXPECT_THROW_MSG(p.calculate(5, 100, -1), ValidationError, "negative");
}

TEST(Penalty, ConstructorRejectsNonsenseRules) {
    EXPECT_THROW(DailyPercentPenalty(-1, 3), ValidationError);
    EXPECT_THROW(DailyPercentPenalty(101, 3), ValidationError);
    EXPECT_THROW(DailyPercentPenalty(10, -1), ValidationError);
    EXPECT_NO_THROW(DailyPercentPenalty(0, 0));
    EXPECT_NO_THROW(DailyPercentPenalty(100, 5));
}

TEST(Penalty, DifferentParametersChangeTheRule) {
    EXPECT_EQ(DailyPercentPenalty(20, 2).calculate(90, 100, 2), 50);
    EXPECT_EQ(DailyPercentPenalty(20, 2).calculate(90, 100, 3), 0);
    EXPECT_EQ(DailyPercentPenalty(0, 5).calculate(90, 100, 5), 90);
}

TEST(Penalty, NoPenaltyPolicyIsInterchangeable) {
    const IPenaltyPolicy& p = NoPenalty();  // Strategy: same interface, different rule
    EXPECT_EQ(p.calculate(80, 100, 0), 80);
    EXPECT_EQ(p.calculate(80, 100, 9), 80);
    EXPECT_THROW(p.calculate(101, 100, 0), ValidationError);
}

// ---- Submission state machine ---------------------------------------------------------------

namespace {
using S = SubmissionStatus;
const S kAll[] = {S::Submitted, S::Late, S::Marked, S::Moderated, S::Released};

Submission make(S status, int lateDays) {
    return Submission(1, 1, 1, 1, "content", DateTime(), status, lateDays, std::nullopt, std::nullopt, "");
}

bool allowed(S from, S to, int lateDays) {
    switch (from) {
        case S::Submitted: return to == S::Marked;
        case S::Late: return to == S::Marked;
        case S::Marked:
            return to == S::Marked || to == S::Moderated || (to == S::Submitted && lateDays == 0) ||
                   (to == S::Late && lateDays >= 1);
        case S::Moderated: return to == S::Moderated || to == S::Released;
        case S::Released: return false;
    }
    return false;
}
}  // namespace

TEST(SubmissionStateMachine, EveryTransitionMatchesTheStateDiagram) {
    for (int lateDays : {0, 2}) {
        for (S from : kAll) {
            for (S to : kAll) {
                Submission s = make(from, lateDays);
                if (allowed(from, to, lateDays)) {
                    EXPECT_NO_THROW(s.transitionTo(to)) << toString(from) << " -> " << toString(to);
                    EXPECT_EQ(s.getStatus(), to);
                } else {
                    EXPECT_THROW(s.transitionTo(to), BusinessRuleError) << toString(from) << " -> " << toString(to);
                    EXPECT_EQ(s.getStatus(), from) << "status must not change after a refused transition";
                }
            }
        }
    }
}

TEST(SubmissionStateMachine, ApplyMarkSetsMarksAndFeedbackOnlyFromMarkableStates) {
    Submission s = make(S::Submitted, 0);
    s.applyMark(80, 80, "Good");
    EXPECT_EQ(s.getStatus(), S::Marked);
    EXPECT_EQ(*s.getRawMark(), 80);
    EXPECT_EQ(*s.getFinalMark(), 80);
    EXPECT_EQ(s.getFeedback(), "Good");

    Submission moderated = make(S::Moderated, 0);
    EXPECT_THROW(moderated.applyMark(10, 10, "x"), BusinessRuleError);
    EXPECT_FALSE(moderated.getRawMark().has_value());  // refused: nothing set
    Submission released = make(S::Released, 0);
    EXPECT_THROW(released.applyMark(10, 10, "x"), BusinessRuleError);
}

TEST(SubmissionStateMachine, AdjustOnlyForMarkedOrModerated) {
    for (S s : kAll) {
        Submission sub = make(s, 0);
        if (s == S::Marked || s == S::Moderated) {
            EXPECT_NO_THROW(sub.adjustFinalMark(55)) << toString(s);
            EXPECT_EQ(*sub.getFinalMark(), 55);
        } else {
            EXPECT_THROW(sub.adjustFinalMark(55), BusinessRuleError) << toString(s);
        }
    }
}

TEST(SubmissionStateMachine, ReturnForRemarkingGoesBackToSubmittedOrLate) {
    Submission onTime = make(S::Marked, 0);
    onTime.returnForRemarking();
    EXPECT_EQ(onTime.getStatus(), S::Submitted);
    Submission late = make(S::Marked, 2);
    late.returnForRemarking();
    EXPECT_EQ(late.getStatus(), S::Late);
    Submission moderated = make(S::Moderated, 0);
    EXPECT_THROW(moderated.returnForRemarking(), BusinessRuleError);
    Submission fresh = make(S::Submitted, 0);
    EXPECT_THROW(fresh.returnForRemarking(), BusinessRuleError);
}

TEST(SubmissionStateMachine, OnlyReleasedWorkIsVisibleToStudent) {
    for (S s : kAll) EXPECT_EQ(make(s, 0).isVisibleToStudent(), s == S::Released) << toString(s);
}

TEST(SubmissionStateMachine, CanResubmitBoundaries) {
    const DateTime deadline = at("2026-11-08T08:00:00Z");
    Submission s = make(S::Submitted, 0);
    EXPECT_TRUE(s.canResubmit(deadline.plusSeconds(-1), deadline));
    EXPECT_TRUE(s.canResubmit(deadline, deadline));                  // at the deadline is still allowed
    EXPECT_FALSE(s.canResubmit(deadline.plusSeconds(1), deadline));  // one second late
    for (S other : {S::Late, S::Marked, S::Moderated, S::Released}) {
        EXPECT_FALSE(make(other, 0).canResubmit(deadline.plusSeconds(-100), deadline)) << toString(other);
    }
}

TEST(SubmissionStatus, StringConversionsRoundTripAndRejectUnknown) {
    for (S s : kAll) EXPECT_EQ(submissionStatusFromString(toString(s)), s);
    EXPECT_THROW(submissionStatusFromString("DONE"), ValidationError);
    EXPECT_THROW(submissionStatusFromString("submitted"), ValidationError);
}

TEST(Entities, AssignmentIsOpenUntilItsDeadline) {
    Assignment a(1, 1, "t", at("2026-11-08T08:00:00Z"), 100);
    EXPECT_TRUE(a.isOpenAt(at("2026-11-08T08:00:00Z")));
    EXPECT_FALSE(a.isOpenAt(at("2026-11-08T08:00:01Z")));
}
