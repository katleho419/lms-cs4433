// System and acceptance tests. These drive the real console application with
// scripted keyboard input against a real (in-memory or file) SQLite database,
// so they exercise every layer from the menu to the stored row.
//
// Naming: SYS-xx are system tests derived from the use cases UC-01..UC-03 and
// their exception flows. ACC-xx are acceptance scenarios written from the point
// of view of each role (see docs/TEST_PLAN_SYSTEM_ACCEPTANCE.md).
#include <cstdio>
#include <sstream>
#include <unistd.h>

#include "TestSupport.hpp"
#include "ui/ConsoleApp.hpp"

namespace lms::test {
namespace {

constexpr const char* kPw = World::kPassword;  // "Passw0rd1" for every seeded user except admin

// ---- scripting helpers -----------------------------------------------------
std::string login(const std::string& user, const std::string& pw = kPw) { return user + "\n" + pw + "\n"; }
const std::string kQuit = "\n";  // empty username ends the program

std::string submitWork(int assignmentId, const std::string& text) {
    return "2\n" + std::to_string(assignmentId) + "\n" + text + "\n.\n";
}
std::string markWork(int id, int raw, const std::string& fb, const std::string& reason = "") {
    return "6\n" + std::to_string(id) + "\n" + std::to_string(raw) + "\n" + fb + "\n" + reason + "\n";
}

// Runs the console program once with the given keystrokes and returns everything it printed.
std::string run(World& w, const std::string& keys, int utcOffsetMinutes = 0) {
    std::istringstream in(keys);
    std::ostringstream out;
    ConsoleApp app({w.auth, w.admin, w.courseSvc, w.assignments, w.submissions, w.marking, w.moderation, w.users},
                   in, out, utcOffsetMinutes);
    app.run();
    return out.str();
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Convenience sessions -------------------------------------------------------
std::string asStudent(World& w, const std::string& user, const std::string& menuKeys) {
    return run(w, login(user) + menuKeys + "0\n" + kQuit);
}
std::string asLecturer(World& w, const std::string& menuKeys, const std::string& user = "lec1") {
    return run(w, login(user) + menuKeys + "0\n" + kQuit);
}
std::string asHod(World& w, const std::string& menuKeys, const std::string& user = "hod") {
    return run(w, login(user) + menuKeys + "0\n" + kQuit);
}

class SystemTest : public ::testing::Test {
protected:
    World w{true};
    int aid() const { return w.assignment.getId(); }
    std::optional<Submission> latest(const UserPtr& s) { return w.submissionRepo.findLatest(aid(), s->getId()); }
    int auditCount(int submissionId) { return static_cast<int>(w.auditRepo.findBySubmission(submissionId).size()); }
    void at_(const DateTime& t) { w.setNow(t); }
    DateTime beforeDeadline() { return w.deadline().plusSeconds(-3600); }
    DateTime afterDeadline(int hours) { return w.deadline().plusSeconds(hours * 3600); }
};

// ============================ UC-01 Submit assignment =======================

TEST_F(SystemTest, SYS01_StudentSubmitsOnTime_MainFlow) {  // UC-01 main flow
    at_(beforeDeadline());
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "My answer to question 1"));
    EXPECT_TRUE(has(out, "Welcome, Thabo Student (Student)"));
    EXPECT_TRUE(has(out, "Submitted as version 1"));
    EXPECT_FALSE(has(out, "LATE"));
    const auto s = latest(w.stu1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->getStatus(), SubmissionStatus::Submitted);
    EXPECT_EQ(s->getLateDays(), 0);
    EXPECT_EQ(s->getContent(), "My answer to question 1\n");
}

TEST_F(SystemTest, SYS02_ResubmitBeforeDeadline_CreatesNewVersion) {  // UC-01 A1
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "draft"));
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "final"));
    EXPECT_TRUE(has(out, "Submitted as version 2"));
    EXPECT_EQ(latest(w.stu1)->getVersion(), 2);
    EXPECT_EQ(w.submissionRepo.findByAssignment(aid()).size(), 2u);  // old version kept
}

TEST_F(SystemTest, SYS03_SubmitJustAfterDeadline_IsLateByOneDay) {  // UC-01 A2, BR late = ceil(days)
    at_(afterDeadline(1));
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "late work"));
    EXPECT_TRUE(has(out, "(LATE by 1 day(s))"));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Late);
}

