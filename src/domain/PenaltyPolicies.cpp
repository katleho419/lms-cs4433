#include "domain/PenaltyPolicies.hpp"

#include <algorithm>

#include "util/Errors.hpp"

namespace lms {
namespace {

void checkInputs(int rawMark, int maxMark, int lateDays) {
    if (maxMark <= 0) throw ValidationError("The maximum mark must be positive.");
    if (rawMark < 0 || rawMark > maxMark) {
        throw ValidationError("The raw mark must be between 0 and " + std::to_string(maxMark) + ".");
    }
    if (lateDays < 0) throw ValidationError("Late days cannot be negative.");
}

}  // namespace

DailyPercentPenalty::DailyPercentPenalty(int percentPerDay, int maxLateDays)
    : percentPerDay_(percentPerDay), maxLateDays_(maxLateDays) {
    if (percentPerDay < 0 || percentPerDay > 100) {
        throw ValidationError("The penalty percentage must be between 0 and 100.");
    }
    if (maxLateDays < 0) throw ValidationError("The late window cannot be negative.");
}

int DailyPercentPenalty::calculate(int rawMark, int maxMark, int lateDays) const {
    checkInputs(rawMark, maxMark, lateDays);
    if (lateDays == 0) return rawMark;
    if (lateDays > maxLateDays_) return 0;
    const long long numerator = static_cast<long long>(maxMark) * percentPerDay_ * lateDays;
    const int deduction = static_cast<int>((numerator + 50) / 100);
    return std::max(0, rawMark - deduction);
}

int NoPenalty::calculate(int rawMark, int maxMark, int lateDays) const {
    checkInputs(rawMark, maxMark, lateDays);
    return rawMark;
}

}  // namespace lms
