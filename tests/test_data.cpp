// Integration tests: SQLite database, transactions, schema constraints and repositories.
#include <cstdio>
#include <filesystem>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#include "TestSupport.hpp"

using namespace lms;
using namespace lms::test;

namespace {
int countRows(Database& db, const std::string& sql) {
    Statement q(db, sql);
    q.step();
    return q.columnInt(0);
}
}  // namespace

TEST(Database, CreatesSchemaOnFirstOpen) {
    Database db(":memory:");
    for (const char* table : {"department", "app_user", "student", "staff", "course", "course_lecturer", "enrolment",
                              "assignment", "extension", "submission", "audit_entry"}) {
        EXPECT_EQ(countRows(db, std::string("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='") + table + "'"), 1) << table;
    }
}

TEST(Database, ForeignKeysAreEnforced) {
    Database db(":memory:");
    EXPECT_THROW(db.execute("INSERT INTO course (code, title, department_id) VALUES ('X', 'Y', 999)"), DatabaseError);
}

TEST(Database, BadSqlIsReportedAsDatabaseError) {
    Database db(":memory:");
    EXPECT_THROW(db.execute("SELEC nonsense"), DatabaseError);
    EXPECT_THROW(Statement(db, "SELECT * FROM no_such_table"), DatabaseError);
}

TEST(Database, CannotOpenAnImpossiblePath) {
    EXPECT_THROW_MSG(Database impossible("/no/such/folder/lms.db"), DatabaseError, "Cannot open the database file");
}

TEST(Database, ReopeningAKeepsDataAndRejectsOtherVersions) {
    const auto path = (std::filesystem::temp_directory_path() / "lms_test_reopen.db").string();
    std::remove(path.c_str());
    {
        Database db(path);
        db.execute("INSERT INTO department (name) VALUES ('Physics')");
    }
    {
        Database db(path);  // opens the existing file; schema is not created twice
        EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 1);
        db.execute("PRAGMA user_version = 99");
    }
    EXPECT_THROW_MSG(Database reopened(path), DatabaseError, "different version");
    std::remove(path.c_str());
}

#if defined(__unix__) || defined(__APPLE__)
// Code review finding R-01: the file holds password hashes and all marks, so it must not be readable by other users.
TEST(Database, FileIsReadableAndWritableOnlyByItsOwner) {
    const auto path = (std::filesystem::temp_directory_path() / "lms_test_permissions.db").string();
    std::remove(path.c_str());
    {
        Database db(path);
    }
    struct stat info {};
    ASSERT_EQ(stat(path.c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 0777u, 0600u);

    // A file made earlier with open permissions is tightened the next time the program opens it.
    ASSERT_EQ(chmod(path.c_str(), 0644), 0);
    {
        Database db(path);
    }
    ASSERT_EQ(stat(path.c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 0777u, 0600u);
    std::remove(path.c_str());
}
#endif

TEST(Database, TransactionCommitsOnSuccess) {
    Database db(":memory:");
    db.runInTransaction([&] { db.execute("INSERT INTO department (name) VALUES ('A')"); });
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 1);
}

TEST(Database, TransactionRollsBackEverythingWhenAnExceptionIsThrown) {
    Database db(":memory:");
    EXPECT_THROW(db.runInTransaction([&] {
                     db.execute("INSERT INTO department (name) VALUES ('A')");
                     db.execute("INSERT INTO department (name) VALUES ('B')");
                     throw BusinessRuleError("simulated failure");
                 }),
                 BusinessRuleError);
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 0);
    // The database is still usable afterwards.
    db.runInTransaction([&] { db.execute("INSERT INTO department (name) VALUES ('C')"); });
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 1);
}

TEST(Database, SqlErrorInsideTransactionRollsBackEarlierStatements) {
    Database db(":memory:");
    EXPECT_THROW(db.runInTransaction([&] {
                     db.execute("INSERT INTO department (name) VALUES ('A')");
                     db.execute("INSERT INTO department (name) VALUES ('A')");  // UNIQUE violation
                 }),
                 DatabaseError);
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 0);
}