TEST_F(SystemTest, SYS04_SubmitTwoAndAHalfDaysLate_IsLateByThreeDays) {
    at_(afterDeadline(60));
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "very late"));
    EXPECT_TRUE(has(out, "(LATE by 3 day(s))"));
}

TEST_F(SystemTest, SYS05_EmptySubmission_IsRejectedAndNothingStored) {  // UC-01 E1
    at_(beforeDeadline());
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "   "));
    EXPECT_TRUE(has(out, "! The submission must not be empty."));
    EXPECT_FALSE(latest(w.stu1).has_value());
}

TEST_F(SystemTest, SYS06_NotEnrolledStudent_IsRefused) {  // UC-01 E2
    at_(beforeDeadline());
    const std::string out = asStudent(w, "stu3", submitWork(aid(), "sneaky"));
    EXPECT_TRUE(has(out, "! You are not enrolled in the course for this assignment."));
    EXPECT_FALSE(latest(w.stu3).has_value());
}

TEST_F(SystemTest, SYS07_UnknownAssignment_IsReportedNotCrashed) {  // UC-01 E3
    at_(beforeDeadline());
    const std::string out = asStudent(w, "stu1", submitWork(999, "x"));
    EXPECT_TRUE(has(out, "! There is no assignment with id 999."));
    EXPECT_TRUE(has(out, "Goodbye."));
}

TEST_F(SystemTest, SYS08_ResubmitAfterDeadline_IsRefused) {  // UC-01 E4
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "first"));
    at_(afterDeadline(2));
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "second"));
    EXPECT_TRUE(has(out, "! The deadline has passed, so you can no longer resubmit."));
    EXPECT_EQ(latest(w.stu1)->getVersion(), 1);
}

TEST_F(SystemTest, SYS09_ResubmitAfterMarking_IsRefused) {  // UC-01 E5
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "first"));
    const int id = latest(w.stu1)->getId();
    asLecturer(w, markWork(id, 70, "ok"));
    const std::string out = asStudent(w, "stu1", submitWork(aid(), "changed my mind"));
    EXPECT_TRUE(has(out, "can no longer be replaced"));
    EXPECT_EQ(latest(w.stu1)->getVersion(), 1);
}

TEST_F(SystemTest, SYS10_ExtensionForOneStudent_MakesLateWorkOnTime) {  // UC-01 A3 / BR extension
    at_(beforeDeadline());
    // Lecturer grants stu1 two more days. Dates are typed in UTC for this run.
    const DateTime ext = w.deadline().plusSeconds(2 * kDay);
    const std::string when = ext.toLocal(0);
    asLecturer(w, "4\n" + std::to_string(aid()) + "\n" + std::to_string(w.stu1->getId()) + "\n" + when + "\n");
    at_(afterDeadline(24));
    const std::string out1 = asStudent(w, "stu1", submitWork(aid(), "with extension"));
    EXPECT_FALSE(has(out1, "LATE"));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Submitted);
    const std::string out2 = asStudent(w, "stu2", submitWork(aid(), "no extension"));  // no extension for stu2
    EXPECT_TRUE(has(out2, "(LATE by 1 day(s))"));
}

TEST_F(SystemTest, SYS11_SubmissionLargerThanLimit_IsRejected) {
    at_(beforeDeadline());
    const std::string big(300000, 'a');
    const std::string out = asStudent(w, "stu1", submitWork(aid(), big));
    EXPECT_TRUE(has(out, "! The submission is too large"));
    EXPECT_FALSE(latest(w.stu1).has_value());
}

// ============================ UC-02 Mark submission =========================

TEST_F(SystemTest, SYS20_MarkOnTimeWork_NoPenalty) {  // UC-02 main flow
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "answer"));
    const int id = latest(w.stu1)->getId();
    const std::string out = asLecturer(w, "5\n" + std::to_string(aid()) + "\n" + markWork(id, 80, "Good work"));
    EXPECT_TRUE(has(out, "Thabo Student (stu1)  v1  SUBMITTED"));
    EXPECT_TRUE(has(out, "Marked. Raw 80, final after late penalty 80."));
    const auto s = latest(w.stu1);
    EXPECT_EQ(s->getStatus(), SubmissionStatus::Marked);
    EXPECT_EQ(*s->getFinalMark(), 80);
    EXPECT_EQ(auditCount(id), 1);
}

