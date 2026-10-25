// Lecturer use case: mark a submission and write feedback. Applies the late
// penalty through the injected IPenaltyPolicy (Strategy) and records every
// change in the audit log inside the same transaction.
#ifndef LMS_SERVICE_MARKINGSERVICE_HPP
#define LMS_SERVICE_MARKINGSERVICE_HPP

#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "domain/IPenaltyPolicy.hpp"
#include "service/AuditService.hpp"
#include "service/CourseService.hpp"
#include "service/IUnitOfWork.hpp"

namespace lms {

class MarkingService {
public:
    MarkingService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                   CourseService& access, const IPenaltyPolicy& penalty, AuditService& audit,
                   IUnitOfWork& unitOfWork)
        : submissions_(submissions),
          assignments_(assignments),
          access_(access),
          penalty_(penalty),
          audit_(audit),
          unitOfWork_(unitOfWork) {}

    // Current version of each student's work for an assignment the lecturer teaches.
    std::vector<Submission> submissionsFor(const User& lecturer, int assignmentId) const;

    // Marks (or re-marks) the current version of a submission. The reason is
    // required when changing a mark that already exists (BR-09). Work that is
    // moderated or released can only be changed by the Head of Department (BR-10).
    Submission mark(const User& lecturer, int submissionId, int rawMark, const std::string& feedback,
                    const std::string& reason);

private:
    ISubmissionRepository& submissions_;
    IAssignmentRepository& assignments_;
    CourseService& access_;
    const IPenaltyPolicy& penalty_;
    AuditService& audit_;
    IUnitOfWork& unitOfWork_;
};

}  // namespace lms

#endif