TEST(Database, NestedTransactionsCommitTogetherOrNotAtAll) {
    Database db(":memory:");
    db.runInTransaction([&] {
        db.execute("INSERT INTO department (name) VALUES ('A')");
        db.runInTransaction([&] { db.execute("INSERT INTO department (name) VALUES ('B')"); });
    });
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 2);

    EXPECT_THROW(db.runInTransaction([&] {
                     db.runInTransaction([&] { db.execute("INSERT INTO department (name) VALUES ('C')"); });
                     throw LmsError("outer fails");
                 }),
                 LmsError);
    EXPECT_EQ(countRows(db, "SELECT COUNT(*) FROM department"), 2);  // inner work was rolled back too
}

TEST(Database, StatementBindsAllTypesIncludingNull) {
    Database db(":memory:");
    db.execute("CREATE TABLE t (a INTEGER, b TEXT, c INTEGER, d INTEGER)");
    Statement ins(db, "INSERT INTO t VALUES (?, ?, ?, ?)");
    ins.bind(1, 5).bind(2, std::string("hi")).bind(3, std::optional<int>()).bind(4, 123456789012LL);
    ins.run();
    Statement q(db, "SELECT a, b, c, d FROM t");
    ASSERT_TRUE(q.step());
    EXPECT_EQ(q.columnInt(0), 5);
    EXPECT_EQ(q.columnText(1), "hi");
    EXPECT_TRUE(q.columnIsNull(2));
    EXPECT_FALSE(q.columnOptionalInt(2).has_value());
    EXPECT_EQ(*q.columnOptionalInt(0), 5);
    EXPECT_FALSE(q.step());
}

// ---- Schema rules enforced by the database itself ------------------------------------------

TEST(Schema, MarksCannotExceedTheAssignmentMaximum) {
    World w;
    w.submitAs(w.stu1, at(kStart));
    EXPECT_THROW_MSG(w.db.execute("UPDATE submission SET raw_mark = 101, final_mark = 50"), DatabaseError, "exceeds");
    EXPECT_THROW(w.db.execute("UPDATE submission SET raw_mark = 50, final_mark = 101"), DatabaseError);
    EXPECT_NO_THROW(w.db.execute("UPDATE submission SET raw_mark = 100, final_mark = 100"));  // boundary
}

TEST(Schema, ReleasedSubmissionsCannotBeChangedByAnySqlUpdate) {
    World w;
    w.submitAs(w.stu1, at(kStart));
    w.db.execute("UPDATE submission SET status = 'RELEASED', raw_mark = 5, final_mark = 5, feedback = 'ok'");
    EXPECT_THROW_MSG(w.db.execute("UPDATE submission SET final_mark = 99"), DatabaseError, "released");
}

TEST(Schema, AuditEntriesAreAppendOnly) {
    World w;
    Submission s = w.submitAs(w.stu1, at(kStart));
    w.marking.mark(*w.lec, s.getId(), 70, "ok", "");
    EXPECT_EQ(countRows(w.db, "SELECT COUNT(*) FROM audit_entry"), 1);
    EXPECT_THROW_MSG(w.db.execute("UPDATE audit_entry SET new_value = 100"), DatabaseError, "cannot be changed");
    EXPECT_THROW_MSG(w.db.execute("DELETE FROM audit_entry"), DatabaseError, "cannot be deleted");
    EXPECT_EQ(countRows(w.db, "SELECT COUNT(*) FROM audit_entry"), 1);
}

TEST(Schema, AuditReasonCannotBeBlank) {
    World w;
    Submission s = w.submitAs(w.stu1, at(kStart));
    EXPECT_THROW(w.db.execute("INSERT INTO audit_entry (submission_id, changed_by, changed_at, new_value, reason) "
                              "VALUES (" + std::to_string(s.getId()) + ", 1, '2026-11-01T08:00:00Z', 5, '   ')"),
                 DatabaseError);
}