TEST_F(SystemTest, SYS21_MarkTwoDaysLateWork_AppliesTwentyPercentPenalty) {  // UC-02 A1
    at_(afterDeadline(30));
    asStudent(w, "stu1", submitWork(aid(), "late"));
    const int id = latest(w.stu1)->getId();
    const std::string out = asLecturer(w, markWork(id, 80, "Late but fine"));
    EXPECT_TRUE(has(out, "Marked. Raw 80, final after late penalty 60."));
    EXPECT_EQ(*latest(w.stu1)->getRawMark(), 80);
}

TEST_F(SystemTest, SYS22_WorkMoreThanThreeDaysLate_ScoresZero) {  // UC-02 A2
    at_(afterDeadline(24 * 4 - 1));
    asStudent(w, "stu1", submitWork(aid(), "way too late"));
    const int id = latest(w.stu1)->getId();
    const std::string out = asLecturer(w, markWork(id, 90, "Too late"));
    EXPECT_TRUE(has(out, "final after late penalty 0."));
}

TEST_F(SystemTest, SYS22b_WorkExactlyThreeDaysLate_StillGetsThirtyPercentPenalty) {  // UC-02 A1/A2 boundary
    at_(afterDeadline(72));  // exactly 3 days: last day inside the late window
    asStudent(w, "stu1", submitWork(aid(), "last chance"));
    const int id = latest(w.stu1)->getId();
    EXPECT_EQ(latest(w.stu1)->getLateDays(), 3);
    const std::string out = asLecturer(w, markWork(id, 80, "Just in time"));
    EXPECT_TRUE(has(out, "Marked. Raw 80, final after late penalty 50."));
}

TEST_F(SystemTest, SYS23_RawMarkAboveMaximum_IsRejected) {  // UC-02 E1
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "answer"));
    const int id = latest(w.stu1)->getId();
    const std::string out = asLecturer(w, markWork(id, 101, "x"));
    EXPECT_TRUE(has(out, "! The mark must be between 0 and 100."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Submitted);
}

TEST_F(SystemTest, SYS24_NonNumericMark_IsReprompted) {  // UC-02 E2
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "answer"));
    const int id = latest(w.stu1)->getId();
    const std::string keys = "6\n" + std::to_string(id) + "\nabc\n75\nNice\n\n";
    const std::string out = asLecturer(w, keys);
    EXPECT_TRUE(has(out, "Please enter a whole number."));
    EXPECT_TRUE(has(out, "Marked. Raw 75"));
}

TEST_F(SystemTest, SYS25_LecturerOfAnotherCourse_CannotMark) {  // UC-02 E3
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "answer"));
    const int id = latest(w.stu1)->getId();
    const std::string out = asLecturer(w, markWork(id, 50, "x"), "lec2");  // lec2 teaches nothing
    EXPECT_TRUE(has(out, "! You do not teach this course."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Submitted);
    EXPECT_EQ(auditCount(id), 0);
}

TEST_F(SystemTest, SYS26_ReMarkWithoutReason_IsRejected_WithReason_IsAudited) {  // UC-02 E4 / A3
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "answer"));
    const int id = latest(w.stu1)->getId();
    asLecturer(w, markWork(id, 60, "first pass"));
    const std::string refused = asLecturer(w, markWork(id, 65, "second pass"));
    EXPECT_TRUE(has(refused, "! A reason is required when changing a mark."));
    EXPECT_EQ(*latest(w.stu1)->getFinalMark(), 60);
    const std::string ok = asLecturer(w, markWork(id, 65, "second pass", "Missed a section"));
    EXPECT_TRUE(has(ok, "Marked. Raw 65"));
    EXPECT_EQ(auditCount(id), 2);
}

TEST_F(SystemTest, SYS27_MarkingUnknownSubmission_IsReported) {  // UC-02 E5
    const std::string out = asLecturer(w, markWork(4242, 50, "x"));
    EXPECT_TRUE(has(out, "! There is no submission with id 4242."));
}

TEST_F(SystemTest, SYS28_StudentCannotReachMarkingMenu) {  // role separation
    const std::string out = asStudent(w, "stu1", "6\n");
    EXPECT_TRUE(has(out, "Unknown option."));
    EXPECT_FALSE(has(out, "Submission id:"));
}

// ============================ UC-03 Moderate marks ==========================

