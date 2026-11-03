// Unit and integration tests for the service (business-logic) layer.
// Each test name says which business rule or use-case flow it checks.
#include "TestSupport.hpp"
#include "service/AccessControl.hpp"

using namespace lms;
using namespace lms::test;

namespace {
const DateTime kNow = at(kStart);
DateTime afterDeadline(World& w, int seconds) { return w.deadline().plusSeconds(seconds); }

// Repository stubs that fail on save, to prove transactions roll back (E4/E5 flows).
class FailingAuditRepository : public IAuditRepository {
public:
    std::optional<AuditEntry> findById(int) const override { return std::nullopt; }
    void save(AuditEntry&) override { throw DatabaseError("simulated disk failure"); }
    std::vector<AuditEntry> findBySubmission(int) const override { return {}; }
};

class FailingSubmissionRepository : public ISubmissionRepository {
public:
    explicit FailingSubmissionRepository(ISubmissionRepository& real) : real_(real) {}
    std::optional<Submission> findById(int id) const override { return real_.findById(id); }
    void save(Submission&) override { throw DatabaseError("simulated disk failure"); }
    std::vector<Submission> findByAssignment(int a) const override { return real_.findByAssignment(a); }
    std::vector<Submission> findCurrentByAssignment(int a) const override { return real_.findCurrentByAssignment(a); }
    std::optional<Submission> findLatest(int a, int s) const override { return real_.findLatest(a, s); }
    std::vector<Submission> findCurrentByStudent(int s) const override { return real_.findCurrentByStudent(s); }
private:
    ISubmissionRepository& real_;
};

int auditCount(World& w) {
    Statement q(w.db, "SELECT COUNT(*) FROM audit_entry");
    q.step();
    return q.columnInt(0);
}
}  // namespace

// ===== AccessControl =====================================================================

TEST(AccessControl, DeniesWithAReadableMessage) {
    World w;
    EXPECT_NO_THROW(requirePermission(*w.lec, Permission::MarkSubmission, "mark"));
    EXPECT_THROW_MSG(requirePermission(*w.stu1, Permission::MarkSubmission, "mark submissions"), AuthorizationError,
                     "not allowed to mark submissions");
}

// ===== AuthService =======================================================================

class AuthTest : public ::testing::Test {
protected:
    Database db{":memory:"};
    SqliteUserRepository users{db};
    AuthService auth{users};
    UserPtr admin;
    void SetUp() override {
        ASSERT_TRUE(auth.noUsersExist());
        admin = auth.createInitialAdministrator(UserData{"admin", "Admin", "Admin12345", "", 0});
    }
};

TEST_F(AuthTest, FirstAdministratorCanBeCreatedOnlyOnce) {
    EXPECT_FALSE(auth.noUsersExist());
    EXPECT_NE(admin->getId(), 0);
    EXPECT_THROW_MSG(auth.createInitialAdministrator(UserData{"admin2", "Two", "Admin12345", "", 0}), BusinessRuleError,
                     "already exists");
}

TEST_F(AuthTest, LoginSucceedsWithCorrectCredentialsAndTrimsTheUsername) {
    EXPECT_EQ(auth.login("admin", "Admin12345")->getId(), admin->getId());
    EXPECT_EQ(auth.login("  admin  ", "Admin12345")->getRole(), Role::Admin);
}

TEST_F(AuthTest, WrongPasswordAndUnknownUserGiveTheSameMessage) {
    std::string m1, m2;
    try { auth.login("admin", "wrong"); } catch (const AuthenticationError& e) { m1 = e.what(); }
    try { auth.login("nobody", "wrong"); } catch (const AuthenticationError& e) { m2 = e.what(); }
    EXPECT_FALSE(m1.empty());
    EXPECT_EQ(m1, m2);  // does not reveal which usernames exist
}

TEST_F(AuthTest, InvalidLoginInputsAreRejectedSafely) {
    EXPECT_THROW(auth.login("", ""), AuthenticationError);
    EXPECT_THROW(auth.login("admin", ""), AuthenticationError);
    EXPECT_THROW(auth.login(std::string(65, 'a'), "x"), AuthenticationError);    // username too long
    EXPECT_THROW(auth.login("admin", std::string(129, 'a')), AuthenticationError);  // password too long
    EXPECT_THROW(auth.login("' OR '1'='1", "' OR '1'='1"), AuthenticationError);
    EXPECT_THROW(auth.login("admin'--", "x"), AuthenticationError);
}

TEST_F(AuthTest, ChangePasswordPersistsAndInvalidatesTheOldPassword) {
    UserPtr u = auth.login("admin", "Admin12345");
    auth.changePassword(u, "Admin12345", "NewPass99");
    EXPECT_THROW(auth.login("admin", "Admin12345"), AuthenticationError);
    EXPECT_NO_THROW(auth.login("admin", "NewPass99"));
}

TEST_F(AuthTest, FailedPasswordChangeLeavesStoredPasswordAndSessionUnchanged) {
    UserPtr u = auth.login("admin", "Admin12345");
    EXPECT_THROW(auth.changePassword(u, "wrong", "NewPass99"), AuthenticationError);
    EXPECT_THROW(auth.changePassword(u, "Admin12345", "weak"), ValidationError);
    EXPECT_TRUE(u->verifyPassword("Admin12345"));
    EXPECT_NO_THROW(auth.login("admin", "Admin12345"));
}

// ===== AdminService ======================================================================

TEST(AdminService, OnlyAdministratorsMayUseAdministratorFunctions) {
    World w;
    for (UserPtr actor : {w.lec, w.hod, w.stu1}) {
        EXPECT_THROW(w.admin.createUser(*actor, Role::Admin, UserData{"newadm", "N", "Passw0rd1", "", 0}), AuthorizationError);
        EXPECT_THROW(w.admin.listUsers(*actor), AuthorizationError);
        EXPECT_THROW(w.admin.createDepartment(*actor, "X"), AuthorizationError);
        EXPECT_THROW(w.admin.createCourse(*actor, "C1", "T", w.dept.getId()), AuthorizationError);
        EXPECT_THROW(w.admin.assignLecturer(*actor, w.course.getId(), w.lec2->getId()), AuthorizationError);
        EXPECT_THROW(w.admin.enrolStudent(*actor, w.course.getId(), w.stu3->getId()), AuthorizationError);
    }
}

