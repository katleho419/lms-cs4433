#include "security/PasswordHasher.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdint>
#include <sstream>
#include <vector>

#include "util/Errors.hpp"

namespace lms {
namespace {

constexpr std::uint64_t kCost = 32768;  // N = 2^15, about 32 MiB of memory
constexpr std::uint64_t kBlockSize = 8;
constexpr std::uint64_t kParallelism = 1;
constexpr std::size_t kKeyBytes = 32;
constexpr std::size_t kSaltBytes = 16;

// Upper limits for parameters read back from the database, so a corrupted
// record cannot make the program allocate huge amounts of memory.
constexpr std::uint64_t kMaxCost = 1ULL << 17;
constexpr std::uint64_t kMaxBlockSize = 16;
constexpr std::uint64_t kMaxParallelism = 4;

std::string toHex(const std::vector<unsigned char>& bytes) {
    static const char* const digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fromHex(const std::string& text, std::vector<unsigned char>& out) {
    if (text.empty() || text.size() % 2 != 0) return false;
    out.clear();
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = hexValue(text[i]);
        const int lo = hexValue(text[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<unsigned char>(hi * 16 + lo));
    }
    return true;
}

bool deriveKey(const std::string& plain, const std::vector<unsigned char>& salt, std::uint64_t n,
               std::uint64_t r, std::uint64_t p, std::vector<unsigned char>& key) {
    key.assign(kKeyBytes, 0);
    const std::uint64_t maxMemory = 2 * 128 * n * r + 1024 * 1024;
    return EVP_PBE_scrypt(plain.data(), plain.size(), salt.data(), salt.size(), n, r, p, maxMemory,
                          key.data(), key.size()) == 1;
}

std::vector<std::string> split(const std::string& text, char sep) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, sep)) parts.push_back(part);
    return parts;
}

bool parseNumber(const std::string& text, std::uint64_t& out) {
    if (text.empty() || text.size() > 9) return false;
    out = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        out = out * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return true;
}

}  // namespace

std::string PasswordHasher::generateSalt() {
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw LmsError("Could not generate a random salt.");
    }
    return toHex(salt);
}

std::string PasswordHasher::hash(const std::string& plain, const std::string& saltHex) {
    std::vector<unsigned char> salt;
    if (!fromHex(saltHex, salt)) throw LmsError("The salt is not valid hexadecimal.");
    std::vector<unsigned char> key;
    if (!deriveKey(plain, salt, kCost, kBlockSize, kParallelism, key)) {
        throw LmsError("Password hashing failed.");
    }
    return "scrypt$" + std::to_string(kCost) + "$" + std::to_string(kBlockSize) + "$" +
           std::to_string(kParallelism) + "$" + toHex(key);
}

bool PasswordHasher::verify(const std::string& plain, const std::string& saltHex,
                            const std::string& stored) {
    const std::vector<std::string> parts = split(stored, '$');
    if (parts.size() != 5 || parts[0] != "scrypt") return false;

    std::uint64_t n = 0;
    std::uint64_t r = 0;
    std::uint64_t p = 0;
    if (!parseNumber(parts[1], n) || !parseNumber(parts[2], r) || !parseNumber(parts[3], p)) return false;
    if (n < 1024 || n > kMaxCost || (n & (n - 1)) != 0) return false;
    if (r < 1 || r > kMaxBlockSize || p < 1 || p > kMaxParallelism) return false;

    std::vector<unsigned char> salt;
    std::vector<unsigned char> expected;
    if (!fromHex(saltHex, salt) || !fromHex(parts[4], expected) || expected.size() != kKeyBytes) return false;

    std::vector<unsigned char> actual;
    if (!deriveKey(plain, salt, n, r, p, actual)) return false;
    return CRYPTO_memcmp(actual.data(), expected.data(), kKeyBytes) == 0;
}

}  // namespace lms
