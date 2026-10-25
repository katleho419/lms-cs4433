// Roles and the permissions each role holds (role-based access control).
#ifndef LMS_DOMAIN_ROLE_HPP
#define LMS_DOMAIN_ROLE_HPP

#include <string>

namespace lms {

enum class Role { Admin, Lecturer, Hod, Student };

enum class Permission {
    ManageAccounts,
    ManageDepartments,
    ManageCourses,
    AssignLecturers,
    EnrolStudents,
    CreateAssignment,
    GrantExtension,
    MarkSubmission,
    ModerateMarks,
    ReleaseMarks,
    ViewAuditHistory,
    SubmitWork,
    ViewOwnMarks
};

// Text stored in the database: ADMIN, LECTURER, HOD, STUDENT.
std::string toString(Role role);
// Throws ValidationError for unknown text.
Role roleFromString(const std::string& text);
// Friendly name for screens: "Administrator", "Head of Department", ...
std::string displayName(Role role);

}  // namespace lms

#endif