class ModerationSystemTest : public SystemTest {
protected:
    int id1 = 0;
    void SetUp() override {
        at_(beforeDeadline());
        asStudent(w, "stu1", submitWork(aid(), "answer one"));
        id1 = latest(w.stu1)->getId();
        asLecturer(w, markWork(id1, 70, "Solid"));
    }
};

TEST_F(ModerationSystemTest, SYS30_HodApprovesOne_ThenReleases_StudentSeesMark) {  // UC-03 main flow
    const std::string approve = asHod(w, "5\n" + std::to_string(id1) + "\ny\n");
    EXPECT_TRUE(has(approve, "raw 70, final 70, feedback: Solid"));
    EXPECT_TRUE(has(approve, "Approved."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Moderated);
    const std::string rel = asHod(w, "8\n" + std::to_string(aid()) + "\n");
    EXPECT_TRUE(has(rel, "Released 1 mark(s); 0 still hidden"));
    const std::string view = asStudent(w, "stu1", "1\n");
    EXPECT_TRUE(has(view, "MARK 70/100  feedback: Solid"));
}

TEST_F(ModerationSystemTest, SYS31_StudentSeesNoMarkBeforeRelease_BR05) {
    const std::string marked = asStudent(w, "stu1", "1\n");
    EXPECT_TRUE(has(marked, "submitted (v1), awaiting marks"));
    EXPECT_FALSE(has(marked, "MARK"));
    EXPECT_FALSE(has(marked, "Solid"));
    asHod(w, "5\n" + std::to_string(id1) + "\ny\n");  // moderated, still not released
    const std::string moderated = asStudent(w, "stu1", "1\n");
    EXPECT_FALSE(has(moderated, "MARK"));
    EXPECT_FALSE(has(moderated, "Solid"));
}

TEST_F(ModerationSystemTest, SYS32_HodDeclinesApproval_NothingChanges) {  // UC-03 A1
    const std::string out = asHod(w, "5\n" + std::to_string(id1) + "\nn\n");
    EXPECT_TRUE(has(out, "Not approved."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Marked);
}

TEST_F(ModerationSystemTest, SYS33_HodAdjustsMark_WithReason_IsAudited) {  // UC-03 A2
    const std::string out = asHod(w, "4\n" + std::to_string(id1) + "\n75\nModeration across the cohort\n");
    EXPECT_TRUE(has(out, "Mark adjusted and logged."));
    EXPECT_EQ(*latest(w.stu1)->getFinalMark(), 75);
    const std::string audit = asHod(w, "9\n" + std::to_string(id1) + "\n");
    EXPECT_TRUE(has(audit, "70 -> 75"));
    EXPECT_TRUE(has(audit, "Moderation across the cohort"));
}

TEST_F(ModerationSystemTest, SYS34_AdjustWithoutReason_IsRejected) {  // UC-03 E1
    const std::string out = asHod(w, "4\n" + std::to_string(id1) + "\n75\n\n");
    EXPECT_TRUE(has(out, "! The reason must not be empty."));
    EXPECT_EQ(*latest(w.stu1)->getFinalMark(), 70);
}

TEST_F(ModerationSystemTest, SYS35_HodFromAnotherDepartment_IsRefused) {  // UC-03 E2
    const std::string out = asHod(w, "5\n" + std::to_string(id1) + "\ny\n", "hod2");
    EXPECT_TRUE(has(out, "! This course is not in your department."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Marked);
}

TEST_F(ModerationSystemTest, SYS36_ApproveUnmarkedWork_IsRefused) {  // UC-03 E3
    at_(beforeDeadline());
    asStudent(w, "stu2", submitWork(aid(), "answer two"));
    const int id2 = latest(w.stu2)->getId();
    const std::string out = asHod(w, "5\n" + std::to_string(id2) + "\ny\n");
    EXPECT_TRUE(has(out, "! That work has not been marked yet."));
}

TEST_F(ModerationSystemTest, SYS37_MarkChangedWhileHodWasReviewing_IsNotApproved) {  // UC-03 E4 (DEF-02)
    // The HoD sees 70. Before "y" is typed the lecturer changes the mark to 40.
    // The console reads the whole script up front, so emulate the race at the service level
    // through the same UI path by changing the mark between review and approve using two sessions.
    const Submission seen = w.moderation.review(*w.hod, id1);
    asLecturer(w, markWork(id1, 40, "Re-marked", "Found a plagiarised paragraph"));
    EXPECT_THROW_MSG(w.moderation.approve(*w.hod, id1, *seen.getFinalMark()), BusinessRuleError,
                     "changed after you reviewed it");
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Marked);
    EXPECT_EQ(*latest(w.stu1)->getFinalMark(), 40);
}

TEST_F(ModerationSystemTest, SYS38_HodReturnsToLecturer_StudentWorkBecomesMarkableAgain) {  // UC-03 A3
    const std::string out = asHod(w, "7\n" + std::to_string(id1) + "\nPlease double check Q3\n");
    EXPECT_TRUE(has(out, "Returned to the lecturer."));
    EXPECT_EQ(latest(w.stu1)->getStatus(), SubmissionStatus::Submitted);
    const std::string remark = asLecturer(w, markWork(id1, 72, "Rechecked"));
    EXPECT_TRUE(has(remark, "Marked. Raw 72"));
}

TEST_F(ModerationSystemTest, SYS39_ReleaseWithUnmoderatedWork_KeepsThatWorkHidden) {  // UC-03 A4
    at_(beforeDeadline());
    asStudent(w, "stu2", submitWork(aid(), "answer two"));
    const int id2 = latest(w.stu2)->getId();
    asLecturer(w, markWork(id2, 55, "Fair"));
    asHod(w, "5\n" + std::to_string(id1) + "\ny\n");  // only stu1 is moderated
    const std::string rel = asHod(w, "8\n" + std::to_string(aid()) + "\n");
    EXPECT_TRUE(has(rel, "Released 1 mark(s); 1 still hidden"));
    EXPECT_TRUE(has(asStudent(w, "stu1", "1\n"), "MARK 70/100"));
    const std::string hidden = asStudent(w, "stu2", "1\n");
    EXPECT_FALSE(has(hidden, "MARK"));
    EXPECT_FALSE(has(hidden, "Fair"));
}

TEST_F(ModerationSystemTest, SYS40_LecturerCannotChangeMarkAfterModeration) {  // E5
    asHod(w, "5\n" + std::to_string(id1) + "\ny\n");
    const std::string out = asLecturer(w, markWork(id1, 10, "x", "why"));
    EXPECT_TRUE(has(out, "only the Head of Department can change its mark"));
    EXPECT_EQ(*latest(w.stu1)->getFinalMark(), 70);
}

TEST_F(ModerationSystemTest, SYS41_ApproveAll_ShowsListAndApprovesOnlyWhatWasShown) {
    at_(beforeDeadline());
    asStudent(w, "stu2", submitWork(aid(), "answer two"));
    asLecturer(w, markWork(latest(w.stu2)->getId(), 55, "Fair"));
    const std::string out = asHod(w, "6\n" + std::to_string(aid()) + "\ny\n");
    EXPECT_TRUE(has(out, "Approve the 2 marked submission(s) shown above?"));
    EXPECT_TRUE(has(out, "Approved 2 submission(s)."));
}

// ============================ Access control / login ========================

TEST_F(SystemTest, SYS50_WrongPassword_GivesGenericMessage) {
    const std::string out = run(w, login("stu1", "WrongPass99") + kQuit);
    EXPECT_TRUE(has(out, "! Invalid username or password."));
    EXPECT_FALSE(has(out, "Welcome"));
}

TEST_F(SystemTest, SYS51_UnknownUser_GivesSameMessageAsWrongPassword) {
    const std::string a = run(w, login("nobody") + kQuit);
    const std::string b = run(w, login("stu1", "WrongPass99") + kQuit);
    EXPECT_TRUE(has(a, "! Invalid username or password."));
    EXPECT_TRUE(has(b, "! Invalid username or password."));
}

TEST_F(SystemTest, SYS52_FiveFailedLogins_EndTheProgram) {
    std::string keys;
    for (int i = 0; i < 5; ++i) keys += login("stu1", "WrongPass99");
    const std::string out = run(w, keys + login("stu1"));
    EXPECT_TRUE(has(out, "Too many failed attempts. Exiting."));
    EXPECT_FALSE(has(out, "Welcome"));
}

TEST_F(SystemTest, SYS53_EndOfInputInsideAMenu_ExitsCleanly) {
    const std::string out = run(w, login("stu1") + "2\n" + std::to_string(aid()) + "\nhalf typed answer");
    EXPECT_TRUE(has(out, "Goodbye."));
    EXPECT_FALSE(latest(w.stu1).has_value());
}

TEST_F(SystemTest, SYS54_SqlInjectionInLoginAndAnswer_IsHarmless) {
    const std::string out = run(w, login("' OR '1'='1' --", "x") + kQuit);
    EXPECT_FALSE(has(out, "Welcome"));
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "'); DROP TABLE submission; --"));
    EXPECT_EQ(latest(w.stu1)->getContent(), "'); DROP TABLE submission; --\n");
}

