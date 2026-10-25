#include "service/AuditService.hpp"

#include "util/Validation.hpp"

namespace lms {

void AuditService::record(int submissionId, int userId, std::optional<int> oldValue, int newValue,
                          const std::string& reason) {
    AuditEntry entry(0, submissionId, userId, clock_.now(), oldValue, newValue,
                     requireText(reason, "The reason", 600));
    audits_.save(entry);
}

std::vector<AuditEntry> AuditService::history(int submissionId) const {
    return audits_.findBySubmission(submissionId);
}

}  // namespace lms