TEST(AdminService, CreateUserValidatesAndRejectsDuplicates) {
    World w;
    UserPtr s = w.admin.createUser(*w.adminU, Role::Student, UserData{"newstu", "New S", "Passw0rd1", "S0100", 0});
    EXPECT_NE(s->getId(), 0);
    EXPECT_TRUE(w.users.findByUsername("newstu").has_value());
    EXPECT_THROW_MSG(w.admin.createUser(*w.adminU, Role::Student, UserData{"newstu", "X", "Passw0rd1", "S0101", 0}), ValidationError, "username is already taken");
    EXPECT_THROW_MSG(w.admin.createUser(*w.adminU, Role::Student, UserData{"other1", "X", "Passw0rd1", "S0100", 0}), ValidationError, "student number is already in use");
    EXPECT_THROW_MSG(w.admin.createUser(*w.adminU, Role::Lecturer, UserData{"lecnew", "X", "Passw0rd1", "", 9999}), NotFoundError, "department does not exist");
    EXPECT_THROW_MSG(w.admin.createUser(*w.adminU, Role::Hod, UserData{"hodnew", "X", "Passw0rd1", "", w.dept.getId()}), BusinessRuleError, "already has a Head");
    EXPECT_THROW(w.admin.createUser(*w.adminU, Role::Student, UserData{"weakpw", "X", "weak", "S0102", 0}), ValidationError);
    EXPECT_EQ(w.users.countAll(), 9);  // only the one valid user was added
    EXPECT_EQ(w.admin.listUsers(*w.adminU).size(), 9u);
}

TEST(AdminService, DepartmentsAndCourses) {
    World w;
    EXPECT_NE(w.admin.createDepartment(*w.adminU, "  Physics  ").getId(), 0);
    EXPECT_EQ(w.departments.findByName("Physics")->getName(), "Physics");  // trimmed
    EXPECT_THROW_MSG(w.admin.createDepartment(*w.adminU, "Physics"), ValidationError, "already exists");
    EXPECT_THROW(w.admin.createDepartment(*w.adminU, "   "), ValidationError);
    EXPECT_THROW(w.admin.createDepartment(*w.adminU, std::string(101, 'x')), ValidationError);

    EXPECT_NE(w.admin.createCourse(*w.adminU, "MA1001", "Calculus", w.deptOther.getId()).getId(), 0);
    EXPECT_THROW_MSG(w.admin.createCourse(*w.adminU, "MA1001", "Again", w.deptOther.getId()), ValidationError, "code already exists");
    EXPECT_THROW_MSG(w.admin.createCourse(*w.adminU, "MA1002", "X", 9999), NotFoundError, "department does not exist");
    EXPECT_THROW(w.admin.createCourse(*w.adminU, "", "X", w.dept.getId()), ValidationError);
    EXPECT_THROW(w.admin.createCourse(*w.adminU, "C", "", w.dept.getId()), ValidationError);
    EXPECT_THROW(w.admin.createCourse(*w.adminU, std::string(21, 'C'), "T", w.dept.getId()), ValidationError);
}

TEST(AdminService, AssignLecturerRules) {
    World w;
    UserPtr mathLec = w.quick(Role::Lecturer, "mlec", "Math Lecturer", w.deptOther.getId(), "");
    EXPECT_THROW_MSG(w.admin.assignLecturer(*w.adminU, 9999, w.lec2->getId()), NotFoundError, "no course");
    EXPECT_THROW_MSG(w.admin.assignLecturer(*w.adminU, w.course.getId(), w.stu1->getId()), NotFoundError, "no Lecturer");
    EXPECT_THROW_MSG(w.admin.assignLecturer(*w.adminU, w.course.getId(), 9999), NotFoundError, "no Lecturer");
    EXPECT_THROW_MSG(w.admin.assignLecturer(*w.adminU, w.course.getId(), mathLec->getId()), BusinessRuleError, "different department");
    EXPECT_THROW_MSG(w.admin.assignLecturer(*w.adminU, w.course.getId(), w.lec->getId()), BusinessRuleError, "already teaches");
    EXPECT_NO_THROW(w.admin.assignLecturer(*w.adminU, w.course.getId(), w.lec2->getId()));
    EXPECT_TRUE(w.courses.isLecturerOf(w.course.getId(), w.lec2->getId()));
}

TEST(AdminService, EnrolStudentRules) {
    World w;
    EXPECT_THROW_MSG(w.admin.enrolStudent(*w.adminU, 9999, w.stu3->getId()), NotFoundError, "no course");
    EXPECT_THROW_MSG(w.admin.enrolStudent(*w.adminU, w.course.getId(), w.lec->getId()), NotFoundError, "no Student");
    EXPECT_THROW_MSG(w.admin.enrolStudent(*w.adminU, w.course.getId(), w.stu1->getId()), BusinessRuleError, "already enrolled");
    EXPECT_NO_THROW(w.admin.enrolStudent(*w.adminU, w.course.getId(), w.stu3->getId()));
    EXPECT_TRUE(w.courses.isEnrolled(w.course.getId(), w.stu3->getId()));
}

// ===== CourseService =====================================================================

TEST(CourseService, EachRoleSeesOnlyItsOwnCourses) {
    World w;
    Course other = w.admin.createCourse(*w.adminU, "MA1001", "Calculus", w.deptOther.getId());
    EXPECT_EQ(w.courseSvc.coursesFor(*w.adminU).size(), 2u);
    EXPECT_EQ(w.courseSvc.coursesFor(*w.lec).size(), 1u);
    EXPECT_EQ(w.courseSvc.coursesFor(*w.lec2).size(), 0u);
    EXPECT_EQ(w.courseSvc.coursesFor(*w.hod).size(), 1u);
    EXPECT_EQ(w.courseSvc.coursesFor(*w.hod).front().getCode(), "CS4433");
    EXPECT_EQ(w.courseSvc.coursesFor(*w.hodOther).front().getId(), other.getId());
    EXPECT_EQ(w.courseSvc.coursesFor(*w.stu1).size(), 1u);
    EXPECT_EQ(w.courseSvc.coursesFor(*w.stu3).size(), 0u);
}

TEST(CourseService, RequireCanSeeMatrix) {
    World w;
    EXPECT_NO_THROW(w.courseSvc.requireCanSee(*w.adminU, w.course));
    EXPECT_NO_THROW(w.courseSvc.requireCanSee(*w.lec, w.course));
    EXPECT_NO_THROW(w.courseSvc.requireCanSee(*w.hod, w.course));
    EXPECT_NO_THROW(w.courseSvc.requireCanSee(*w.stu1, w.course));
    EXPECT_THROW_MSG(w.courseSvc.requireCanSee(*w.lec2, w.course), AuthorizationError, "do not teach");
    EXPECT_THROW_MSG(w.courseSvc.requireCanSee(*w.hodOther, w.course), AuthorizationError, "not in your department");
    EXPECT_THROW_MSG(w.courseSvc.requireCanSee(*w.stu3, w.course), AuthorizationError, "not enrolled");
    EXPECT_THROW_MSG(w.courseSvc.requireCourse(9999), NotFoundError, "no course");
}