TEST_F(SystemTest, SYS55_AdminCreatesUsersAndEnrols_NewStudentCanThenSubmit) {
    const std::string out = run(w, login("admin", World::kAdminPassword) +
                                       "3\n4\nnewstu\nNew Student\nNewPass123\nS0099\n" +
                                       "4\n" + "0\n" + kQuit);
    EXPECT_TRUE(has(out, "Created user ["));
    EXPECT_TRUE(has(out, "newstu"));
    const auto created = w.users.findByUsername("newstu");
    ASSERT_TRUE(created.has_value());
    run(w, login("admin", World::kAdminPassword) + "6\n1\n" + std::to_string((*created)->getId()) + "\n0\n" + kQuit);
    at_(beforeDeadline());
    const std::string sub = run(w, login("newstu", "NewPass123") + submitWork(aid(), "hello") + "0\n" + kQuit);
    EXPECT_TRUE(has(sub, "Submitted as version 1"));
}

TEST_F(SystemTest, SYS56_WeakPasswordForNewUser_IsRejected) {
    const std::string out = run(w, login("admin", World::kAdminPassword) + "3\n4\nweak1\nWeak One\nabc\nS0100\n0\n" + kQuit);
    EXPECT_TRUE(has(out, "! "));
    EXPECT_FALSE(w.users.findByUsername("weak1").has_value());
}

