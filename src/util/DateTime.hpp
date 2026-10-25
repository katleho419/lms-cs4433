// A point in time with one-second resolution, stored as seconds since the
// Unix epoch (UTC). The database stores ISO 8601 text such as
// 2026-11-07T23:59:00Z. Local-time conversion is done only at the UI edge.
#ifndef LMS_UTIL_DATETIME_HPP
#define LMS_UTIL_DATETIME_HPP

#include <cstdint>
#include <string>

namespace lms {

class DateTime {
public:
    DateTime() = default;

    static DateTime fromEpochSeconds(std::int64_t seconds);
    // "YYYY-MM-DDTHH:MM:SSZ". Throws ValidationError on bad input.
    static DateTime parseIso(const std::string& text);
    // "YYYY-MM-DD HH:MM" in local time with the given UTC offset in minutes.
    static DateTime parseLocal(const std::string& text, int utcOffsetMinutes);

    std::string toIso() const;
    std::string toLocal(int utcOffsetMinutes) const;

    std::int64_t epochSeconds() const { return epoch_; }
    DateTime plusSeconds(std::int64_t seconds) const { return DateTime(epoch_ + seconds); }
    std::int64_t secondsSince(const DateTime& other) const { return epoch_ - other.epoch_; }

    friend bool operator==(const DateTime& a, const DateTime& b) { return a.epoch_ == b.epoch_; }
    friend bool operator!=(const DateTime& a, const DateTime& b) { return a.epoch_ != b.epoch_; }
    friend bool operator<(const DateTime& a, const DateTime& b) { return a.epoch_ < b.epoch_; }
    friend bool operator<=(const DateTime& a, const DateTime& b) { return a.epoch_ <= b.epoch_; }
    friend bool operator>(const DateTime& a, const DateTime& b) { return a.epoch_ > b.epoch_; }
    friend bool operator>=(const DateTime& a, const DateTime& b) { return a.epoch_ >= b.epoch_; }

private:
    explicit DateTime(std::int64_t epoch) : epoch_(epoch) {}
    std::int64_t epoch_ = 0;
};

}  // namespace lms

#endif
