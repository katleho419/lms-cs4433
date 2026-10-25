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
    // One menu loop per role.
    void adminMenu(const UserPtr& user);
    void lecturerMenu(const UserPtr& user);
    void hodMenu(const UserPtr& user);
    void studentMenu(const UserPtr& user);

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
