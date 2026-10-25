#include "service/AuthService.hpp"

#include "security/PasswordHasher.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {
namespace {

constexpr const char* kLoginFailed = "Invalid username or password.";

// Hashing a password costs real time. When the username does not exist we
// still hash once, so response time does not reveal which usernames exist.
void spendHashingTime(const std::string& password) {
    static const std::string salt = PasswordHasher::generateSalt();
    static const std::string hash = PasswordHasher::hash("not-a-real-password-1", salt);
    PasswordHasher::verify(password, salt, hash);
}

}  // namespace

UserPtr AuthService::login(const std::string& username, const std::string& password) const {
    if (username.size() > 64 || password.size() > 128) {
        spendHashingTime("x");
        throw AuthenticationError(kLoginFailed);
    }
    const std::optional<UserPtr> user = users_.findByUsername(trim(username));
    if (!user) {
        spendHashingTime(password);
        throw AuthenticationError(kLoginFailed);
    }
    if (!(*user)->verifyPassword(password)) throw AuthenticationError(kLoginFailed);
    return *user;
}

void AuthService::changePassword(UserPtr& user, const std::string& oldPassword,
                                 const std::string& newPassword) {
    try {
        user->changePassword(oldPassword, newPassword);
        users_.save(user);
    } catch (...) {
        // Put the in-memory user back in step with what is stored.
        const std::optional<UserPtr> stored = users_.findById(user->getId());
        if (stored) user = *stored;
        throw;
    }
}

UserPtr AuthService::createInitialAdministrator(const UserData& data) {
    if (!noUsersExist()) throw BusinessRuleError("An administrator already exists.");
    UserPtr admin(UserFactory::createUser(Role::Admin, data));
    users_.save(admin);
    return admin;
}

}  // namespace lms
