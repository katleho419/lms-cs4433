// Rules a new password must meet (SRS security requirement).
#ifndef LMS_SECURITY_PASSWORDPOLICY_HPP
#define LMS_SECURITY_PASSWORDPOLICY_HPP

#include <algorithm>
#include <cctype>
#include <string>

#include "util/Errors.hpp"

namespace lms {

class PasswordPolicy {
public:
    static constexpr std::size_t kMinLength = 8;
    static constexpr std::size_t kMaxLength = 128;

    // Throws ValidationError describing the first rule that is broken.
    static void validate(const std::string& password, const std::string& username) {
        if (password.size() < kMinLength) {
            throw ValidationError("The password must be at least " + std::to_string(kMinLength) +
                                  " characters long.");
        }
        if (password.size() > kMaxLength) {
            throw ValidationError("The password must be at most " + std::to_string(kMaxLength) +
                                  " characters long.");
        }
        const auto isLetter = [](unsigned char c) { return std::isalpha(c) != 0; };
        const auto isDigit = [](unsigned char c) { return std::isdigit(c) != 0; };
        if (!std::any_of(password.begin(), password.end(), isLetter) ||
            !std::any_of(password.begin(), password.end(), isDigit)) {
            throw ValidationError("The password must contain at least one letter and one digit.");
        }
        if (lower(password) == lower(username)) {
            throw ValidationError("The password must not be the same as the username.");
        }
    }

private:
    static std::string lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }
};

}  // namespace lms

#endif