TEST(Schema, ExtensionMustBeLaterThanTheDeadline) {
    World w;
    const std::string id = std::to_string(w.assignment.getId());
    const std::string stu = std::to_string(w.stu1->getId());
    const std::string lec = std::to_string(w.lec->getId());
    EXPECT_THROW_MSG(w.db.execute("INSERT INTO extension (assignment_id, student_id, new_deadline, granted_by) VALUES (" +
                                  id + "," + stu + ",'" + w.deadline().toIso() + "'," + lec + ")"),
                     DatabaseError, "later than");
    EXPECT_NO_THROW(w.db.execute("INSERT INTO extension (assignment_id, student_id, new_deadline, granted_by) VALUES (" +
                                 id + "," + stu + ",'" + w.deadline().plusSeconds(1).toIso() + "'," + lec + ")"));
}

TEST(Schema, OnlyOneHeadOfDepartmentPerDepartment) {
    World w;
    EXPECT_THROW(w.db.execute("INSERT INTO app_user (username, password_hash, salt, full_name, role) VALUES ('hod3','x','00','Third','HOD')"
                              ";INSERT INTO staff (user_id, role, department_id) VALUES (last_insert_rowid(),'HOD'," +
                              std::to_string(w.dept.getId()) + ")"),
                 DatabaseError);
}

TEST(Schema, StudentRowMustBelongToAStudentUser) {
    World w;
    // A lecturer's id with role STUDENT does not match app_user (id, role).
    EXPECT_THROW(w.db.execute("INSERT INTO student (user_id, student_number) VALUES (" + std::to_string(w.lec->getId()) + ", 'S9')"),
                 DatabaseError);
}

TEST(Schema, OneSubmissionVersionPerStudentAndAssignment) {
    World w;
    Submission s = w.submitAs(w.stu1, at(kStart));
    EXPECT_THROW(w.db.execute("INSERT INTO submission (assignment_id, student_id, version, content, submitted_at, status, late_days) VALUES (" +
                              std::to_string(s.getAssignmentId()) + "," + std::to_string(s.getStudentId()) +
                              ",1,'dup','2026-11-01T08:00:00Z','SUBMITTED',0)"),
                 DatabaseError);
}

// ---- Repositories ---------------------------------------------------------------------------

TEST(UserRepository, RoundTripForEveryRole) {
    World w;
    auto s = w.users.findById(w.stu1->getId());
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ((*s)->getRole(), Role::Student);
    EXPECT_EQ(dynamic_cast<Student&>(**s).getStudentNumber(), "S0001");
    auto l = w.users.findByUsername("lec1");
    ASSERT_TRUE(l.has_value());
    EXPECT_EQ((*l)->getDepartmentId(), w.dept.getId());
    auto h = w.users.findHeadOfDepartment(w.dept.getId());
    ASSERT_TRUE(h.has_value());
    EXPECT_EQ((*h)->getUsername(), "hod");
    EXPECT_FALSE(w.users.findHeadOfDepartment(9999).has_value());
    EXPECT_TRUE(w.users.findByStudentNumber("S0002").has_value());
    EXPECT_FALSE(w.users.findByStudentNumber("nope").has_value());
    EXPECT_FALSE(w.users.findById(9999).has_value());
    EXPECT_EQ(w.users.countAll(), 8);
    EXPECT_EQ(w.users.findAll().size(), 8u);
}

TEST(UserRepository, SavingAnExistingUserUpdatesPasswordColumns) {
    World w;
    UserPtr u = *w.users.findByUsername("stu1");
    const int id = u->getId();
    // Update using the model directly (fake hash is not verifiable, so only compare stored columns).
    u = std::make_shared<Student>(id, "stu1", "newhash", "11", "Thabo Renamed", "S0001");
    w.users.save(u);
    UserPtr again = *w.users.findById(id);
    EXPECT_EQ(again->getPasswordHash(), "newhash");
    EXPECT_EQ(again->getSalt(), "11");
    EXPECT_EQ(again->getFullName(), "Thabo Renamed");
    EXPECT_EQ(w.users.countAll(), 8);  // updated, not duplicated
}