TEST(CourseService, StudentListIsForStaffOfThatCourseOnly) {
    World w;
    EXPECT_EQ(w.courseSvc.studentsOf(*w.adminU, w.course.getId()).size(), 2u);
    EXPECT_EQ(w.courseSvc.studentsOf(*w.lec, w.course.getId()).size(), 2u);
    EXPECT_EQ(w.courseSvc.studentsOf(*w.hod, w.course.getId()).size(), 2u);
    EXPECT_THROW(w.courseSvc.studentsOf(*w.lec2, w.course.getId()), AuthorizationError);
    EXPECT_THROW(w.courseSvc.studentsOf(*w.hodOther, w.course.getId()), AuthorizationError);
    EXPECT_THROW(w.courseSvc.studentsOf(*w.stu1, w.course.getId()), AuthorizationError);
    EXPECT_THROW(w.courseSvc.studentsOf(*w.adminU, 9999), NotFoundError);
    EXPECT_EQ(w.courseSvc.departments().size(), 2u);
}

// ===== AssignmentService =================================================================

TEST(AssignmentService, CreateAssignmentPermissionsAndValidation) {
    World w;
    const int c = w.course.getId();
    const DateTime future = kNow.plusSeconds(kDay);
    EXPECT_THROW(w.assignments.createAssignment(*w.stu1, c, "T", future, 100), AuthorizationError);
    EXPECT_THROW(w.assignments.createAssignment(*w.hod, c, "T", future, 100), AuthorizationError);
    EXPECT_THROW_MSG(w.assignments.createAssignment(*w.lec2, c, "T", future, 100), AuthorizationError, "do not teach");
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, 9999, "T", future, 100), NotFoundError);
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, c, "   ", future, 100), ValidationError);
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, c, std::string(101, 't'), future, 100), ValidationError);
    EXPECT_NO_THROW(w.assignments.createAssignment(*w.lec, c, std::string(100, 't'), future, 100));  // boundary
}

TEST(AssignmentService, DeadlineMustBeInTheFuture) {
    World w;
    const int c = w.course.getId();
    EXPECT_THROW_MSG(w.assignments.createAssignment(*w.lec, c, "A", kNow.plusSeconds(-1), 100), ValidationError, "future");
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, c, "B", kNow, 100), ValidationError);  // equal to now
    EXPECT_NO_THROW(w.assignments.createAssignment(*w.lec, c, "C", kNow.plusSeconds(1), 100));  // one second ahead
}

TEST(AssignmentService, MaximumMarkBoundaries) {
    World w;
    const int c = w.course.getId();
    const DateTime d = kNow.plusSeconds(kDay);
    EXPECT_THROW_MSG(w.assignments.createAssignment(*w.lec, c, "M0", d, 0), ValidationError, "between 1 and 1000");
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, c, "Mneg", d, -5), ValidationError);
    EXPECT_NO_THROW(w.assignments.createAssignment(*w.lec, c, "M1", d, 1));
    EXPECT_NO_THROW(w.assignments.createAssignment(*w.lec, c, "M1000", d, 1000));
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, c, "M1001", d, 1001), ValidationError);
}

TEST(AssignmentService, DuplicateTitleInSameCourseIsRejected) {
    World w;
    EXPECT_THROW(w.assignments.createAssignment(*w.lec, w.course.getId(), "Assignment 1", kNow.plusSeconds(kDay), 100), DatabaseError);
}

TEST(AssignmentService, GrantExtensionRules) {
    World w;
    const int a = w.assignment.getId();
    const DateTime ok = w.deadline().plusSeconds(kDay);
    EXPECT_THROW(w.assignments.grantExtension(*w.stu1, a, w.stu1->getId(), ok), AuthorizationError);
    EXPECT_THROW(w.assignments.grantExtension(*w.hod, a, w.stu1->getId(), ok), AuthorizationError);
    EXPECT_THROW(w.assignments.grantExtension(*w.lec2, a, w.stu1->getId(), ok), AuthorizationError);
    EXPECT_THROW(w.assignments.grantExtension(*w.lec, 9999, w.stu1->getId(), ok), NotFoundError);
    EXPECT_THROW_MSG(w.assignments.grantExtension(*w.lec, a, w.stu3->getId(), ok), BusinessRuleError, "not enrolled");
    EXPECT_THROW_MSG(w.assignments.grantExtension(*w.lec, a, w.stu1->getId(), w.deadline()), ValidationError, "later than");
    EXPECT_THROW(w.assignments.grantExtension(*w.lec, a, w.stu1->getId(), w.deadline().plusSeconds(-1)), ValidationError);
    EXPECT_NO_THROW(w.assignments.grantExtension(*w.lec, a, w.stu1->getId(), w.deadline().plusSeconds(1)));  // boundary
    Extension e = w.assignments.grantExtension(*w.lec, a, w.stu1->getId(), ok);  // replaces the earlier one
    EXPECT_EQ(w.assignmentRepo.findExtension(a, w.stu1->getId())->getNewDeadline(), ok);
    EXPECT_EQ(e.getGrantedBy(), w.lec->getId());
}

TEST(AssignmentService, VisibilityFollowsCourseMembership) {
    World w;
    EXPECT_EQ(w.assignments.assignmentsForCourse(*w.stu1, w.course.getId()).size(), 1u);
    EXPECT_EQ(w.assignments.assignmentsForCourse(*w.hod, w.course.getId()).size(), 1u);
    EXPECT_EQ(w.assignments.assignmentsForCourse(*w.adminU, w.course.getId()).size(), 1u);
    EXPECT_THROW(w.assignments.assignmentsForCourse(*w.stu3, w.course.getId()), AuthorizationError);
    EXPECT_THROW(w.assignments.assignmentsForCourse(*w.lec2, w.course.getId()), AuthorizationError);
    EXPECT_THROW(w.assignments.assignmentsForCourse(*w.lec, 9999), NotFoundError);
    EXPECT_EQ(w.assignments.getAssignment(*w.stu1, w.assignment.getId()).getTitle(), "Assignment 1");
    EXPECT_THROW(w.assignments.getAssignment(*w.stu3, w.assignment.getId()), AuthorizationError);
}

// ===== SubmissionService: UC-01 ==========================================================

struct LateCase { int offsetSeconds; int expectedLateDays; };
class LateDaysBoundary : public ::testing::TestWithParam<LateCase> {};

TEST_P(LateDaysBoundary, ComputeLateDays) {
    const DateTime deadline = at("2026-11-08T08:00:00Z");
    EXPECT_EQ(SubmissionService::computeLateDays(deadline, deadline.plusSeconds(GetParam().offsetSeconds)),
              GetParam().expectedLateDays);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, LateDaysBoundary, ::testing::Values(
    LateCase{-100 * kDay, 0}, LateCase{-1, 0}, LateCase{0, 0},
    LateCase{1, 1}, LateCase{kDay - 1, 1}, LateCase{kDay, 1},
    LateCase{kDay + 1, 2}, LateCase{2 * kDay, 2}, LateCase{2 * kDay + 1, 3},
    LateCase{3 * kDay, 3}, LateCase{3 * kDay + 1, 4}, LateCase{30 * kDay, 30}),
    [](const ::testing::TestParamInfo<LateCase>& info) {
        const int off = info.param.offsetSeconds;
        return std::string(off < 0 ? "minus_" : "plus_") + std::to_string(off < 0 ? -off : off) + "_seconds";
    });

