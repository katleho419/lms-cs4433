#include "service/ModerationService.hpp"

#include "service/AccessControl.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {

Assignment ModerationService::requireAssignmentInDepartment(const User& hod, int assignmentId) const {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) throw NotFoundError("There is no assignment with id " + std::to_string(assignmentId) + ".");
    access_.requireHodOf(hod, access_.requireCourse(assignment->getCourseId()));
    return *assignment;
}

Submission ModerationService::loadForHod(const User& hod, int submissionId) const {
    const std::optional<Submission> submission = submissions_.findById(submissionId);
    if (!submission) throw NotFoundError("There is no submission with id " + std::to_string(submissionId) + ".");
    requireAssignmentInDepartment(hod, submission->getAssignmentId());
    return *submission;
}

void ModerationService::requireCurrentVersion(const Submission& submission) const {
    const std::optional<Submission> latest =
        submissions_.findLatest(submission.getAssignmentId(), submission.getStudentId());
    if (latest && latest->getId() != submission.getId()) {
        throw BusinessRuleError("This is an older version of the student's work.");
    }
}

std::vector<Submission> ModerationService::reviewMarks(const User& hod, int assignmentId) const {
    requirePermission(hod, Permission::ModerateMarks, "review marks");
    requireAssignmentInDepartment(hod, assignmentId);
    return submissions_.findCurrentByAssignment(assignmentId);
}

Submission ModerationService::adjustMark(const User& hod, int submissionId, int newMark,
                                         const std::string& reason) {
    requirePermission(hod, Permission::ModerateMarks, "adjust marks");
    const std::string why = requireText(reason, "The reason", 500);

    std::optional<Submission> result;
    unitOfWork_.runInTransaction([&] {
        Submission submission = loadForHod(hod, submissionId);
        requireCurrentVersion(submission);
        const Assignment assignment = requireAssignmentInDepartment(hod, submission.getAssignmentId());
        requireRange(newMark, "The mark", 0, assignment.getMaxMark());

        const std::optional<int> oldMark = submission.getFinalMark();
        if (oldMark && *oldMark == newMark) throw ValidationError("The new mark is the same as the current mark.");

        submission.adjustFinalMark(newMark);  // only MARKED or MODERATED work
        submissions_.save(submission);
        audit_.record(submission.getId(), hod.getId(), oldMark, newMark, why);
        result = submission;
    });
    return *result;
}

Submission ModerationService::approve(const User& hod, int submissionId) {
    requirePermission(hod, Permission::ModerateMarks, "approve marks");
    std::optional<Submission> result;
    unitOfWork_.runInTransaction([&] {
        Submission submission = loadForHod(hod, submissionId);
        requireCurrentVersion(submission);
        if (submission.getStatus() != SubmissionStatus::Marked) {
            throw BusinessRuleError("Only marked work can be approved (this work is " +
                                    toString(submission.getStatus()) + ").");
        }
        submission.transitionTo(SubmissionStatus::Moderated);
        submissions_.save(submission);
        const int finalMark = *submission.getFinalMark();
        audit_.record(submission.getId(), hod.getId(), finalMark, finalMark, "Approved at moderation");
        result = submission;
    });
    return *result;
}

int ModerationService::approveAll(const User& hod, int assignmentId) {
    requirePermission(hod, Permission::ModerateMarks, "approve marks");
    int approved = 0;
    unitOfWork_.runInTransaction([&] {
        requireAssignmentInDepartment(hod, assignmentId);
        for (Submission submission : submissions_.findCurrentByAssignment(assignmentId)) {
            if (submission.getStatus() != SubmissionStatus::Marked) continue;
            submission.transitionTo(SubmissionStatus::Moderated);
            submissions_.save(submission);
            const int finalMark = *submission.getFinalMark();
            audit_.record(submission.getId(), hod.getId(), finalMark, finalMark, "Approved at moderation");
            ++approved;
        }
    });
    return approved;
}

Submission ModerationService::returnToLecturer(const User& hod, int submissionId, const std::string& comment) {
    requirePermission(hod, Permission::ModerateMarks, "return work to the lecturer");
    const std::string why = "Returned to lecturer: " + requireText(comment, "The comment", 450);

    std::optional<Submission> result;
    unitOfWork_.runInTransaction([&] {
        Submission submission = loadForHod(hod, submissionId);
        requireCurrentVersion(submission);
        if (submission.getStatus() != SubmissionStatus::Marked) {
            throw BusinessRuleError("Only marked work can be returned to the lecturer (this work is " +
                                    toString(submission.getStatus()) + ").");
        }
        const int finalMark = *submission.getFinalMark();
        submission.returnForRemarking();
        submissions_.save(submission);
        audit_.record(submission.getId(), hod.getId(), finalMark, finalMark, why);
        result = submission;
    });
    return *result;
}

ReleaseResult ModerationService::release(const User& hod, int assignmentId) {
    requirePermission(hod, Permission::ReleaseMarks, "release marks");
    ReleaseResult outcome;
    unitOfWork_.runInTransaction([&] {
        requireAssignmentInDepartment(hod, assignmentId);
        for (Submission submission : submissions_.findCurrentByAssignment(assignmentId)) {
            if (submission.getStatus() == SubmissionStatus::Moderated) {
                submission.transitionTo(SubmissionStatus::Released);
                submissions_.save(submission);
                const int finalMark = *submission.getFinalMark();
                audit_.record(submission.getId(), hod.getId(), finalMark, finalMark, "Released to student");
                ++outcome.released;
            } else if (submission.getStatus() != SubmissionStatus::Released) {
                ++outcome.pending;
            }
        }
    });
    return outcome;
}

std::vector<AuditEntry> ModerationService::auditHistory(const User& hod, int submissionId) const {
    requirePermission(hod, Permission::ViewAuditHistory, "view the audit history");
    loadForHod(hod, submissionId);
    return audit_.history(submissionId);
}

}  // namespace lms
