// Writes and reads the audit log: who changed a mark, when, from what, to
// what, and why (BR-12, BR-16).
#ifndef LMS_SERVICE_AUDITSERVICE_HPP
#define LMS_SERVICE_AUDITSERVICE_HPP

#include <optional>
#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "util/Clock.hpp"

namespace lms {

class AuditService {
public:
    AuditService(IAuditRepository& audits, const IClock& clock) : audits_(audits), clock_(clock) {}

    // Adds one entry stamped with the current time. The reason is mandatory.
    // Call inside the same transaction as the change it describes.
    void record(int submissionId, int userId, std::optional<int> oldValue, int newValue,
                const std::string& reason);

    std::vector<AuditEntry> history(int submissionId) const;

private:
    IAuditRepository& audits_;
    const IClock& clock_;
};

}  // namespace lms

#endif
