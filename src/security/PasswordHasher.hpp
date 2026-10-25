// Salted password hashing with scrypt from OpenSSL 3.
//
// Why scrypt: it is a memory-hard key-derivation function, so guessing
// passwords on GPUs or custom hardware is far more expensive than with a plain
// SHA hash. OpenSSL is a recognised, widely audited library that is already
// present on most systems. See docs/SECURITY.md for the full justification.
#ifndef LMS_SECURITY_PASSWORDHASHER_HPP
#define LMS_SECURITY_PASSWORDHASHER_HPP

#include <string>

namespace lms {

class PasswordHasher {
public:
    // 16 random bytes from the OS-seeded OpenSSL generator, as 32 hex characters.
    static std::string generateSalt();

    // Returns "scrypt$N$r$p$<hex key>". The parameters are stored with the hash
    // so they can be raised later without breaking existing accounts.
    static std::string hash(const std::string& plain, const std::string& saltHex);

    // Constant-time comparison. Returns false (never throws) for malformed
    // stored values.
    static bool verify(const std::string& plain, const std::string& saltHex, const std::string& stored);
};

}  // namespace lms

#endif