TEST_F(SystemTest, SYS57_ChangePassword_OldNoLongerWorks) {
    run(w, login("stu1") + "3\n" + std::string(kPw) + "\nNewSecret456\n0\n" + kQuit);
    EXPECT_TRUE(has(run(w, login("stu1") + kQuit), "! Invalid username or password."));
    EXPECT_TRUE(has(run(w, login("stu1", "NewSecret456") + "0\n" + kQuit), "Welcome, Thabo Student"));
}

// ============================ Acceptance scenarios ==========================
// Each scenario is a story a real user of the system must be able to complete.

// ============================ CR1 Extension for the whole class ==============

TEST_F(SystemTest, SYS58_ClassExtension_ThroughTheMenu_MakesLateWorkOnTimeForEveryStudent) {  // CR1
    at_(beforeDeadline());
    const std::string when = w.deadline().plusSeconds(2 * kDay).toLocal(0);
    const std::string out = asLecturer(w, "4\n" + std::to_string(aid()) + "\n0\n" + when + "\n");
    EXPECT_TRUE(has(out, "Student user id (0 for the whole class): "));
    EXPECT_TRUE(has(out, "Extension granted to 2 student(s) until " + when + "."));
    at_(afterDeadline(24));
    EXPECT_FALSE(has(asStudent(w, "stu1", submitWork(aid(), "one")), "LATE"));
    EXPECT_FALSE(has(asStudent(w, "stu2", submitWork(aid(), "two")), "LATE"));
    EXPECT_EQ(latest(w.stu2)->getStatus(), SubmissionStatus::Submitted);
}

TEST_F(SystemTest, SYS59_StudentAssignmentList_ShowsTheExtendedDeadline) {  // CR1
    at_(beforeDeadline());
    const std::string original = w.deadline().toLocal(0);
    const std::string extended = w.deadline().plusSeconds(2 * kDay).toLocal(0);
    // Before any extension: only the original deadline, no mention of an extension.
    const std::string before = asStudent(w, "stu1", "1\n");
    EXPECT_TRUE(has(before, "due " + original + "  not submitted"));
    EXPECT_FALSE(has(before, "extended"));
    // One student's extension is shown to that student only.
    asLecturer(w, "4\n" + std::to_string(aid()) + "\n" + std::to_string(w.stu1->getId()) + "\n" + extended + "\n");
    const std::string mine = asStudent(w, "stu1", "1\n");
    EXPECT_TRUE(has(mine, "due " + extended + " (extended from " + original + ")  not submitted"));
    EXPECT_FALSE(has(asStudent(w, "stu2", "1\n"), "extended"));
    // A class extension is then shown to everybody, with the assignment's own deadline kept visible.
    const std::string classWide = w.deadline().plusSeconds(3 * kDay).toLocal(0);
    asLecturer(w, "4\n" + std::to_string(aid()) + "\n0\n" + classWide + "\n");
    EXPECT_TRUE(has(asStudent(w, "stu2", "1\n"), "due " + classWide + " (extended from " + original + ")"));
}

