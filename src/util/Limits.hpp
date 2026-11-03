// Maximum and minimum text lengths accepted from users, in one place.
// Everything that checks a length uses these names, so a limit is changed here and nowhere else.
#ifndef LMS_UTIL_LIMITS_HPP
#define LMS_UTIL_LIMITS_HPP

#include <cstddef>

namespace lms::limits {

inline constexpr std::size_t kMaxName = 100;   // department name, person's full name
inline constexpr std::size_t kMaxTitle = 100;  // course title, assignment title
inline constexpr std::size_t kMaxCourseCode = 20;
inline constexpr std::size_t kMaxStudentNumber = 20;
inline constexpr std::size_t kMinUsername = 3;
inline constexpr std::size_t kMaxUsername = 32;
inline constexpr std::size_t kMaxLoginUsername = 64;  // longer input is refused before any lookup
inline constexpr std::size_t kMaxLoginPassword = 128;
inline constexpr std::size_t kMaxFeedback = 5000;
inline constexpr std::size_t kMaxReason = 500;   // reason for a mark change, typed by a person
inline constexpr std::size_t kMaxComment = 450;  // comment when returning work to the lecturer
// Audit text is a reason plus a short automatic suffix, so it must be longer than kMaxReason.
inline constexpr std::size_t kMaxAuditText = 600;

}  // namespace lms::limits

#endif
