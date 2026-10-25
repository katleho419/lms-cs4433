#include "service/MarkingService.hpp"

#include "service/AccessControl.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {

std::vector<Submission> MarkingService::submissionsFor(const User& lecturer, int assignmentId) const {
    requirePermission(lecturer, Permission::MarkSubmission, "mark submissions");
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) throw NotFoundError("There is no assignment with id " + std::to_string(assignmentId) + ".");
    access_.requireLecturerOf(lecturer, assignment->getCourseId());
    return submissions_.findCurrentByAssignment(assignmentId);
}

Submission MarkingService::mark(const User& lecturer, int submissionId, int rawMark,
                                const std::string& feedback, const std::string& reason) {
    requirePermission(lecturer, Permission::MarkSubmission, "mark submissions");
    const std::string cleanFeedback = requireText(feedback, "The feedback", 5000);

    std::optional<Submission> result;
    unitOfWork_.runInTransaction([&] {
        std::optional<Submission> found = submissions_.findById(submissionId);
        if (!found) throw NotFoundError("There is no submission with id " + std::to_string(submissionId) + ".");
        Submission submission = *found;

        const std::optional<Assignment> assignment = assignments_.findById(submission.getAssignmentId());
        if (!assignment) throw NotFoundError("The assignment for this submission no longer exists.");
        access_.requireLecturerOf(lecturer, assignment->getCourseId());

        const std::optional<Submission> latest =
            submissions_.findLatest(submission.getAssignmentId(), submission.getStudentId());
        if (latest && latest->getId() != submission.getId()) {
            throw BusinessRuleError("This is an older version. Mark the student's latest version instead.");
        }

        const SubmissionStatus status = submission.getStatus();
        if (status == SubmissionStatus::Moderated || status == SubmissionStatus::Released) {
            throw BusinessRuleError(
                "This work has already been moderated, so only the Head of Department can change its mark.");
        }
        requireRange(rawMark, "The mark", 0, assignment->getMaxMark());

        std::string why = trim(reason);
        if (status == SubmissionStatus::Marked && why.empty()) {
            throw ValidationError("A reason is required when changing a mark.");
        }
        const std::optional<int> oldFinal = submission.getFinalMark();
        if (why.empty()) why = oldFinal ? "Re-marked after return to lecturer" : "Initial marking";

        const int finalMark = penalty_.calculate(rawMark, assignment->getMaxMark(), submission.getLateDays());
        submission.applyMark(rawMark, finalMark, cleanFeedback);
        submissions_.save(submission);
        audit_.record(submission.getId(), lecturer.getId(), oldFinal, finalMark,
                      why + " (raw mark " + std::to_string(rawMark) + ", " +
                          std::to_string(submission.getLateDays()) + " late day(s))");
        result = submission;
    });
    return *result;
}

}  // namespace lms
