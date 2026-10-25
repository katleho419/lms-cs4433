// Login, password change and first-run set-up.
#ifndef LMS_SERVICE_AUTHSERVICE_HPP
#define LMS_SERVICE_AUTHSERVICE_HPP

#include <string>

#include "data/RepositoryInterfaces.hpp"
#include "domain/UserFactory.hpp"

namespace lms {

class AuthService {
public:
    explicit AuthService(IUserRepository& users) : users_(users) {}

    // Returns the user, or throws AuthenticationError with one generic
    // message so an attacker cannot tell a wrong username from a wrong password.
    UserPtr login(const std::string& username, const std::string& password) const;

    // Checks the old password, applies the password policy, saves the new hash.
    void changePassword(UserPtr& user, const std::string& oldPassword, const std::string& newPassword);

    // True on a brand-new installation, before any account exists.
    bool noUsersExist() const { return users_.countAll() == 0; }
    // Creates the first administrator. Throws BusinessRuleError if any user already exists.
    UserPtr createInitialAdministrator(const UserData& data);

private:
    IUserRepository& users_;
};

}  // namespace lms

#endif