TEST(UC01_Submit, OnTimeSubmissionIsSubmittedWithNoLateDays) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    EXPECT_EQ(s.getStatus(), SubmissionStatus::Submitted);
    EXPECT_EQ(s.getLateDays(), 0);
    EXPECT_EQ(s.getVersion(), 1);
    EXPECT_NE(s.getId(), 0);
    EXPECT_EQ(s.getSubmittedAt(), kNow);
}

TEST(UC01_Submit, SubmissionAtExactlyTheDeadlineIsOnTime) {
    World w;
    EXPECT_EQ(w.submitAs(w.stu1, w.deadline()).getStatus(), SubmissionStatus::Submitted);
}

TEST(UC01_A2_A3_Submit, LateSubmissionsAreAlwaysAcceptedAndMarkedLate) {
    World w;
    Submission one = w.submitAs(w.stu1, afterDeadline(w, 1));
    EXPECT_EQ(one.getStatus(), SubmissionStatus::Late);
    EXPECT_EQ(one.getLateDays(), 1);
    Submission three = w.submitAs(w.stu2, afterDeadline(w, 3 * kDay));
    EXPECT_EQ(three.getLateDays(), 3);
    EXPECT_EQ(three.getStatus(), SubmissionStatus::Late);

    // A3: beyond the late window the work is still stored (never silently rejected).
    World v;
    Submission beyond = v.submitAs(v.stu1, afterDeadline(v, 3 * kDay + 1));
    EXPECT_EQ(beyond.getStatus(), SubmissionStatus::Late);
    EXPECT_EQ(beyond.getLateDays(), 4);
    EXPECT_TRUE(v.submissionRepo.findLatest(v.assignment.getId(), v.stu1->getId()).has_value());
}

TEST(UC01_A1_Submit, ResubmissionBeforeTheDeadlineCreatesANewVersionAndKeepsHistory) {
    World w;
    w.submitAs(w.stu1, kNow, "first");
    Submission second = w.submitAs(w.stu1, kNow.plusSeconds(3600), "second");
    EXPECT_EQ(second.getVersion(), 2);
    EXPECT_EQ(second.getStatus(), SubmissionStatus::Submitted);
    Submission third = w.submitAs(w.stu1, w.deadline(), "third");  // exactly at the deadline
    EXPECT_EQ(third.getVersion(), 3);
    EXPECT_EQ(w.submissionRepo.findByAssignment(w.assignment.getId()).size(), 3u);
    EXPECT_EQ(w.submissionRepo.findLatest(w.assignment.getId(), w.stu1->getId())->getContent(), "third");
}

TEST(UC01_E3_Submit, CannotResubmitAfterTheDeadlineOrAfterALateSubmissionOrAfterMarking) {
    {
        World w;  // first on time, then try after the deadline
        w.submitAs(w.stu1, kNow);
        EXPECT_THROW_MSG(w.submitAs(w.stu1, afterDeadline(w, 1)), BusinessRuleError, "deadline has passed");
        EXPECT_EQ(w.submissionRepo.findByAssignment(w.assignment.getId()).size(), 1u);
    }
    {
        World w;  // first one is late, so replacing it is refused
        w.submitAs(w.stu1, afterDeadline(w, 10));
        EXPECT_THROW_MSG(w.submitAs(w.stu1, afterDeadline(w, 20)), BusinessRuleError, "can no longer be replaced");
    }
    {
        World w;  // marking has started
        Submission s = w.submitAs(w.stu1, kNow);
        w.marking.mark(*w.lec, s.getId(), 60, "ok", "");
        EXPECT_THROW_MSG(w.submitAs(w.stu1, kNow.plusSeconds(60)), BusinessRuleError, "can no longer be replaced");
    }
}

TEST(UC01_E1_Submit, InvalidContentIsRejectedAndNothingIsStored) {
    World w;
    EXPECT_THROW_MSG(w.submitAs(w.stu1, kNow, ""), ValidationError, "must not be empty");
    EXPECT_THROW(w.submitAs(w.stu1, kNow, "   \n\t "), ValidationError);
    EXPECT_NO_THROW(w.submitAs(w.stu1, kNow, std::string(SubmissionService::kMaxContentBytes, 'x')));  // boundary
    EXPECT_THROW_MSG(w.submitAs(w.stu2, kNow, std::string(SubmissionService::kMaxContentBytes + 1, 'x')), ValidationError, "too large");
    EXPECT_EQ(w.submissionRepo.findByAssignment(w.assignment.getId()).size(), 1u);
}

TEST(UC01_E2_Submit, NotEnrolledOrWrongRoleOrUnknownAssignmentIsDenied) {
    World w;
    EXPECT_THROW_MSG(w.submitAs(w.stu3, kNow), AuthorizationError, "not enrolled");
    EXPECT_THROW(w.submissions.submit(*w.lec, w.assignment.getId(), "x"), AuthorizationError);
    EXPECT_THROW(w.submissions.submit(*w.hod, w.assignment.getId(), "x"), AuthorizationError);
    EXPECT_THROW_MSG(w.submissions.submit(*w.stu1, 9999, "x"), NotFoundError, "no assignment");
    EXPECT_TRUE(w.submissionRepo.findByAssignment(w.assignment.getId()).empty());
}

TEST(UC01_E4_Submit, DatabaseFailureLeavesThePreviousVersionUntouched) {
    World w;
    w.submitAs(w.stu1, kNow, "good version");
    FailingSubmissionRepository failing(w.submissionRepo);
    SubmissionService broken(failing, w.assignmentRepo, w.courses, w.clock, w.db);
    EXPECT_THROW_MSG(broken.submit(*w.stu1, w.assignment.getId(), "new version"), DatabaseError, "simulated");
    auto all = w.submissionRepo.findByAssignment(w.assignment.getId());
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].getContent(), "good version");
    // The system keeps working after the failure.
    EXPECT_NO_THROW(w.submitAs(w.stu1, kNow.plusSeconds(10), "retry"));
}

