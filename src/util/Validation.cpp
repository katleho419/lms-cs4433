#include "util/Validation.hpp"

#include <cctype>

#include "util/Errors.hpp"

namespace lms {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

std::string requireText(const std::string& text, const std::string& field, std::size_t maxLength) {
    std::string value = trim(text);
    if (value.empty()) throw ValidationError(field + " must not be empty.");
    if (value.size() > maxLength) {
        throw ValidationError(field + " must be at most " + std::to_string(maxLength) + " characters.");
    }
    return value;
}

int requireRange(int value, const std::string& field, int lo, int hi) {
    if (value < lo || value > hi) {
        throw ValidationError(field + " must be between " + std::to_string(lo) + " and " +
                              std::to_string(hi) + ".");
    }
    return value;
}

}  // namespace lms
