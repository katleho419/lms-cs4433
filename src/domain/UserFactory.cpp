#include "domain/UserFactory.hpp"

#include <cctype>

#include "security/PasswordHasher.hpp"
#include "security/PasswordPolicy.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {
namespace {

void validateUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 32) {
        throw ValidationError("The username must be 3 to 32 characters long.");
    }
    for (char c : username) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '_' || c == '.' || c == '-')) {
            throw ValidationError("The username may only contain letters, digits, '_', '.' and '-'.");
        }
    }
}

}  // namespace

std::unique_ptr<User> UserFactory::createUser(Role role, const UserData& data) {
    const std::string username = trim(data.username);
    validateUsername(username);
    const std::string fullName = requireText(data.fullName, "The full name", 100);
    PasswordPolicy::validate(data.password, username);

    std::string studentNumber;
    if (role == Role::Student) {
        studentNumber = requireText(data.studentNumber, "The student number", 20);
    }
    if ((role == Role::Lecturer || role == Role::Hod) && data.departmentId <= 0) {
        throw ValidationError("A department is required for this role.");
    }

    const std::string salt = PasswordHasher::generateSalt();
    const std::string hash = PasswordHasher::hash(data.password, salt);

    switch (role) {
        case Role::Admin:
            return std::make_unique<Administrator>(0, username, hash, salt, fullName);
        case Role::Lecturer:
            return std::make_unique<Lecturer>(0, username, hash, salt, fullName, data.departmentId);
        case Role::Hod:
            return std::make_unique<HeadOfDepartment>(0, username, hash, salt, fullName, data.departmentId);
        case Role::Student:
            return std::make_unique<Student>(0, username, hash, salt, fullName, studentNumber);
    }
    throw ValidationError("Unknown role.");
}

std::unique_ptr<User> UserFactory::restore(const UserRecord& r) {
    switch (r.role) {
        case Role::Admin:
            return std::make_unique<Administrator>(r.id, r.username, r.passwordHash, r.salt, r.fullName);
        case Role::Lecturer:
            return std::make_unique<Lecturer>(r.id, r.username, r.passwordHash, r.salt, r.fullName,
                                              r.departmentId);
        case Role::Hod:
            return std::make_unique<HeadOfDepartment>(r.id, r.username, r.passwordHash, r.salt,
                                                      r.fullName, r.departmentId);
        case Role::Student:
            return std::make_unique<Student>(r.id, r.username, r.passwordHash, r.salt, r.fullName,
                                             r.studentNumber);
    }
    throw ValidationError("Unknown role.");
}

}  // namespace lms