TEST(UserRepository, DuplicateUsernameIsRejectedByTheDatabase) {
    World w;
    UserPtr dup = std::make_shared<Administrator>(0, "admin", "x", "00", "Another");
    EXPECT_THROW(w.users.save(dup), DatabaseError);
    EXPECT_EQ(w.users.countAll(), 8);  // failed save left nothing behind
}

TEST(UserRepository, SqlInjectionInUsernameIsHarmless) {
    World w;
    EXPECT_FALSE(w.users.findByUsername("' OR '1'='1").has_value());
    EXPECT_FALSE(w.users.findByUsername("admin'; DROP TABLE app_user; --").has_value());
    EXPECT_EQ(w.users.countAll(), 8);
    EXPECT_TRUE(w.users.findByUsername("admin").has_value());
}

TEST(OrgRepositories, DepartmentsAndCourses) {
    World w;
    EXPECT_EQ(w.departments.findAll().size(), 2u);
    EXPECT_TRUE(w.departments.findByName("Mathematics").has_value());
    EXPECT_FALSE(w.departments.findByName("Art").has_value());
    EXPECT_FALSE(w.departments.findById(999).has_value());
    EXPECT_EQ(w.departments.findById(w.dept.getId())->getName(), "Computer Science");

    EXPECT_TRUE(w.courses.findByCode("CS4433").has_value());
    EXPECT_FALSE(w.courses.findByCode("XX").has_value());
    EXPECT_EQ(w.courses.findAll().size(), 1u);
    EXPECT_EQ(w.courses.findByDepartment(w.dept.getId()).size(), 1u);
    EXPECT_EQ(w.courses.findByDepartment(w.deptOther.getId()).size(), 0u);
    EXPECT_EQ(w.courses.findByLecturer(w.lec->getId()).size(), 1u);
    EXPECT_EQ(w.courses.findByLecturer(w.lec2->getId()).size(), 0u);
    EXPECT_EQ(w.courses.findByStudent(w.stu1->getId()).size(), 1u);
    EXPECT_EQ(w.courses.findByStudent(w.stu3->getId()).size(), 0u);
    EXPECT_TRUE(w.courses.isLecturerOf(w.course.getId(), w.lec->getId()));
    EXPECT_FALSE(w.courses.isLecturerOf(w.course.getId(), w.lec2->getId()));
    EXPECT_TRUE(w.courses.isEnrolled(w.course.getId(), w.stu2->getId()));
    EXPECT_FALSE(w.courses.isEnrolled(w.course.getId(), w.stu3->getId()));
    EXPECT_EQ(w.courses.studentIdsOf(w.course.getId()).size(), 2u);
}

TEST(OrgRepositories, DuplicateCourseCodeAndDuplicateEnrolmentAreRejected) {
    World w;
    Course dup(0, "CS4433", "Other", w.dept.getId());
    EXPECT_THROW(w.courses.save(dup), DatabaseError);
    EXPECT_THROW(w.courses.enrol(w.course.getId(), w.stu1->getId()), DatabaseError);
    Department d(0, "Computer Science");
    EXPECT_THROW(w.departments.save(d), DatabaseError);
}

TEST(AssignmentRepository, SaveFindAndUpdate) {
    World w;
    auto found = w.assignmentRepo.findById(w.assignment.getId());
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->getTitle(), "Assignment 1");
    EXPECT_EQ(found->getDeadline(), w.deadline());
    EXPECT_EQ(found->getMaxMark(), 100);
    Assignment edited(found->getId(), found->getCourseId(), "Renamed", found->getDeadline(), 80);
    w.assignmentRepo.save(edited);
    EXPECT_EQ(w.assignmentRepo.findById(found->getId())->getTitle(), "Renamed");
    EXPECT_EQ(w.assignmentRepo.findByCourse(w.course.getId()).size(), 1u);
    EXPECT_FALSE(w.assignmentRepo.findById(999).has_value());
}