TEST(UC01_BR01_Submit, ExtensionChangesTheEffectiveDeadline) {
    World w;
    const DateTime extended = w.deadline().plusSeconds(2 * kDay);
    w.assignments.grantExtension(*w.lec, w.assignment.getId(), w.stu1->getId(), extended);
    EXPECT_EQ(w.submissions.effectiveDeadlineFor(w.assignment, w.stu1->getId()), extended);
    EXPECT_EQ(w.submissions.effectiveDeadlineFor(w.assignment, w.stu2->getId()), w.deadline());  // others unaffected

    // Between the original deadline and the extension: not late for stu1, but late for stu2.
    const DateTime between = w.deadline().plusSeconds(kDay);
    EXPECT_EQ(w.submitAs(w.stu1, between).getLateDays(), 0);
    EXPECT_EQ(w.submitAs(w.stu2, between).getLateDays(), 1);
    // Late days are counted from the extension for stu1 only: 1 s after the extension is 1 day late.
    World v;
    v.assignments.grantExtension(*v.lec, v.assignment.getId(), v.stu1->getId(), v.deadline().plusSeconds(2 * kDay));
    EXPECT_EQ(v.submitAs(v.stu1, v.deadline().plusSeconds(2 * kDay + 1)).getLateDays(), 1);
    // The extension also keeps resubmission open until the extended deadline.
    World x;
    x.assignments.grantExtension(*x.lec, x.assignment.getId(), x.stu1->getId(), x.deadline().plusSeconds(2 * kDay));
    x.submitAs(x.stu1, x.deadline().plusSeconds(-10));
    EXPECT_NO_THROW(x.submitAs(x.stu1, x.deadline().plusSeconds(kDay)));
}

TEST(UC01_AssignmentList, ShowsEachAssignmentWithTheStudentsCurrentSubmission) {
    World w;
    w.submitAs(w.stu1, kNow, "v1");
    w.submitAs(w.stu1, kNow.plusSeconds(5), "v2");
    auto rows = w.submissions.assignmentsFor(*w.stu1);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].courseCode, "CS4433");
    ASSERT_TRUE(rows[0].current.has_value());
    EXPECT_EQ(rows[0].current->getVersion(), 2);
    EXPECT_FALSE(w.submissions.assignmentsFor(*w.stu2)[0].current.has_value());
    EXPECT_TRUE(w.submissions.assignmentsFor(*w.stu3).empty());
    EXPECT_THROW(w.submissions.assignmentsFor(*w.lec), AuthorizationError);
}

// ===== MarkingService: UC-02 =============================================================

TEST(UC02_Mark, OnTimeWorkKeepsTheRawMarkAndWritesOneAuditEntry) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    Submission marked = w.marking.mark(*w.lec, s.getId(), 80, "Good work", "");
    EXPECT_EQ(marked.getStatus(), SubmissionStatus::Marked);
    EXPECT_EQ(*marked.getRawMark(), 80);
    EXPECT_EQ(*marked.getFinalMark(), 80);
    EXPECT_EQ(marked.getFeedback(), "Good work");
    auto log = w.audit.history(s.getId());
    ASSERT_EQ(log.size(), 1u);
    EXPECT_FALSE(log[0].getOldValue().has_value());
    EXPECT_EQ(log[0].getNewValue(), 80);
    EXPECT_EQ(log[0].getChangedBy(), w.lec->getId());
    EXPECT_FALSE(log[0].getReason().empty());
}

TEST(UC02_A2_A3_Mark, LatePenaltyIsAppliedAndZeroBeyondTheWindow) {
    struct Row { int lateOffset; int expectedFinal; };
    for (Row r : {Row{1, 70}, Row{kDay + 1, 60}, Row{2 * kDay + 1, 50}, Row{3 * kDay, 50}, Row{3 * kDay + 1, 0}}) {
        World w;
        Submission s = w.submitAs(w.stu1, afterDeadline(w, r.lateOffset));
        Submission m = w.marking.mark(*w.lec, s.getId(), 80, "fb", "");
        EXPECT_EQ(*m.getRawMark(), 80) << r.lateOffset;
        EXPECT_EQ(*m.getFinalMark(), r.expectedFinal) << r.lateOffset;
    }
}

TEST(UC02_E1_Mark, RawMarkMustBeBetweenZeroAndTheMaximum) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), -1, "fb", ""), ValidationError, "between 0 and 100");
    EXPECT_THROW(w.marking.mark(*w.lec, s.getId(), 101, "fb", ""), ValidationError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Submitted);
    EXPECT_EQ(auditCount(w), 0);
    EXPECT_EQ(*w.marking.mark(*w.lec, s.getId(), 0, "fb", "").getRawMark(), 0);      // lower boundary
    EXPECT_EQ(*w.marking.mark(*w.lec, s.getId(), 100, "fb", "because").getRawMark(), 100);  // upper boundary
}

TEST(UC02_E2_Mark, FeedbackIsMandatory) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), 50, "", ""), ValidationError, "feedback");
    EXPECT_THROW(w.marking.mark(*w.lec, s.getId(), 50, "   ", ""), ValidationError);
    EXPECT_THROW(w.marking.mark(*w.lec, s.getId(), 50, std::string(5001, 'f'), ""), ValidationError);
    EXPECT_NO_THROW(w.marking.mark(*w.lec, s.getId(), 50, std::string(5000, 'f'), ""));
}

TEST(UC02_A1_Mark, ChangingAnExistingMarkNeedsAReasonAndKeepsTheOldValueInTheLog) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    w.marking.mark(*w.lec, s.getId(), 60, "fb", "");
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), 65, "fb", ""), ValidationError, "reason is required");
    EXPECT_THROW(w.marking.mark(*w.lec, s.getId(), 65, "fb", "   "), ValidationError);
    EXPECT_EQ(*w.submissionRepo.findById(s.getId())->getFinalMark(), 60);  // unchanged
    w.marking.mark(*w.lec, s.getId(), 65, "fb", "Missed a correct answer");
    auto log = w.audit.history(s.getId());
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(*log[1].getOldValue(), 60);
    EXPECT_EQ(log[1].getNewValue(), 65);
    EXPECT_NE(log[1].getReason().find("Missed a correct answer"), std::string::npos);
}

// Code review finding R-02: the reason was only checked after the automatic suffix had been added, so the limit
// that applied depended on the suffix and the message named a different number (600) from the real one.
TEST(UC02_A1_Mark, TheReasonForChangingAMarkHasOneClearLimitOf500Characters) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    w.marking.mark(*w.lec, s.getId(), 60, "fb", "");
    EXPECT_NO_THROW(w.marking.mark(*w.lec, s.getId(), 61, "fb", std::string(500, 'r')));
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), 62, "fb", std::string(501, 'r')), ValidationError,
                     "at most 500 characters");
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), 62, "fb", std::string(590, 'r')), ValidationError,
                     "at most 500 characters");
    EXPECT_EQ(*w.submissionRepo.findById(s.getId())->getFinalMark(), 61);  // rejected attempts changed nothing
    EXPECT_EQ(w.audit.history(s.getId()).size(), 2u);
}

TEST(UC02_E3_Mark, ModeratedOrReleasedWorkCannotBeChangedByTheLecturer) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    w.marking.mark(*w.lec, s.getId(), 60, "fb", "");
    w.moderation.approve(*w.hod, s.getId());
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, s.getId(), 70, "fb", "reason"), BusinessRuleError, "only the Head of Department");
    w.moderation.release(*w.hod, w.assignment.getId());
    EXPECT_THROW(w.marking.mark(*w.lec, s.getId(), 70, "fb", "reason"), BusinessRuleError);
    EXPECT_EQ(*w.submissionRepo.findById(s.getId())->getFinalMark(), 60);
}

