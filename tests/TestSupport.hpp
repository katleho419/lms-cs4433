// Shared test fixture: an in-memory database, all repositories and services,
// a fixed clock, and a small university set up the same way every time.
#ifndef LMS_TESTS_TESTSUPPORT_HPP
#define LMS_TESTS_TESTSUPPORT_HPP

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "data/Database.hpp"
#include "data/SqliteRepositories.hpp"
#include "domain/PenaltyPolicies.hpp"
#include "service/AdminService.hpp"
#include "service/AssignmentService.hpp"
#include "service/AuditService.hpp"
#include "service/AuthService.hpp"
#include "service/CourseService.hpp"
#include "service/MarkingService.hpp"
#include "service/ModerationService.hpp"
#include "service/SubmissionService.hpp"
#include "util/Clock.hpp"
#include "util/Errors.hpp"

namespace lms::test {

constexpr const char* kStart = "2026-11-01T08:00:00Z";  // the fixed "now"
constexpr int kDay = 86400;

inline DateTime at(const char* iso) { return DateTime::parseIso(iso); }

// EXPECT_THROW that also checks the message contains some text.
#define EXPECT_THROW_MSG(statement, ExceptionType, text)                                   \
    do {                                                                                   \
        try {                                                                              \
            statement;                                                                     \
            ADD_FAILURE() << "expected " #ExceptionType " but nothing was thrown";         \
        } catch (const ExceptionType& e__) {                                               \
            EXPECT_NE(std::string(e__.what()).find(text), std::string::npos)               \
                << "message was: " << e__.what();                                          \
        } catch (...) {                                                                    \
            ADD_FAILURE() << "expected " #ExceptionType " but another type was thrown";    \
        }                                                                                  \
    } while (false)

struct World {
    Database db{":memory:"};
    SqliteUserRepository users{db};
    SqliteDepartmentRepository departments{db};
    SqliteCourseRepository courses{db};
    SqliteAssignmentRepository assignmentRepo{db};
    SqliteSubmissionRepository submissionRepo{db};
    SqliteAuditRepository auditRepo{db};
    FixedClock clock{at(kStart)};
    DailyPercentPenalty penalty{10, 3};

    CourseService courseSvc{users, departments, courses};
    AuthService auth{users};
    AdminService admin{users, departments, courses};
    AssignmentService assignments{assignmentRepo, courses, courseSvc, clock, db};
    AuditService audit{auditRepo, clock};
    SubmissionService submissions{submissionRepo, assignmentRepo, courses, clock, db};
    MarkingService marking{submissionRepo, assignmentRepo, courseSvc, penalty, audit, db};
    ModerationService moderation{submissionRepo, assignmentRepo, courseSvc, audit, db};

    UserPtr adminU, hod, lec, lec2, stu1, stu2, stu3, hodOther;
    Department dept{0, ""}, deptOther{0, ""};
    Course course{0, "", "", 0};
    Assignment assignment{0, 0, "", DateTime(), 0};

    static constexpr const char* kPassword = "Passw0rd1";
    static constexpr const char* kAdminPassword = "Admin12345";

    // realPasswords = true uses the real (slow) scrypt hashing so tests can log in.
    explicit World(bool realPasswords = false) {
        if (realPasswords) {
            adminU = auth.createInitialAdministrator(UserData{"admin", "Admin User", kAdminPassword, "", 0});
        } else {
            adminU = quick(Role::Admin, "admin", "Admin User", 0, "");
        }
        dept = admin.createDepartment(*adminU, "Computer Science");
        deptOther = admin.createDepartment(*adminU, "Mathematics");
        course = admin.createCourse(*adminU, "CS4433", "Software Engineering", dept.getId());

        hod = make(realPasswords, Role::Hod, "hod", "Dr Head", dept.getId(), "");
        hodOther = make(realPasswords, Role::Hod, "hod2", "Dr Other", deptOther.getId(), "");
        lec = make(realPasswords, Role::Lecturer, "lec1", "Ms Lecturer", dept.getId(), "");
        lec2 = make(realPasswords, Role::Lecturer, "lec2", "Mr Second", dept.getId(), "");
        stu1 = make(realPasswords, Role::Student, "stu1", "Thabo Student", 0, "S0001");
        stu2 = make(realPasswords, Role::Student, "stu2", "Palesa Student", 0, "S0002");
        stu3 = make(realPasswords, Role::Student, "stu3", "Not Enrolled", 0, "S0003");

        admin.assignLecturer(*adminU, course.getId(), lec->getId());
        admin.enrolStudent(*adminU, course.getId(), stu1->getId());
        admin.enrolStudent(*adminU, course.getId(), stu2->getId());

        // Deadline is exactly 7 days after the fixed clock.
        assignment = assignments.createAssignment(*lec, course.getId(), "Assignment 1",
                                                  at(kStart).plusSeconds(7 * kDay), 100);
    }

    DateTime deadline() const { return assignment.getDeadline(); }
    void setNow(const DateTime& t) { clock.set(t); }

    // Submits as stu1 at `now` and returns the stored submission.
    Submission submitAs(const UserPtr& student, const DateTime& now, const std::string& text = "my answer") {
        clock.set(now);
        return submissions.submit(*student, assignment.getId(), text);
    }

    // Quick user with a fake hash (fast). Cannot log in.
    UserPtr quick(Role role, const std::string& username, const std::string& fullName, int departmentId,
                  const std::string& studentNumber) {
        UserPtr u;
        switch (role) {
            case Role::Admin: u = std::make_shared<Administrator>(0, username, "x", "00", fullName); break;
            case Role::Lecturer:
                u = std::make_shared<Lecturer>(0, username, "x", "00", fullName, departmentId);
                break;
            case Role::Hod:
                u = std::make_shared<HeadOfDepartment>(0, username, "x", "00", fullName, departmentId);
                break;
            case Role::Student: u = std::make_shared<Student>(0, username, "x", "00", fullName, studentNumber); break;
        }
        users.save(u);
        return u;
    }

private:
    UserPtr make(bool real, Role role, const std::string& username, const std::string& fullName,
                 int departmentId, const std::string& studentNumber) {
        if (!real) return quick(role, username, fullName, departmentId, studentNumber);
        return admin.createUser(*adminU, role, UserData{username, fullName, kPassword, studentNumber, departmentId});
    }
};

}  // namespace lms::test

#endif
