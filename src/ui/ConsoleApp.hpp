// Text-based user interface. Talks only to the service layer.
#ifndef LMS_UI_CONSOLEAPP_HPP
#define LMS_UI_CONSOLEAPP_HPP

#include <iosfwd>
#include <string>

#include "service/AdminService.hpp"
#include "service/AssignmentService.hpp"
#include "service/AuthService.hpp"
#include "service/CourseService.hpp"
#include "service/MarkingService.hpp"
#include "service/ModerationService.hpp"
#include "service/SubmissionService.hpp"

namespace lms {

struct Services {
    AuthService& auth;
    AdminService& admin;
    CourseService& courses;
    AssignmentService& assignments;
    SubmissionService& submissions;
    MarkingService& marking;
    ModerationService& moderation;
    IUserRepository& users;
};

class ConsoleApp {
public:
    ConsoleApp(Services services, std::istream& in, std::ostream& out, int utcOffsetMinutes);
    // Returns when the user quits.
    void run();

private:
    // One menu loop per role. Each loop prints its menu text and hands the choice to a dispatcher,
    // which calls one small method per action.
    using MenuAction = void (ConsoleApp::*)(UserPtr& self, int choice);
    void runMenu(const UserPtr& user, const std::string& menuText, MenuAction action);

    void adminMenu(const UserPtr& user);
    void adminAction(UserPtr& self, int choice);
    void adminCreateDepartment(const User& self);
    void adminCreateCourse(const User& self);
    void adminCreateUser(const User& self);
    void adminListUsers(const User& self);
    void adminAssignLecturer(const User& self);
    void adminEnrolStudent(const User& self);

    void lecturerMenu(const UserPtr& user);
    void lecturerAction(UserPtr& self, int choice);
    void lecturerCreateAssignment(const User& self);
    void lecturerGrantExtension(const User& self);
    void lecturerViewSubmissions(const User& self);
    void lecturerMark(const User& self);

    void hodMenu(const UserPtr& user);
    void hodAction(UserPtr& self, int choice);
    void hodReviewMarks(const User& self);
    void hodAdjustMark(const User& self);
    void hodApproveOne(const User& self);
    void hodApproveAll(const User& self);
    void hodReturnToLecturer(const User& self);
    void hodRelease(const User& self);
    void hodAudit(const User& self);

    void studentMenu(const UserPtr& user);
    void studentAction(UserPtr& self, int choice);
    void studentShowAssignments(const User& self);
    void studentSubmit(const User& self);

    void listAssignmentsOfChosenCourse(const User& self);
    void printDepartments();

    void firstRunSetup();
    void changePassword(UserPtr& user);

    // Input helpers: re-prompt until the value is valid; return false on end of input.
    bool readLine(const std::string& prompt, std::string& value);
    bool readInt(const std::string& prompt, int& value);
    bool readDateTime(const std::string& prompt, DateTime& value);
    bool readSecret(const std::string& prompt, std::string& value);
    bool readMultiline(const std::string& prompt, std::string& value);

    void printCourses(const User& user);
    void printAssignments(const User& user, int courseId);
    void printSubmissions(const std::vector<Submission>& list);
    void printAudit(const std::vector<AuditEntry>& entries);
    std::string when(const DateTime& t) const;
    std::string nameOf(int userId) const;

    // Runs one menu action, turning LmsError into a message instead of a crash.
    template <typename F>
    void guarded(F&& action);

    Services s_;
    std::istream& in_;
    std::ostream& out_;
    int offset_;
    bool eof_ = false;
};

}  // namespace lms

#endif