TEST(UC02_E4_Mark, OnlyTheCourseLecturerMayMark) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    EXPECT_THROW_MSG(w.marking.mark(*w.lec2, s.getId(), 50, "fb", ""), AuthorizationError, "do not teach");
    EXPECT_THROW(w.marking.mark(*w.stu1, s.getId(), 50, "fb", ""), AuthorizationError);
    EXPECT_THROW(w.marking.mark(*w.hod, s.getId(), 50, "fb", ""), AuthorizationError);
    EXPECT_THROW(w.marking.mark(*w.adminU, s.getId(), 50, "fb", ""), AuthorizationError);
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, 9999, 50, "fb", ""), NotFoundError, "no submission");
    EXPECT_THROW(w.marking.submissionsFor(*w.lec2, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.marking.submissionsFor(*w.lec, 9999), NotFoundError);
    EXPECT_EQ(w.marking.submissionsFor(*w.lec, w.assignment.getId()).size(), 1u);
    EXPECT_EQ(auditCount(w), 0);
}

TEST(UC02_E5_Mark, AuditFailureRollsTheMarkBackToo) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    FailingAuditRepository failingRepo;
    AuditService brokenAudit(failingRepo, w.clock);
    MarkingService broken(w.submissionRepo, w.assignmentRepo, w.courseSvc, w.penalty, brokenAudit, w.db);
    EXPECT_THROW_MSG(broken.mark(*w.lec, s.getId(), 80, "fb", ""), DatabaseError, "simulated");
    Submission back = *w.submissionRepo.findById(s.getId());
    EXPECT_EQ(back.getStatus(), SubmissionStatus::Submitted);  // mark was rolled back with the audit entry
    EXPECT_FALSE(back.getFinalMark().has_value());
    EXPECT_NO_THROW(w.marking.mark(*w.lec, s.getId(), 80, "fb", ""));  // still works afterwards
}

TEST(UC02_Mark, OnlyTheLatestVersionCanBeMarked) {
    World w;
    Submission v1 = w.submitAs(w.stu1, kNow, "v1");
    w.submitAs(w.stu1, kNow.plusSeconds(5), "v2");
    EXPECT_THROW_MSG(w.marking.mark(*w.lec, v1.getId(), 50, "fb", ""), BusinessRuleError, "older version");
    EXPECT_EQ(w.marking.submissionsFor(*w.lec, w.assignment.getId()).size(), 1u);  // current versions only
}

TEST(UC02_Mark, ReMarkingAfterReturnNeedsNoReasonAndIsLogged) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    w.marking.mark(*w.lec, s.getId(), 60, "fb", "");
    w.moderation.returnToLecturer(*w.hod, s.getId(), "Please check Q3");
    EXPECT_NO_THROW(w.marking.mark(*w.lec, s.getId(), 66, "fb", ""));
    EXPECT_EQ(w.audit.history(s.getId()).size(), 3u);  // mark, return, re-mark
}

// ===== ModerationService: UC-03 ==========================================================

namespace {
Submission markedSubmission(World& w, const UserPtr& student, int mark, const DateTime& when) {
    Submission s = w.submitAs(student, when);
    return w.marking.mark(*w.lec, s.getId(), mark, "fb", "");
}
}  // namespace