TEST(AssignmentRepository, ExtensionIsReplacedNotDuplicated) {
    World w;
    Extension e1(0, w.assignment.getId(), w.stu1->getId(), w.deadline().plusSeconds(kDay), w.lec->getId());
    w.assignmentRepo.saveExtension(e1);
    Extension e2(0, w.assignment.getId(), w.stu1->getId(), w.deadline().plusSeconds(2 * kDay), w.lec->getId());
    w.assignmentRepo.saveExtension(e2);
    EXPECT_EQ(countRows(w.db, "SELECT COUNT(*) FROM extension"), 1);
    EXPECT_EQ(w.assignmentRepo.findExtension(w.assignment.getId(), w.stu1->getId())->getNewDeadline(),
              w.deadline().plusSeconds(2 * kDay));
    EXPECT_FALSE(w.assignmentRepo.findExtension(w.assignment.getId(), w.stu2->getId()).has_value());
    EXPECT_EQ(e1.getId(), e2.getId());
}

TEST(SubmissionRepository, VersionsAndCurrentQueries) {
    World w;
    w.submitAs(w.stu1, at(kStart), "v1");
    w.submitAs(w.stu1, at(kStart).plusSeconds(60), "v2");
    w.submitAs(w.stu2, at(kStart).plusSeconds(120), "only");
    const int a = w.assignment.getId();
    EXPECT_EQ(w.submissionRepo.findByAssignment(a).size(), 3u);         // history kept
    auto current = w.submissionRepo.findCurrentByAssignment(a);
    ASSERT_EQ(current.size(), 2u);                                       // latest per student
    EXPECT_EQ(current[0].getContent(), "v2");
    EXPECT_EQ(w.submissionRepo.findLatest(a, w.stu1->getId())->getVersion(), 2);
    EXPECT_FALSE(w.submissionRepo.findLatest(a, w.stu3->getId()).has_value());
    EXPECT_EQ(w.submissionRepo.findCurrentByStudent(w.stu1->getId()).size(), 1u);
    EXPECT_FALSE(w.submissionRepo.findById(999).has_value());
}

TEST(SubmissionRepository, MarksAndFeedbackRoundTrip) {
    World w;
    Submission s = w.submitAs(w.stu1, at(kStart));
    s.applyMark(77, 77, "Well done");
    w.submissionRepo.save(s);
    Submission back = *w.submissionRepo.findById(s.getId());
    EXPECT_EQ(back.getStatus(), SubmissionStatus::Marked);
    EXPECT_EQ(*back.getRawMark(), 77);
    EXPECT_EQ(*back.getFinalMark(), 77);
    EXPECT_EQ(back.getFeedback(), "Well done");
    EXPECT_EQ(back.getSubmittedAt(), s.getSubmittedAt());
}

TEST(AuditRepository, AppendOnlyAtApplicationLevelToo) {
    World w;
    Submission s = w.submitAs(w.stu1, at(kStart));
    AuditEntry e(0, s.getId(), w.lec->getId(), at(kStart), std::nullopt, 50, "first");
    w.auditRepo.save(e);
    EXPECT_NE(e.getId(), 0);
    EXPECT_THROW_MSG(w.auditRepo.save(e), BusinessRuleError, "cannot be changed");
    auto back = w.auditRepo.findById(e.getId());
    ASSERT_TRUE(back.has_value());
    EXPECT_FALSE(back->getOldValue().has_value());
    EXPECT_EQ(back->getNewValue(), 50);
    EXPECT_EQ(w.auditRepo.findBySubmission(s.getId()).size(), 1u);
    EXPECT_TRUE(w.auditRepo.findBySubmission(9999).empty());
}
