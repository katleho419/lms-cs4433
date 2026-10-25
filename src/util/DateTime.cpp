#include "util/DateTime.hpp"

#include <cstdio>
#include <string>

#include "util/Errors.hpp"

namespace lms {
namespace {

constexpr std::int64_t kSecondsPerDay = 86400;
constexpr int kMinYear = 1970;
constexpr int kMaxYear = 9999;

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's algorithm).
std::int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

void civilFromDays(std::int64_t z, int& year, unsigned& month, unsigned& day) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = static_cast<int>(y + (month <= 2 ? 1 : 0));
}

std::int64_t floorDiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

int digits(const std::string& text, std::size_t pos, std::size_t len) {
    int value = 0;
    for (std::size_t i = pos; i < pos + len; ++i) {
        if (text[i] < '0' || text[i] > '9') throw ValidationError("Invalid date or time: " + text);
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

void expectChar(const std::string& text, std::size_t pos, char c) {
    if (text[pos] != c) throw ValidationError("Invalid date or time: " + text);
}

// Builds epoch seconds from fields, rejecting impossible dates such as 31 April.
std::int64_t buildEpoch(const std::string& original, int year, int month, int day, int hour,
                        int minute, int second) {
    if (year < kMinYear || year > kMaxYear || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59) {
        throw ValidationError("Invalid date or time: " + original);
    }
    const std::int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    int y2 = 0;
    unsigned m2 = 0;
    unsigned d2 = 0;
    civilFromDays(days, y2, m2, d2);
    if (y2 != year || static_cast<int>(m2) != month || static_cast<int>(d2) != day) {
        throw ValidationError("Invalid date or time: " + original);
    }
    return days * kSecondsPerDay + hour * 3600 + minute * 60 + second;
}

}  // namespace

DateTime DateTime::fromEpochSeconds(std::int64_t seconds) { return DateTime(seconds); }

DateTime DateTime::parseIso(const std::string& text) {
    if (text.size() != 20) throw ValidationError("Invalid date or time: " + text);
    expectChar(text, 4, '-');
    expectChar(text, 7, '-');
    expectChar(text, 10, 'T');
    expectChar(text, 13, ':');
    expectChar(text, 16, ':');
    expectChar(text, 19, 'Z');
    return DateTime(buildEpoch(text, digits(text, 0, 4), digits(text, 5, 2), digits(text, 8, 2),
                               digits(text, 11, 2), digits(text, 14, 2), digits(text, 17, 2)));
}

DateTime DateTime::parseLocal(const std::string& text, int utcOffsetMinutes) {
    if (text.size() != 16) throw ValidationError("Use the format YYYY-MM-DD HH:MM, for example 2026-11-07 23:59.");
    try {
        expectChar(text, 4, '-');
        expectChar(text, 7, '-');
        expectChar(text, 10, ' ');
        expectChar(text, 13, ':');
        const std::int64_t local = buildEpoch(text, digits(text, 0, 4), digits(text, 5, 2),
                                              digits(text, 8, 2), digits(text, 11, 2),
                                              digits(text, 14, 2), 0);
        return DateTime(local - static_cast<std::int64_t>(utcOffsetMinutes) * 60);
    } catch (const ValidationError&) {
        throw ValidationError("Use a real date in the format YYYY-MM-DD HH:MM, for example 2026-11-07 23:59.");
    }
}

std::string DateTime::toIso() const {
    const std::int64_t days = floorDiv(epoch_, kSecondsPerDay);
    const std::int64_t secondOfDay = epoch_ - days * kSecondsPerDay;
    int y = 0;
    unsigned m = 0;
    unsigned d = 0;
    civilFromDays(days, y, m, d);
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02uT%02d:%02d:%02dZ", y, m, d,
                  static_cast<int>(secondOfDay / 3600), static_cast<int>((secondOfDay % 3600) / 60),
                  static_cast<int>(secondOfDay % 60));
    return buffer;
}

std::string DateTime::toLocal(int utcOffsetMinutes) const {
    const std::int64_t local = epoch_ + static_cast<std::int64_t>(utcOffsetMinutes) * 60;
    const std::int64_t days = floorDiv(local, kSecondsPerDay);
    const std::int64_t secondOfDay = local - days * kSecondsPerDay;
    int y = 0;
    unsigned m = 0;
    unsigned d = 0;
    civilFromDays(days, y, m, d);
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02u %02d:%02d", y, m, d,
                  static_cast<int>(secondOfDay / 3600), static_cast<int>((secondOfDay % 3600) / 60));
    return buffer;
}

}  // namespace lms
