// Small input-validation helpers shared by services and the UI.
#ifndef LMS_UTIL_VALIDATION_HPP
#define LMS_UTIL_VALIDATION_HPP

#include <cstddef>
#include <string>

namespace lms {

// Removes leading and trailing whitespace.
std::string trim(const std::string& text);

// Returns the trimmed text. Throws ValidationError if it is empty or longer
// than maxLength characters. `field` is used in the message.
std::string requireText(const std::string& text, const std::string& field, std::size_t maxLength);

// Throws ValidationError unless lo <= value <= hi.
int requireRange(int value, const std::string& field, int lo, int hi);

}  // namespace lms

#endif
