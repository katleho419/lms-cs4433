// Factory pattern (design: Figure 1a). One place knows how to build each kind
// of user, so no other code branches on role when creating accounts.
#ifndef LMS_DOMAIN_USERFACTORY_HPP
#define LMS_DOMAIN_USERFACTORY_HPP

#include <memory>
#include <string>

#include "domain/Role.hpp"
#include "domain/User.hpp"

namespace lms {

// Input for creating a new account. The password is plain text here and is
// hashed immediately by the factory; it is never stored.
struct UserData {
    std::string username;
    std::string fullName;
    std::string password;
    std::string studentNumber;  // students only
    int departmentId = 0;       // lecturers and heads of department only
};

// A row read back from the database: the password is already hashed.
struct UserRecord {
    int id = 0;
    std::string username;
    std::string passwordHash;
    std::string salt;
    std::string fullName;
    Role role = Role::Student;
    std::string studentNumber;
    int departmentId = 0;
};

class UserFactory {
public:
    // Validates the data, applies the password policy, creates a salt, hashes
    // the password and returns the right subclass. Throws ValidationError.
    static std::unique_ptr<User> createUser(Role role, const UserData& data);

    // Rebuilds a user from stored data without touching the password.
    static std::unique_ptr<User> restore(const UserRecord& record);
};

}  // namespace lms

#endif
