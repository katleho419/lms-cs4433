#ifndef LMS_DOMAIN_PENALTYPOLICIES_HPP
#define LMS_DOMAIN_PENALTYPOLICIES_HPP

#include "domain/IPenaltyPolicy.hpp"

namespace lms {

// The school rule (BR-08): each late day removes percentPerDay percent of the
// MAXIMUM mark, for up to maxLateDays days. Later than that the mark is zero.
// The deduction is rounded to the nearest whole mark (halves round up).
class DailyPercentPenalty : public IPenaltyPolicy {
public:
    explicit DailyPercentPenalty(int percentPerDay = 10, int maxLateDays = 3);
    int calculate(int rawMark, int maxMark, int lateDays) const override;

private:
    int percentPerDay_;
    int maxLateDays_;
};

// No penalty at all. Useful for tests and for schools that switch the rule off.
class NoPenalty : public IPenaltyPolicy {
public:
    int calculate(int rawMark, int maxMark, int lateDays) const override;
};

}  // namespace lms

#endif
