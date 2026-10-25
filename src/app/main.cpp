// Composition root: builds the object graph and starts the console UI.
#include <cstdlib>
#include <iostream>
#include <string>

#include "data/Database.hpp"
#include "data/SqliteRepositories.hpp"
#include "domain/PenaltyPolicies.hpp"
#include "service/AuditService.hpp"
#include "ui/ConsoleApp.hpp"
#include "util/Clock.hpp"
#include "util/Errors.hpp"

namespace {

constexpr int kDefaultUtcOffsetMinutes = 120;  // Lesotho (SAST)

// Clock normally comes from the system. A test build may override it.
lms::DateTime startTime() {
#ifdef LMS_ENABLE_TEST_CLOCK
    if (const char* fake = std::getenv("LMS_FAKE_NOW")) return lms::DateTime::parseIso(fake);
#endif
    return lms::SystemClock().now();
}

void seedDemo(lms::AdminService& admin, lms::AuthService& auth, lms::AssignmentService& assignments,
              lms::SubmissionService& submissions, const lms::IClock& clock) {
    using namespace lms;
    UserData a{"admin", "Demo Administrator", "Admin12345", "", 0};
    UserPtr adminUser = auth.createInitialAdministrator(a);
    const Department dep = admin.createDepartment(*adminUser, "Computer Science");
    const Course course = admin.createCourse(*adminUser, "CS4433", "Software Engineering", dep.getId());
    UserPtr hod = admin.createUser(*adminUser, Role::Hod, {"hod", "Dr Head", "Hod123456", "", dep.getId()});
    UserPtr lec = admin.createUser(*adminUser, Role::Lecturer, {"lecturer", "Ms Lecturer", "Lect123456", "", dep.getId()});
    UserPtr s1 = admin.createUser(*adminUser, Role::Student, {"student1", "Thabo Student", "Stud123456", "S0001", 0});
    UserPtr s2 = admin.createUser(*adminUser, Role::Student, {"student2", "Palesa Student", "Stud123456", "S0002", 0});
    admin.assignLecturer(*adminUser, course.getId(), lec->getId());
    admin.enrolStudent(*adminUser, course.getId(), s1->getId());
    admin.enrolStudent(*adminUser, course.getId(), s2->getId());
    assignments.createAssignment(*lec, course.getId(), "Assignment 1",
                                 clock.now().plusSeconds(7 * 24 * 3600), 100);
    (void)submissions;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dbPath = "lms.db";
    bool demo = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
        } else if (arg == "--seed-demo") {
            demo = true;
        } else {
            std::cerr << "Usage: lms [--db FILE] [--seed-demo]\n";
            return 2;
        }
    }
    try {
        lms::Database db(dbPath);
        lms::SqliteUserRepository users(db);
        lms::SqliteDepartmentRepository departments(db);
        lms::SqliteCourseRepository courses(db);
        lms::SqliteAssignmentRepository assignmentRepo(db);
        lms::SqliteSubmissionRepository submissionRepo(db);
        lms::SqliteAuditRepository auditRepo(db);

        const lms::FixedClock fixed(startTime());
        lms::SystemClock system;
#ifdef LMS_ENABLE_TEST_CLOCK
        const lms::IClock& clock = std::getenv("LMS_FAKE_NOW") ? static_cast<const lms::IClock&>(fixed)
                                                              : static_cast<const lms::IClock&>(system);
#else
        const lms::IClock& clock = system;
        (void)fixed;
#endif
        lms::DailyPercentPenalty penalty(10, 3);
        lms::AuthService auth(users);
        lms::AdminService admin(users, departments, courses);
        lms::CourseService courseService(users, departments, courses);
        lms::AssignmentService assignments(assignmentRepo, courses, courseService, clock);
        lms::SubmissionService submissions(submissionRepo, assignmentRepo, courses, clock, db);
        lms::AuditService audit(auditRepo, clock);
        lms::MarkingService marking(submissionRepo, assignmentRepo, courseService, penalty, audit, db);
        lms::ModerationService moderation(submissionRepo, assignmentRepo, courseService, audit, db);

        if (demo) {
            if (!auth.noUsersExist()) {
                std::cerr << "--seed-demo needs an empty database.\n";
                return 2;
            }
            seedDemo(admin, auth, assignments, submissions, clock);
            std::cout << "Demo data created. Logins: admin/Admin12345, hod/Hod123456, "
                         "lecturer/Lect123456, student1/Stud123456, student2/Stud123456\n";
        }

        lms::ConsoleApp app({auth, admin, courseService, assignments, submissions, marking, moderation, users},
                            std::cin, std::cout, kDefaultUtcOffsetMinutes);
        app.run();
        return 0;
    } catch (const lms::LmsError& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    } catch (const std::exception&) {
        std::cerr << "Fatal: unexpected error.\n";
        return 1;
    }
}
