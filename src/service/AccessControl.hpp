// Role-based access check used at the top of every service operation.
#ifndef LMS_SERVICE_ACCESSCONTROL_HPP
#define LMS_SERVICE_ACCESSCONTROL_HPP

#include <string>

#include "domain/User.hpp"
#include "util/Errors.hpp"

namespace lms {

// Throws AuthorizationError unless the actor's role holds the permission.
// `action` completes the sentence "You are not allowed to ...".
inline void requirePermission(const User& actor, Permission permission, const std::string& action) {
    if (!actor.can(permission)) {
        throw AuthorizationError("You are not allowed to " + action + ".");
    }
}

}  // namespace lms

#endif