TEST_F(SystemTest, SYS60_ClassExtension_ByALecturerOfAnotherCourse_IsRefused) {  // CR1 exception flow
    at_(beforeDeadline());
    const std::string when = w.deadline().plusSeconds(2 * kDay).toLocal(0);
    const std::string out = asLecturer(w, "4\n" + std::to_string(aid()) + "\n0\n" + when + "\n", "lec2");
    EXPECT_TRUE(has(out, "! "));
    EXPECT_FALSE(w.assignmentRepo.findExtension(aid(), w.stu1->getId()).has_value());
}

TEST_F(SystemTest, SYS61_ClassExtensionKeepsALongerIndividualExtension_AndSaysSo) {  // CR1
    at_(beforeDeadline());
    const std::string longer = w.deadline().plusSeconds(3 * kDay).toLocal(0);
    const std::string shorter = w.deadline().plusSeconds(kDay).toLocal(0);
    asLecturer(w, "4\n" + std::to_string(aid()) + "\n" + std::to_string(w.stu1->getId()) + "\n" + longer + "\n");
    const std::string out = asLecturer(w, "4\n" + std::to_string(aid()) + "\n0\n" + shorter + "\n");
    EXPECT_TRUE(has(out, "Extension granted to 1 student(s) until " + shorter + ". 1 already had the same or a later "
                                                                              "extension and kept it."));
    EXPECT_EQ(w.assignmentRepo.findExtension(aid(), w.stu1->getId())->getNewDeadline(), w.deadline().plusSeconds(3 * kDay));
}

TEST_F(SystemTest, ACC01_StudentStory_SubmitThenSeeMarkOnlyAfterRelease) {
    at_(beforeDeadline());
    const std::string a = asStudent(w, "stu1", "1\n" + submitWork(aid(), "Essay on testing") + "1\n");
    EXPECT_TRUE(has(a, "not submitted"));
    EXPECT_TRUE(has(a, "Submitted as version 1"));
    EXPECT_TRUE(has(a, "submitted (v1), awaiting marks"));
    const int id = latest(w.stu1)->getId();
    asLecturer(w, markWork(id, 88, "Clear and well argued"));
    asHod(w, "5\n" + std::to_string(id) + "\ny\n8\n" + std::to_string(aid()) + "\n");
    EXPECT_TRUE(has(asStudent(w, "stu1", "1\n"), "MARK 88/100  feedback: Clear and well argued"));
}

TEST_F(SystemTest, ACC02_LecturerStory_CreateAssignmentThenMarkLateWork) {
    const std::string created = asLecturer(w, "2\n1\nAssignment 2\n2026-11-20 17:00\n50\n3\n1\n");
    EXPECT_TRUE(has(created, "Created assignment ["));
    EXPECT_TRUE(has(created, "Assignment 2  due 2026-11-20 17:00  max 50"));
    // Student hands in Assignment 1 one day late; lecturer marks and sees the penalty.
    at_(afterDeadline(5));
    asStudent(w, "stu1", submitWork(aid(), "late essay"));
    const int id = latest(w.stu1)->getId();
    const std::string marked = asLecturer(w, "5\n" + std::to_string(aid()) + "\n" + markWork(id, 90, "Good"));
    EXPECT_TRUE(has(marked, "LATE  late days 1"));
    EXPECT_TRUE(has(marked, "Marked. Raw 90, final after late penalty 80."));
}

