// Time source. Services receive an IClock so that late-submission rules can be
// tested without waiting for real time to pass.
#ifndef LMS_UTIL_CLOCK_HPP
#define LMS_UTIL_CLOCK_HPP

#include <chrono>

#include "util/DateTime.hpp"

namespace lms {

class IClock {
public:
    virtual ~IClock() = default;
    virtual DateTime now() const = 0;
};

class SystemClock : public IClock {
public:
    DateTime now() const override {
        const auto since = std::chrono::system_clock::now().time_since_epoch();
        return DateTime::fromEpochSeconds(std::chrono::duration_cast<std::chrono::seconds>(since).count());
    }
};

// A clock that only moves when told to. Used by tests and by the optional
// test-clock build (see docs/BUILD.md).
class FixedClock : public IClock {
public:
    explicit FixedClock(DateTime start) : now_(start) {}
    DateTime now() const override { return now_; }
    void set(DateTime t) { now_ = t; }

private:
    DateTime now_;
};

}  // namespace lms

#endif
