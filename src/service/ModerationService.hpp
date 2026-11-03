// Head of Department use cases: review, adjust, approve, return and release
// marks for their own department. Students see nothing until release (BR-05).
#ifndef LMS_SERVICE_MODERATIONSERVICE_HPP
#define LMS_SERVICE_MODERATIONSERVICE_HPP

#include <optional>
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

// What the Head of Department saw on screen: used to refuse an approval if the
// lecturer changed the mark in the meantime (UC-03 E4).
struct ReviewedMark {
    int submissionId = 0;
    int finalMark = 0;
};

struct ApproveResult {
    int approved = 0;  // approved because the mark was unchanged since review
    int skipped = 0;   // marked work that changed or was not reviewed, left for another review
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

    // One submission as the HoD would see it before deciding (department checked).
    Submission review(const User& hod, int submissionId) const;

    // MARKED -> MODERATED. If reviewedFinalMark is given and the stored mark is
    // different, the work changed after the review and nothing is approved (UC-03 E4).
    Submission approve(const User& hod, int submissionId, std::optional<int> reviewedFinalMark = std::nullopt);
    // Approves every MARKED submission of the assignment. Returns how many.
    int approveAll(const User& hod, int assignmentId);
    // Approves only the MARKED submissions whose mark still equals the reviewed one.
    ApproveResult approveReviewed(const User& hod, int assignmentId, const std::vector<ReviewedMark>& reviewed);

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