TEST_F(SystemTest, ACC03_HodStory_ReviewAdjustApproveReleaseAndAudit) {
    at_(beforeDeadline());
    asStudent(w, "stu1", submitWork(aid(), "a1"));
    asStudent(w, "stu2", submitWork(aid(), "a2"));
    const int id1 = latest(w.stu1)->getId();
    const int id2 = latest(w.stu2)->getId();
    asLecturer(w, markWork(id1, 40, "Weak"));
    asLecturer(w, markWork(id2, 85, "Strong"));
    const std::string review = asHod(w, "3\n" + std::to_string(aid()) + "\n");
    EXPECT_TRUE(has(review, "final 40"));
    EXPECT_TRUE(has(review, "final 85"));
    asHod(w, "4\n" + std::to_string(id1) + "\n50\nMarking was harsher than the cohort\n"
             "6\n" + std::to_string(aid()) + "\ny\n8\n" + std::to_string(aid()) + "\n");
    EXPECT_TRUE(has(asStudent(w, "stu1", "1\n"), "MARK 50/100"));
    EXPECT_TRUE(has(asStudent(w, "stu2", "1\n"), "MARK 85/100"));
    const std::string audit = asHod(w, "9\n" + std::to_string(id1) + "\n");
    EXPECT_TRUE(has(audit, "Dr Head (hod)"));
    EXPECT_TRUE(has(audit, "Marking was harsher than the cohort"));
}

TEST_F(SystemTest, ACC04_AdministratorStory_SetUpTermFromNothing) {
    const std::string keys = login("admin", World::kAdminPassword) +
                             "1\nPhysics\n"                                  // department
                             "2\nPH1001\nMechanics\n3\n"                     // course in dept 3
                             "3\n2\nlecphys\nDr Newton\nNewton1234\n3\n"     // lecturer
                             "7\n0\n" + kQuit;
    const std::string out = run(w, keys);
    EXPECT_TRUE(has(out, "Created department ["));
    EXPECT_TRUE(has(out, "Created course ["));
    EXPECT_TRUE(has(out, "PH1001  Mechanics"));
    EXPECT_TRUE(w.users.findByUsername("lecphys").has_value());
}

TEST_F(SystemTest, ACC05_AuditTrailAndMarksSurviveRestart_FileDatabase) {
    char path[] = "/tmp/lms_acc05_XXXXXX";
    const int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    close(fd);
    int submissionId = 0;
    {
        Database db(path);
        SqliteUserRepository users(db);
        SqliteDepartmentRepository deps(db);
        SqliteCourseRepository courses(db);
        SqliteAssignmentRepository asg(db);
        SqliteSubmissionRepository subs(db);
        SqliteAuditRepository audit(db);
        FixedClock clock(at(kStart));
        DailyPercentPenalty penalty(10, 3);
        CourseService cs(users, deps, courses);
        AdminService admin(users, deps, courses);
        AuthService auth(users);
        AssignmentService assignments(asg, courses, cs, clock, db);
        AuditService auditSvc(audit, clock);
        SubmissionService submissions(subs, asg, courses, clock, db);
        MarkingService marking(subs, asg, cs, penalty, auditSvc, db);
        ModerationService moderation(subs, asg, cs, auditSvc, db);
        auto adminU = auth.createInitialAdministrator(UserData{"admin", "Admin", "Admin12345", "", 0});
        auto dep = admin.createDepartment(*adminU, "CS");
        auto course = admin.createCourse(*adminU, "CS1", "Intro", dep.getId());
        auto lec = admin.createUser(*adminU, Role::Lecturer, UserData{"lec", "Lec", "Lect123456", "", dep.getId()});
        auto stu = admin.createUser(*adminU, Role::Student, UserData{"stu", "Stu", "Stud123456", "S1", 0});
        admin.assignLecturer(*adminU, course.getId(), lec->getId());
        admin.enrolStudent(*adminU, course.getId(), stu->getId());
        auto a = assignments.createAssignment(*lec, course.getId(), "A1", clock.now().plusSeconds(kDay), 100);
        submissionId = submissions.submit(*stu, a.getId(), "persisted answer").getId();
        marking.mark(*lec, submissionId, 66, "kept", "");
    }  // program "closed"
    {
        Database db(path);  // program "restarted"
        SqliteSubmissionRepository subs(db);
        SqliteAuditRepository audit(db);
        const auto s = subs.findById(submissionId);
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->getContent(), "persisted answer");
        EXPECT_EQ(*s->getFinalMark(), 66);
        EXPECT_EQ(audit.findBySubmission(submissionId).size(), 1u);
    }
    std::remove(path);
}

}  // namespace
}  // namespace lms::test
