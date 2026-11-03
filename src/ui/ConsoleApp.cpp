#include "ui/ConsoleApp.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "util/Errors.hpp"
#include "util/Validation.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace lms {
namespace {

bool parseInt(const std::string& text, int& out) {
    const std::string t = trim(text);
    if (t.empty() || t.size() > 9) {
        return false;
    }
    const std::size_t i = (t[0] == '-') ? 1 : 0;
    if (i == t.size()) {
        return false;
    }
    for (std::size_t j = i; j < t.size(); ++j) {
        if (t[j] < '0' || t[j] > '9') {
            return false;
        }
    }
    out = std::stoi(t);
    return true;
}

}  // namespace

ConsoleApp::ConsoleApp(Services services, std::istream& in, std::ostream& out, int utcOffsetMinutes)
    : s_(services), in_(in), out_(out), offset_(utcOffsetMinutes) {}

template <typename F>
void ConsoleApp::guarded(F&& action) {
    try {
        action();
    } catch (const LmsError& e) {
        out_ << "\n  ! " << e.what() << "\n";
    } catch (const std::exception&) {
        out_ << "\n  ! An unexpected error occurred. Nothing was changed.\n";
    }
}

bool ConsoleApp::readLine(const std::string& prompt, std::string& value) {
    out_ << prompt << std::flush;
    if (!std::getline(in_, value)) {
        eof_ = true;
        return false;
    }
    return true;
}

bool ConsoleApp::readInt(const std::string& prompt, int& value) {
    std::string line;
    while (readLine(prompt, line)) {
        if (parseInt(line, value)) {
            return true;
        }
        out_ << "  Please enter a whole number.\n";
    }
    return false;
}

bool ConsoleApp::readDateTime(const std::string& prompt, DateTime& value) {
    std::string line;
    while (readLine(prompt + " (YYYY-MM-DD HH:MM): ", line)) {
        try {
            value = DateTime::parseLocal(trim(line), offset_);
            return true;
        } catch (const LmsError& e) {
            out_ << "  " << e.what() << "\n";
        }
    }
    return false;
}

