// Head of Department use cases: review, adjust, approve, return and release
// marks for their own department. Students see nothing until release (BR-05).
#ifndef LMS_SERVICE_MODERATIONSERVICE_HPP
#define LMS_SERVICE_MODERATIONSERVICE_HPP

#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "service/AuditService.hpp"
#include "service/CourseService.hpp"
#include "service/IUnitOfWork.hpp"

namespace lms {

struct ReleaseResult {
    int released = 0;  // moderated marks that became visible to students
    int pending = 0;   // work that is not yet moderated and stays hidden
};

class ModerationService {
public:
    ModerationService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                      CourseService& access, AuditService& audit, IUnitOfWork& unitOfWork)
        : submissions_(submissions),
          assignments_(assignments),
          access_(access),
          audit_(audit),
          unitOfWork_(unitOfWork) {}

    // Current version of every student's work for an assignment in the HoD's department (BR-13, BR-14).
    std::vector<Submission> reviewMarks(const User& hod, int assignmentId) const;

    // Changes the final mark of marked or moderated work. A reason is required (BR-12).
    Submission adjustMark(const User& hod, int submissionId, int newMark, const std::string& reason);

    // MARKED -> MODERATED.
    Submission approve(const User& hod, int submissionId);
    // Approves every MARKED submission of the assignment. Returns how many.
    int approveAll(const User& hod, int assignmentId);

    // MARKED -> SUBMITTED or LATE so the lecturer can mark again.
    Submission returnToLecturer(const User& hod, int submissionId, const std::string& comment);

    // MODERATED -> RELEASED for the whole assignment (BR-15).
    ReleaseResult release(const User& hod, int assignmentId);

    std::vector<AuditEntry> auditHistory(const User& hod, int submissionId) const;

private:
    Assignment requireAssignmentInDepartment(const User& hod, int assignmentId) const;
    // Loads a submission and checks that it belongs to the HoD's department.
    Submission loadForHod(const User& hod, int submissionId) const;
    void requireCurrentVersion(const Submission& submission) const;

    ISubmissionRepository& submissions_;
    IAssignmentRepository& assignments_;
    CourseService& access_;
    AuditService& audit_;
    IUnitOfWork& unitOfWork_;
};

}  // namespace lms

#endif
