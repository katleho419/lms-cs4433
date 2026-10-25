// Strategy pattern (design: Figure 1c). The late-penalty rule is the rule most
// likely to change, so MarkingService depends only on this interface.
#ifndef LMS_DOMAIN_IPENALTYPOLICY_HPP
#define LMS_DOMAIN_IPENALTYPOLICY_HPP

namespace lms {

class IPenaltyPolicy {
public:
    virtual ~IPenaltyPolicy() = default;
    // Returns the final mark after the penalty. 0 <= result <= rawMark.
    virtual int calculate(int rawMark, int maxMark, int lateDays) const = 0;
};

}  // namespace lms

#endif
