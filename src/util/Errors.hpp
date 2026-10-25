// Exception hierarchy used across all layers.
// The UI catches LmsError and shows e.what() to the user; anything else is
// treated as an unexpected fault and reported without details.
#ifndef LMS_UTIL_ERRORS_HPP
#define LMS_UTIL_ERRORS_HPP

#include <stdexcept>
#include <string>

namespace lms {

class LmsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Input is malformed or out of range.
class ValidationError : public LmsError {
public:
    using LmsError::LmsError;
};

// Wrong username or password.
class AuthenticationError : public LmsError {
public:
    using LmsError::LmsError;
};

// The user's role (or relationship to the course) does not allow the action.
class AuthorizationError : public LmsError {
public:
    using LmsError::LmsError;
};

// A requested record does not exist.
class NotFoundError : public LmsError {
public:
    using LmsError::LmsError;
};

// A business rule (deadline, state machine, moderation) forbids the action.
class BusinessRuleError : public LmsError {
public:
    using LmsError::LmsError;
};

// SQLite reported a problem.
class DatabaseError : public LmsError {
public:
    using LmsError::LmsError;
};

}  // namespace lms

#endif