TEST(UC03_Review, HodSeesEveryCurrentMarkInTheirDepartmentOnly) {
    World w;
    markedSubmission(w, w.stu1, 70, kNow);
    w.submitAs(w.stu2, kNow);
    EXPECT_EQ(w.moderation.reviewMarks(*w.hod, w.assignment.getId()).size(), 2u);  // BR-14
    EXPECT_THROW_MSG(w.moderation.reviewMarks(*w.hodOther, w.assignment.getId()), AuthorizationError, "not in your department");  // BR-13
    EXPECT_THROW(w.moderation.reviewMarks(*w.lec, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.reviewMarks(*w.stu1, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.reviewMarks(*w.hod, 9999), NotFoundError);
}

TEST(UC03_A1_Adjust, RequiresAReasonAndRangeAndIsAudited) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    EXPECT_THROW_MSG(w.moderation.adjustMark(*w.hod, s.getId(), 75, ""), ValidationError, "reason");       // E2
    EXPECT_THROW(w.moderation.adjustMark(*w.hod, s.getId(), 75, "  "), ValidationError);
    EXPECT_THROW_MSG(w.moderation.adjustMark(*w.hod, s.getId(), 101, "why"), ValidationError, "between 0 and 100");  // E1
    EXPECT_THROW(w.moderation.adjustMark(*w.hod, s.getId(), -1, "why"), ValidationError);
    EXPECT_THROW_MSG(w.moderation.adjustMark(*w.hod, s.getId(), 70, "why"), ValidationError, "same as the current");
    EXPECT_EQ(*w.submissionRepo.findById(s.getId())->getFinalMark(), 70);  // E1: old value kept
    Submission adjusted = w.moderation.adjustMark(*w.hod, s.getId(), 75, "Rubric moderation");
    EXPECT_EQ(*adjusted.getFinalMark(), 75);
    EXPECT_EQ(*adjusted.getRawMark(), 70);  // raw mark is the lecturer's and is untouched
    auto log = w.audit.history(s.getId());
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(*log[1].getOldValue(), 70);
    EXPECT_EQ(log[1].getNewValue(), 75);
    EXPECT_EQ(log[1].getChangedBy(), w.hod->getId());
    EXPECT_EQ(log[1].getReason(), "Rubric moderation");
    EXPECT_EQ(w.moderation.adjustMark(*w.hod, s.getId(), 0, "boundary").getFinalMark().value(), 0);
    EXPECT_EQ(w.moderation.adjustMark(*w.hod, s.getId(), 100, "boundary").getFinalMark().value(), 100);
}

TEST(UC03_Adjust, UnmarkedOrReleasedWorkCannotBeAdjustedAndOtherDepartmentsAreDenied) {
    World w;
    Submission unmarked = w.submitAs(w.stu1, kNow);
    EXPECT_THROW_MSG(w.moderation.adjustMark(*w.hod, unmarked.getId(), 50, "why"), BusinessRuleError, "marked or moderated");
    w.marking.mark(*w.lec, unmarked.getId(), 60, "fb", "");
    EXPECT_THROW(w.moderation.adjustMark(*w.hodOther, unmarked.getId(), 50, "why"), AuthorizationError);  // E3
    EXPECT_THROW(w.moderation.adjustMark(*w.lec, unmarked.getId(), 50, "why"), AuthorizationError);
    w.moderation.approve(*w.hod, unmarked.getId());
    EXPECT_NO_THROW(w.moderation.adjustMark(*w.hod, unmarked.getId(), 62, "after approval"));  // moderated work may still be adjusted
    w.moderation.release(*w.hod, w.assignment.getId());
    EXPECT_THROW(w.moderation.adjustMark(*w.hod, unmarked.getId(), 90, "too late"), BusinessRuleError);
    EXPECT_THROW(w.moderation.adjustMark(*w.hod, 9999, 90, "why"), NotFoundError);
}

TEST(UC03_Approve, OnlyMarkedWorkMovesToModerated) {
    World w;
    Submission unmarked = w.submitAs(w.stu1, kNow);
    EXPECT_THROW_MSG(w.moderation.approve(*w.hod, unmarked.getId()), BusinessRuleError, "Only marked work");
    w.marking.mark(*w.lec, unmarked.getId(), 60, "fb", "");
    EXPECT_EQ(w.moderation.approve(*w.hod, unmarked.getId()).getStatus(), SubmissionStatus::Moderated);
    EXPECT_THROW(w.moderation.approve(*w.hod, unmarked.getId()), BusinessRuleError);  // already moderated
    EXPECT_THROW(w.moderation.approve(*w.hodOther, unmarked.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.approve(*w.lec, unmarked.getId()), AuthorizationError);
    EXPECT_EQ(w.audit.history(unmarked.getId()).size(), 2u);  // marking + approval
}

TEST(UC03_A3_ApproveAll, ApprovesMarkedWorkAndSkipsTheRest) {
    World w;
    markedSubmission(w, w.stu1, 70, kNow);
    w.submitAs(w.stu2, kNow);  // not marked
    EXPECT_EQ(w.moderation.approveAll(*w.hod, w.assignment.getId()), 1);
    EXPECT_EQ(w.moderation.approveAll(*w.hod, w.assignment.getId()), 0);  // A4: nothing left to moderate
    EXPECT_THROW(w.moderation.approveAll(*w.hodOther, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.approveAll(*w.hod, 9999), NotFoundError);
}

TEST(UC03_A2_Return, SendsWorkBackToTheLecturerWithAComment) {
    World w;
    Submission onTime = markedSubmission(w, w.stu1, 70, kNow);
    Submission late = markedSubmission(w, w.stu2, 70, afterDeadline(w, 10));
    EXPECT_THROW_MSG(w.moderation.returnToLecturer(*w.hod, onTime.getId(), ""), ValidationError, "comment");
    EXPECT_EQ(w.moderation.returnToLecturer(*w.hod, onTime.getId(), "Check Q2").getStatus(), SubmissionStatus::Submitted);
    EXPECT_EQ(w.moderation.returnToLecturer(*w.hod, late.getId(), "Check Q2").getStatus(), SubmissionStatus::Late);
    auto log = w.audit.history(onTime.getId());
    ASSERT_EQ(log.size(), 2u);
    EXPECT_NE(log[1].getReason().find("Returned to lecturer: Check Q2"), std::string::npos);
    EXPECT_THROW_MSG(w.moderation.returnToLecturer(*w.hod, onTime.getId(), "again"), BusinessRuleError, "Only marked work");
    EXPECT_THROW(w.moderation.returnToLecturer(*w.hodOther, late.getId(), "x"), AuthorizationError);
}

TEST(UC03_Release, ReleasesOnlyModeratedWorkAndCountsThePending) {
    World w;
    Submission a = markedSubmission(w, w.stu1, 70, kNow);
    markedSubmission(w, w.stu2, 55, kNow);           // marked, not approved
    w.moderation.approve(*w.hod, a.getId());
    ReleaseResult r = w.moderation.release(*w.hod, w.assignment.getId());
    EXPECT_EQ(r.released, 1);
    EXPECT_EQ(r.pending, 1);
    EXPECT_EQ(w.submissionRepo.findById(a.getId())->getStatus(), SubmissionStatus::Released);
    ReleaseResult again = w.moderation.release(*w.hod, w.assignment.getId());  // nothing new moderated
    EXPECT_EQ(again.released, 0);
    EXPECT_EQ(again.pending, 1);
    EXPECT_THROW(w.moderation.release(*w.hodOther, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.release(*w.lec, w.assignment.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.release(*w.hod, 9999), NotFoundError);
}

TEST(UC03_AuditHistory, OnlyTheHodOfTheDepartmentMaySeeIt) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    EXPECT_EQ(w.moderation.auditHistory(*w.hod, s.getId()).size(), 1u);
    EXPECT_THROW(w.moderation.auditHistory(*w.hodOther, s.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.auditHistory(*w.lec, s.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.auditHistory(*w.stu1, s.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.auditHistory(*w.hod, 9999), NotFoundError);
}

TEST(UC03_E5_Moderation, AuditFailureRollsBackAdjustApproveReturnAndRelease) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    FailingAuditRepository failingRepo;
    AuditService brokenAudit(failingRepo, w.clock);
    ModerationService broken(w.submissionRepo, w.assignmentRepo, w.courseSvc, brokenAudit, w.db);
    EXPECT_THROW(broken.adjustMark(*w.hod, s.getId(), 80, "why"), DatabaseError);
    EXPECT_EQ(*w.submissionRepo.findById(s.getId())->getFinalMark(), 70);
    EXPECT_THROW(broken.approve(*w.hod, s.getId()), DatabaseError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Marked);
    EXPECT_THROW(broken.approveAll(*w.hod, w.assignment.getId()), DatabaseError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Marked);
    EXPECT_THROW(broken.returnToLecturer(*w.hod, s.getId(), "c"), DatabaseError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Marked);
    w.moderation.approve(*w.hod, s.getId());
    EXPECT_THROW(broken.release(*w.hod, w.assignment.getId()), DatabaseError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Moderated);
}

TEST(UC03_BR05, StudentsNeverReceiveAMarkOrFeedbackBeforeRelease) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    auto check = [&](bool expectVisible) {
        auto rows = w.submissions.assignmentsFor(*w.stu1);
        ASSERT_EQ(rows.size(), 1u);
        ASSERT_TRUE(rows[0].current.has_value());
        EXPECT_EQ(rows[0].current->getFinalMark().has_value(), expectVisible);
        EXPECT_EQ(rows[0].current->getRawMark().has_value(), expectVisible);
        EXPECT_EQ(!rows[0].current->getFeedback().empty(), expectVisible);
    };
    check(false);                       // marked
    w.moderation.approve(*w.hod, s.getId());
    check(false);                       // moderated
    w.moderation.release(*w.hod, w.assignment.getId());
    check(true);                        // released
}

// ===== AuditService ======================================================================

TEST(AuditService, EntriesAreTimestampedFromTheClockAndNeedAReason) {
    World w;
    Submission s = w.submitAs(w.stu1, kNow);
    w.setNow(at("2026-11-02T10:00:00Z"));
    w.audit.record(s.getId(), w.lec->getId(), std::nullopt, 40, "  because  ");
    auto h = w.audit.history(s.getId());
    ASSERT_EQ(h.size(), 1u);
    EXPECT_EQ(h[0].getChangedAt().toIso(), "2026-11-02T10:00:00Z");
    EXPECT_EQ(h[0].getReason(), "because");
    EXPECT_THROW(w.audit.record(s.getId(), w.lec->getId(), 40, 50, ""), ValidationError);
    EXPECT_THROW(w.audit.record(s.getId(), w.lec->getId(), 40, 50, std::string(601, 'r')), ValidationError);
    EXPECT_NO_THROW(w.audit.record(s.getId(), w.lec->getId(), 40, 50, std::string(600, 'r')));
    EXPECT_EQ(w.audit.history(s.getId()).size(), 2u);
}

// ===== DEF-01 and DEF-02 regression tests ================================================

TEST(UC03_BR05, StudentsCannotSeeHowFarMarkingHasProgressedEither) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    w.moderation.approve(*w.hod, s.getId());
    auto shown = w.submissions.assignmentsFor(*w.stu1)[0].current;
    ASSERT_TRUE(shown.has_value());
    EXPECT_EQ(shown->getStatus(), SubmissionStatus::Submitted);  // not MARKED or MODERATED
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Moderated);  // stored state unchanged
}

TEST(UC03_BR05, LateWorkStaysLateInTheStudentView) {
    World w;
    Submission s = w.submitAs(w.stu1, afterDeadline(w, 10));
    w.marking.mark(*w.lec, s.getId(), 70, "fb", "");
    EXPECT_EQ(w.submissions.assignmentsFor(*w.stu1)[0].current->getStatus(), SubmissionStatus::Late);
}

TEST(Submission, RedactedCopyKeepsEverythingExceptMarksForUnreleasedWork) {
    Submission marked(5, 2, 3, 4, "my text", at(kStart), SubmissionStatus::Marked, 0, 60, 60, "good");
    Submission shown = marked.redactedForStudent();
    EXPECT_EQ(shown.getId(), 5);
    EXPECT_EQ(shown.getVersion(), 4);
    EXPECT_EQ(shown.getContent(), "my text");
    EXPECT_FALSE(shown.getRawMark().has_value());
    EXPECT_FALSE(shown.getFinalMark().has_value());
    EXPECT_TRUE(shown.getFeedback().empty());
    Submission released(5, 2, 3, 4, "my text", at(kStart), SubmissionStatus::Released, 0, 60, 55, "good");
    Submission full = released.redactedForStudent();
    EXPECT_EQ(*full.getFinalMark(), 55);
    EXPECT_EQ(full.getFeedback(), "good");
}

TEST(UC03_E4_Concurrency, ApprovalIsRefusedIfTheLecturerChangedTheMarkAfterTheReview) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    const int seen = *w.moderation.review(*w.hod, s.getId()).getFinalMark();
    w.marking.mark(*w.lec, s.getId(), 40, "fb", "Marked wrongly");        // changes while the HoD is looking
    EXPECT_THROW_MSG(w.moderation.approve(*w.hod, s.getId(), seen), BusinessRuleError, "changed after you reviewed");
    Submission stored = *w.submissionRepo.findById(s.getId());
    EXPECT_EQ(stored.getStatus(), SubmissionStatus::Marked);               // approved nothing stale
    EXPECT_EQ(*stored.getFinalMark(), 40);
    // After reloading, the HoD approves the mark they actually see.
    EXPECT_NO_THROW(w.moderation.approve(*w.hod, s.getId(), *w.moderation.review(*w.hod, s.getId()).getFinalMark()));
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Moderated);
}

TEST(UC03_E4_Concurrency, ApproveReviewedSkipsChangedAndUnreviewedWorkAndApprovesTheRest) {
    World w;
    Submission a = markedSubmission(w, w.stu1, 70, kNow);
    Submission b = markedSubmission(w, w.stu2, 60, kNow);
    std::vector<ReviewedMark> reviewed = {{a.getId(), 70}, {b.getId(), 60}};
    w.marking.mark(*w.lec, b.getId(), 50, "fb", "Changed after review");   // b changes
    ApproveResult r = w.moderation.approveReviewed(*w.hod, w.assignment.getId(), reviewed);
    EXPECT_EQ(r.approved, 1);
    EXPECT_EQ(r.skipped, 1);
    EXPECT_EQ(w.submissionRepo.findById(a.getId())->getStatus(), SubmissionStatus::Moderated);
    EXPECT_EQ(w.submissionRepo.findById(b.getId())->getStatus(), SubmissionStatus::Marked);
    // Marked work that was never in the reviewed list is also left alone.
    ApproveResult none = w.moderation.approveReviewed(*w.hod, w.assignment.getId(), {});
    EXPECT_EQ(none.approved, 0);
    EXPECT_EQ(none.skipped, 1);
    EXPECT_THROW(w.moderation.approveReviewed(*w.hodOther, w.assignment.getId(), reviewed), AuthorizationError);
    EXPECT_THROW(w.moderation.approveReviewed(*w.lec, w.assignment.getId(), reviewed), AuthorizationError);
}

TEST(UC03_E4_Concurrency, ReviewIsRestrictedToTheHodOfTheDepartment) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    EXPECT_EQ(w.moderation.review(*w.hod, s.getId()).getId(), s.getId());
    EXPECT_THROW(w.moderation.review(*w.hodOther, s.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.review(*w.lec, s.getId()), AuthorizationError);
    EXPECT_THROW(w.moderation.review(*w.hod, 9999), NotFoundError);
}

TEST(UC03_E5_Moderation, AuditFailureAlsoRollsBackApproveReviewed) {
    World w;
    Submission s = markedSubmission(w, w.stu1, 70, kNow);
    FailingAuditRepository failingRepo;
    AuditService brokenAudit(failingRepo, w.clock);
    ModerationService broken(w.submissionRepo, w.assignmentRepo, w.courseSvc, brokenAudit, w.db);
    EXPECT_THROW(broken.approveReviewed(*w.hod, w.assignment.getId(), {{s.getId(), 70}}), DatabaseError);
    EXPECT_EQ(w.submissionRepo.findById(s.getId())->getStatus(), SubmissionStatus::Marked);
}

TEST(UC03_Moderation, OlderVersionsOfAStudentsWorkCannotBeModerated) {
    World w;
    Submission v1 = w.submitAs(w.stu1, kNow, "v1");
    w.submitAs(w.stu1, kNow.plusSeconds(5), "v2");   // v1 is now superseded
    EXPECT_THROW_MSG(w.moderation.approve(*w.hod, v1.getId()), BusinessRuleError, "older version");
    EXPECT_THROW(w.moderation.adjustMark(*w.hod, v1.getId(), 50, "why"), BusinessRuleError);
    EXPECT_THROW(w.moderation.returnToLecturer(*w.hod, v1.getId(), "why"), BusinessRuleError);
}
