#include "domain/User.hpp"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "security/PasswordHasher.hpp"
#include "security/PasswordPolicy.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

bool isOneOf(Permission p, std::initializer_list<Permission> allowed) {
    return std::find(allowed.begin(), allowed.end(), p) != allowed.end();
}

}  // namespace

User::User(int id, std::string username, std::string passwordHash, std::string salt, std::string fullName)
    : id_(id),
      username_(std::move(username)),
      passwordHash_(std::move(passwordHash)),
      salt_(std::move(salt)),
      fullName_(std::move(fullName)) {}

bool User::verifyPassword(const std::string& plain) const {
    return PasswordHasher::verify(plain, salt_, passwordHash_);
}

void User::changePassword(const std::string& oldPassword, const std::string& newPassword) {
    if (!verifyPassword(oldPassword)) throw AuthenticationError("The current password is not correct.");
    PasswordPolicy::validate(newPassword, username_);
    const std::string newSalt = PasswordHasher::generateSalt();
    passwordHash_ = PasswordHasher::hash(newPassword, newSalt);
    salt_ = newSalt;
}

bool Administrator::can(Permission permission) const {
    return isOneOf(permission, {Permission::ManageAccounts, Permission::ManageDepartments,
                                Permission::ManageCourses, Permission::AssignLecturers,
                                Permission::EnrolStudents});
}

Lecturer::Lecturer(int id, std::string username, std::string passwordHash, std::string salt,
                   std::string fullName, int departmentId)
    : User(id, std::move(username), std::move(passwordHash), std::move(salt), std::move(fullName)),
      departmentId_(departmentId) {}

bool Lecturer::can(Permission permission) const {
    return isOneOf(permission,
                   {Permission::CreateAssignment, Permission::GrantExtension, Permission::MarkSubmission});
}

HeadOfDepartment::HeadOfDepartment(int id, std::string username, std::string passwordHash,
                                   std::string salt, std::string fullName, int departmentId)
    : User(id, std::move(username), std::move(passwordHash), std::move(salt), std::move(fullName)),
      departmentId_(departmentId) {}

bool HeadOfDepartment::can(Permission permission) const {
    return isOneOf(permission,
                   {Permission::ModerateMarks, Permission::ReleaseMarks, Permission::ViewAuditHistory});
}

Student::Student(int id, std::string username, std::string passwordHash, std::string salt,
                 std::string fullName, std::string studentNumber)
    : User(id, std::move(username), std::move(passwordHash), std::move(salt), std::move(fullName)),
      studentNumber_(std::move(studentNumber)) {}

bool Student::can(Permission permission) const {
    return isOneOf(permission, {Permission::SubmitWork, Permission::ViewOwnMarks});
}

}  // namespace lms