bool ConsoleApp::readSecret(const std::string& prompt, std::string& value) {
#if defined(__unix__) || defined(__APPLE__)
    termios oldSettings{};
    const bool tty = isatty(STDIN_FILENO) != 0 && tcgetattr(STDIN_FILENO, &oldSettings) == 0;
    if (tty) {
        termios hidden = oldSettings;
        hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
    const bool ok = readLine(prompt, value);
    if (tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        out_ << "\n";
    }
    return ok;
#else
    return readLine(prompt, value);
#endif
}

bool ConsoleApp::readMultiline(const std::string& prompt, std::string& value) {
    out_ << prompt << " (finish with a line containing only a full stop '.')\n";
    value.clear();
    std::string line;
    while (std::getline(in_, line)) {
        if (trim(line) == ".") {
            return true;
        }
        value += line + "\n";
    }
    eof_ = true;
    return false;
}

std::string ConsoleApp::when(const DateTime& t) const {
    return t.toLocal(offset_);
}

std::string ConsoleApp::nameOf(int userId) const {
    std::string fallback = "user #" + std::to_string(userId);
    try {
        const auto user = s_.users.findById(userId);
        return user ? (*user)->getFullName() + " (" + (*user)->getUsername() + ")" : fallback;
    } catch (const LmsError&) {
        return fallback;  // a name that cannot be read must not stop a listing from printing
    }
}

void ConsoleApp::printCourses(const User& user) {
    const auto courses = s_.courses.coursesFor(user);
    if (courses.empty()) {
        out_ << "  (no courses)\n";
    }
    for (const auto& c : courses) {
        out_ << "  [" << c.getId() << "] " << c.getCode() << "  " << c.getTitle() << "\n";
    }
}

void ConsoleApp::printAssignments(const User& user, int courseId) {
    const auto list = s_.assignments.assignmentsForCourse(user, courseId);
    if (list.empty()) {
        out_ << "  (no assignments)\n";
    }
    for (const auto& a : list) {
        out_ << "  [" << a.getId() << "] " << a.getTitle() << "  due " << when(a.getDeadline()) << "  max "
             << a.getMaxMark() << "\n";
    }
}

void ConsoleApp::printSubmissions(const std::vector<Submission>& list) {
    if (list.empty()) {
        out_ << "  (no submissions)\n";
    }
    for (const auto& sub : list) {
        out_ << "  [" << sub.getId() << "] " << nameOf(sub.getStudentId()) << "  v" << sub.getVersion()
             << "  " << toString(sub.getStatus()) << "  late days " << sub.getLateDays();
        if (const auto raw = sub.getRawMark()) {
            out_ << "  raw " << *raw;
        }
        if (const auto finalMark = sub.getFinalMark()) {
            out_ << "  final " << *finalMark;
        }
        out_ << "\n";
    }
}

void ConsoleApp::printAudit(const std::vector<AuditEntry>& entries) {
    if (entries.empty()) {
        out_ << "  (no audit entries)\n";
    }
    for (const auto& e : entries) {
        out_ << "  " << when(e.getChangedAt()) << "  " << nameOf(e.getChangedBy()) << "  ";
        if (const auto oldValue = e.getOldValue()) {
            out_ << *oldValue;
        } else {
            out_ << "-";
        }
        out_ << " -> " << e.getNewValue() << "  : " << e.getReason() << "\n";
    }
}

void ConsoleApp::firstRunSetup() {
    out_ << "No accounts exist yet. Create the first administrator.\n";
    while (!eof_) {
        UserData data;
        if (!readLine("Username: ", data.username) || !readLine("Full name: ", data.fullName) ||
            !readSecret("Password: ", data.password)) {
            return;
        }
        bool created = false;
        guarded([&] {
            s_.auth.createInitialAdministrator(data);
            out_ << "Administrator created.\n";
            created = true;
        });
        if (created) {
            return;
        }
    }
}

void ConsoleApp::changePassword(UserPtr& user) {
    std::string oldPw;
    std::string newPw;
    if (!readSecret("Current password: ", oldPw) || !readSecret("New password: ", newPw)) {
        return;
    }
    guarded([&] {
        s_.auth.changePassword(user, oldPw, newPw);
        out_ << "Password changed.\n";
    });
}

void ConsoleApp::run() {
    out_ << "=== Moderated Learning Management System ===\n";
    guarded([&] {
        if (s_.auth.noUsersExist()) {
            firstRunSetup();
        }
    });
    int failures = 0;
    while (!eof_) {
        out_ << "\nLog in (leave the username empty to quit)\n";
        std::string username;
        std::string password;
        if (!readLine("Username: ", username)) {
            break;
        }
        if (trim(username).empty()) {
            break;
        }
        if (!readSecret("Password: ", password)) {
            break;
        }
        UserPtr user;
        try {
            user = s_.auth.login(trim(username), password);
        } catch (const AuthenticationError& e) {
            out_ << "  ! " << e.what() << "\n";
            if (++failures >= 5) {
                out_ << "Too many failed attempts. Exiting.\n";
                break;
            }
            continue;
        } catch (const LmsError& e) {
            out_ << "  ! " << e.what() << "\n";
            continue;
        } catch (const std::exception&) {
            out_ << "  ! An unexpected error occurred.\n";
            continue;
        }
        failures = 0;
        out_ << "\nWelcome, " << user->getFullName() << " (" << displayName(user->getRole()) << ").\n";
        switch (user->getRole()) {
            case Role::Admin:
                adminMenu(user);
                break;
            case Role::Lecturer:
                lecturerMenu(user);
                break;
            case Role::Hod:
                hodMenu(user);
                break;
            case Role::Student:
                studentMenu(user);
                break;
        }
    }
    out_ << "Goodbye.\n";
}

// ---- menus ------------------------------------------------------------------------------------
// Each role has a menu text, a dispatcher (one switch, no logic) and one small method per action.

void ConsoleApp::runMenu(const UserPtr& user, const std::string& menuText, MenuAction action) {
    UserPtr self = user;
    while (!eof_) {
        out_ << menuText;
        int choice = -1;
        if (!readInt("> ", choice)) {
            return;
        }
        guarded([&] { (this->*action)(self, choice); });
        if (choice == 0) {
            return;
        }
    }
}

void ConsoleApp::adminMenu(const UserPtr& user) {
    runMenu(user,
            "\n-- Administrator --\n 1 Create department\n 2 Create course\n 3 Create user\n"
            " 4 List users\n 5 Assign lecturer to course\n 6 Enrol student\n 7 List courses\n"
            " 8 Change my password\n 0 Log out\n",
            &ConsoleApp::adminAction);
}

void ConsoleApp::adminAction(UserPtr& self, int choice) {
    switch (choice) {
        case 0:
            break;
        case 1:
            adminCreateDepartment(*self);
            break;
        case 2:
            adminCreateCourse(*self);
            break;
        case 3:
            adminCreateUser(*self);
            break;
        case 4:
            adminListUsers(*self);
            break;
        case 5:
            adminAssignLecturer(*self);
            break;
        case 6:
            adminEnrolStudent(*self);
            break;
        case 7:
            printCourses(*self);
            break;
        case 8:
            changePassword(self);
            break;
        default:
            out_ << "  Unknown option.\n";
    }
}

void ConsoleApp::printDepartments() {
    for (const auto& d : s_.courses.departments()) {
        out_ << "  [" << d.getId() << "] " << d.getName() << "\n";
    }
}

void ConsoleApp::adminCreateDepartment(const User& self) {
    std::string name;
    if (!readLine("Department name: ", name)) {
        return;
    }
    const auto d = s_.admin.createDepartment(self, name);
    out_ << "  Created department [" << d.getId() << "].\n";
}

void ConsoleApp::adminCreateCourse(const User& self) {
    std::string code;
    std::string title;
    int dep = 0;
    printDepartments();
    if (!readLine("Course code: ", code) || !readLine("Title: ", title) || !readInt("Department id: ", dep)) {
        return;
    }
    const auto c = s_.admin.createCourse(self, code, title, dep);
    out_ << "  Created course [" << c.getId() << "].\n";
}

void ConsoleApp::adminCreateUser(const User& self) {
    out_ << "  Role: 1 Administrator, 2 Lecturer, 3 Head of Department, 4 Student\n";
    int r = 0;
    if (!readInt("Role: ", r)) {
        return;
    }
    if (r < 1 || r > 4) {
        throw ValidationError("Choose a role from 1 to 4.");
    }
    static constexpr std::array<Role, 4> kRoles = {Role::Admin, Role::Lecturer, Role::Hod, Role::Student};
    const Role role = kRoles.at(static_cast<std::size_t>(r - 1));
    UserData d;
    if (!readLine("Username: ", d.username) || !readLine("Full name: ", d.fullName) ||
        !readSecret("Initial password: ", d.password)) {
        return;
    }
    if (role == Role::Student && !readLine("Student number: ", d.studentNumber)) {
        return;
    }
    if (role == Role::Lecturer || role == Role::Hod) {
        printDepartments();
        if (!readInt("Department id: ", d.departmentId)) {
            return;
        }
    }
    const auto created = s_.admin.createUser(self, role, d);
    out_ << "  Created user [" << created->getId() << "].\n";
}

void ConsoleApp::adminListUsers(const User& self) {
    for (const auto& u : s_.admin.listUsers(self)) {
        out_ << "  [" << u->getId() << "] " << u->getUsername() << "  " << u->getFullName() << "  "
             << displayName(u->getRole()) << "\n";
    }
}

void ConsoleApp::adminAssignLecturer(const User& self) {
    int c = 0;
    int l = 0;
    printCourses(self);
    if (!readInt("Course id: ", c) || !readInt("Lecturer user id: ", l)) {
        return;
    }
    s_.admin.assignLecturer(self, c, l);
    out_ << "  Lecturer assigned.\n";
}

void ConsoleApp::adminEnrolStudent(const User& self) {
    int c = 0;
    int st = 0;
    printCourses(self);
    if (!readInt("Course id: ", c) || !readInt("Student user id: ", st)) {
        return;
    }
    s_.admin.enrolStudent(self, c, st);
    out_ << "  Student enrolled.\n";
}

void ConsoleApp::lecturerMenu(const UserPtr& user) {
    runMenu(user,
            "\n-- Lecturer --\n 1 My courses\n 2 Create assignment\n 3 List assignments\n"
            " 4 Grant extension (one student or the whole class)\n 5 View submissions\n 6 Mark a submission\n"
            " 7 Change my password\n 0 Log out\n",
            &ConsoleApp::lecturerAction);
}

void ConsoleApp::lecturerAction(UserPtr& self, int choice) {
    switch (choice) {
        case 0:
            break;
        case 1:
            printCourses(*self);
            break;
        case 2:
            lecturerCreateAssignment(*self);
            break;
        case 3:
            listAssignmentsOfChosenCourse(*self);
            break;
        case 4:
            lecturerGrantExtension(*self);
            break;
        case 5:
            lecturerViewSubmissions(*self);
            break;
        case 6:
            lecturerMark(*self);
            break;
        case 7:
            changePassword(self);
            break;
        default:
            out_ << "  Unknown option.\n";
    }
}

void ConsoleApp::listAssignmentsOfChosenCourse(const User& self) {
    int c = 0;
    printCourses(self);
    if (!readInt("Course id: ", c)) {
        return;
    }
    printAssignments(self, c);
}

void ConsoleApp::lecturerCreateAssignment(const User& self) {
    int c = 0;
    int max = 0;
    std::string title;
    DateTime deadline;
    printCourses(self);
    if (!readInt("Course id: ", c) || !readLine("Title: ", title) || !readDateTime("Deadline", deadline) ||
        !readInt("Maximum mark: ", max)) {
        return;
    }
    const auto a = s_.assignments.createAssignment(self, c, title, deadline, max);
    out_ << "  Created assignment [" << a.getId() << "].\n";
}

void ConsoleApp::lecturerGrantExtension(const User& self) {
    int a = 0;
    int st = 0;
    DateTime d;
    if (!readInt("Assignment id: ", a)) {
        return;
    }
    const auto asg = s_.assignments.getAssignment(self, a);
    for (const auto& u : s_.courses.studentsOf(self, asg.getCourseId())) {
        out_ << "  [" << u->getId() << "] " << u->getFullName() << "\n";
    }
    if (!readInt("Student user id (0 for the whole class): ", st) || !readDateTime("New deadline", d)) {
        return;
    }
    if (st == 0) {
        const ClassExtensionResult r = s_.assignments.grantClassExtension(self, a, d);
        out_ << "  Extension granted to " << r.extended << " student(s) until " << when(d) << ".";
        if (r.unchanged > 0) {
            out_ << " " << r.unchanged << " already had the same or a later extension and kept it.";
        }
        out_ << "\n";
        return;
    }
    s_.assignments.grantExtension(self, a, st, d);
    out_ << "  Extension granted until " << when(d) << ".\n";
}

void ConsoleApp::lecturerViewSubmissions(const User& self) {
    int a = 0;
    if (!readInt("Assignment id: ", a)) {
        return;
    }
    printSubmissions(s_.marking.submissionsFor(self, a));
}

void ConsoleApp::lecturerMark(const User& self) {
    int id = 0;
    int raw = 0;
    std::string fb;
    std::string reason;
    if (!readInt("Submission id: ", id) || !readInt("Raw mark: ", raw) || !readLine("Feedback: ", fb) ||
        !readLine("Reason for change (required when re-marking): ", reason)) {
        return;
    }
    const auto sub = s_.marking.mark(self, id, raw, fb, reason);
    out_ << "  Marked. Raw " << raw;
    if (const auto finalMark = sub.getFinalMark()) {
        out_ << ", final after late penalty " << *finalMark;
    }
    out_ << ".\n";
}

void ConsoleApp::hodMenu(const UserPtr& user) {
    runMenu(user,
            "\n-- Head of Department --\n 1 Department courses\n 2 List assignments\n"
            " 3 Review marks\n 4 Adjust a mark\n 5 Approve one submission\n 6 Approve all marked\n"
            " 7 Return to lecturer\n 8 Release marks to students\n 9 Audit history\n"
            "10 Change my password\n 0 Log out\n",
            &ConsoleApp::hodAction);
}

void ConsoleApp::hodAction(UserPtr& self, int choice) {
    switch (choice) {
        case 0:
            break;
        case 1:
            printCourses(*self);
            break;
        case 2:
            listAssignmentsOfChosenCourse(*self);
            break;
        case 3:
            hodReviewMarks(*self);
            break;
        case 4:
            hodAdjustMark(*self);
            break;
        case 5:
            hodApproveOne(*self);
            break;
        case 6:
            hodApproveAll(*self);
            break;
        case 7:
            hodReturnToLecturer(*self);
            break;
        case 8:
            hodRelease(*self);
            break;
        case 9:
            hodAudit(*self);
            break;
        case 10:
            changePassword(self);
            break;
        default:
            out_ << "  Unknown option.\n";
    }
}

void ConsoleApp::hodReviewMarks(const User& self) {
    int a = 0;
    if (!readInt("Assignment id: ", a)) {
        return;
    }
    printSubmissions(s_.moderation.reviewMarks(self, a));
}

void ConsoleApp::hodAdjustMark(const User& self) {
    int id = 0;
    int m = 0;
    std::string reason;
    if (!readInt("Submission id: ", id) || !readInt("New final mark: ", m) || !readLine("Reason: ", reason)) {
        return;
    }
    s_.moderation.adjustMark(self, id, m, reason);
    out_ << "  Mark adjusted and logged.\n";
}

void ConsoleApp::hodApproveOne(const User& self) {
    int id = 0;
    if (!readInt("Submission id: ", id)) {
        return;
    }
    const Submission seen = s_.moderation.review(self, id);
    const auto raw = seen.getRawMark();
    const auto finalMark = seen.getFinalMark();
    if (!raw || !finalMark) {
        throw BusinessRuleError("That work has not been marked yet.");
    }
    out_ << "  " << nameOf(seen.getStudentId()) << ": raw " << *raw << ", final " << *finalMark
         << ", feedback: " << seen.getFeedback() << "\n";
    std::string answer;
    if (!readLine("Approve this mark? (y/n): ", answer)) {
        return;
    }
    if (trim(answer) != "y") {
        out_ << "  Not approved.\n";
        return;
    }
    s_.moderation.approve(self, id, finalMark);
    out_ << "  Approved.\n";
}

void ConsoleApp::hodApproveAll(const User& self) {
    int a = 0;
    if (!readInt("Assignment id: ", a)) {
        return;
    }
    std::vector<ReviewedMark> reviewed;
    const auto list = s_.moderation.reviewMarks(self, a);
    printSubmissions(list);
    for (const auto& sub : list) {
        const auto finalMark = sub.getFinalMark();
        if (sub.getStatus() == SubmissionStatus::Marked && finalMark) {
            reviewed.push_back({sub.getId(), *finalMark});
        }
    }
    if (reviewed.empty()) {
        out_ << "  No marked submissions.\n";
        return;
    }
    std::string answer;
    if (!readLine(
            "Approve the " + std::to_string(reviewed.size()) + " marked submission(s) shown above? (y/n): ",
            answer)) {
        return;
    }
    if (trim(answer) != "y") {
        out_ << "  Not approved.\n";
        return;
    }
    const ApproveResult r = s_.moderation.approveReviewed(self, a, reviewed);
    out_ << "  Approved " << r.approved << " submission(s).";
    if (r.skipped > 0) {
        out_ << " " << r.skipped << " changed after you reviewed them and were skipped.";
    }
    out_ << "\n";
}

void ConsoleApp::hodReturnToLecturer(const User& self) {
    int id = 0;
    std::string comment;
    if (!readInt("Submission id: ", id) || !readLine("Comment for the lecturer: ", comment)) {
        return;
    }
    s_.moderation.returnToLecturer(self, id, comment);
    out_ << "  Returned to the lecturer.\n";
}

void ConsoleApp::hodRelease(const User& self) {
    int a = 0;
    if (!readInt("Assignment id: ", a)) {
        return;
    }
    const auto r = s_.moderation.release(self, a);
    out_ << "  Released " << r.released << " mark(s); " << r.pending << " still hidden (not moderated).\n";
}

void ConsoleApp::hodAudit(const User& self) {
    int id = 0;
    if (!readInt("Submission id: ", id)) {
        return;
    }
    printAudit(s_.moderation.auditHistory(self, id));
}

void ConsoleApp::studentMenu(const UserPtr& user) {
    runMenu(user,
            "\n-- Student --\n 1 My assignments and marks\n 2 Submit work\n"
            " 3 Change my password\n 0 Log out\n",
            &ConsoleApp::studentAction);
}

void ConsoleApp::studentAction(UserPtr& self, int choice) {
    switch (choice) {
        case 0:
            break;
        case 1:
            studentShowAssignments(*self);
            break;
        case 2:
            studentSubmit(*self);
            break;
        case 3:
            changePassword(self);
            break;
        default:
            out_ << "  Unknown option.\n";
    }
}

void ConsoleApp::studentShowAssignments(const User& self) {
    const auto rows = s_.submissions.assignmentsFor(self);
    if (rows.empty()) {
        out_ << "  (no assignments)\n";
    }
    for (const auto& r : rows) {
        out_ << "  [" << r.assignment.getId() << "] " << r.courseCode << "  " << r.assignment.getTitle()
             << "  due " << when(r.effectiveDeadline);
        if (r.effectiveDeadline > r.assignment.getDeadline()) {
            out_ << " (extended from " << when(r.assignment.getDeadline()) << ")";
        }
        out_ << "  ";
        if (!r.current) {
            out_ << "not submitted\n";
            continue;
        }
        const auto finalMark = r.current->getFinalMark();
        if (r.current->getStatus() == SubmissionStatus::Released && finalMark) {
            out_ << "MARK " << *finalMark << "/" << r.assignment.getMaxMark()
                 << "  feedback: " << r.current->getFeedback() << "\n";
        } else {
            out_ << "submitted (v" << r.current->getVersion() << "), awaiting marks\n";
        }
    }
}

void ConsoleApp::studentSubmit(const User& self) {
    int a = 0;
    std::string content;
    if (!readInt("Assignment id: ", a) || !readMultiline("Enter your answer", content)) {
        return;
    }
    const auto sub = s_.submissions.submit(self, a, content);
    out_ << "  Submitted as version " << sub.getVersion() << " at " << when(sub.getSubmittedAt());
    if (sub.getLateDays() > 0) {
        out_ << " (LATE by " << sub.getLateDays() << " day(s))";
    }
    out_ << ".\n";
}

}  // namespace lms
