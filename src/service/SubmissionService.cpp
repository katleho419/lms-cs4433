#include "service/SubmissionService.hpp"

#include "service/AccessControl.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {

int SubmissionService::computeLateDays(const DateTime& deadline, const DateTime& now) {
    const std::int64_t late = now.secondsSince(deadline);
    if (late <= 0) return 0;
    return static_cast<int>((late + kSecondsPerDay - 1) / kSecondsPerDay);
}

DateTime SubmissionService::effectiveDeadlineFor(const Assignment& assignment, int studentId) const {
    const std::optional<Extension> extension = assignments_.findExtension(assignment.getId(), studentId);
    if (extension && extension->getNewDeadline() > assignment.getDeadline()) {
        return extension->getNewDeadline();
    }
    return assignment.getDeadline();
}

Submission SubmissionService::submit(const User& student, int assignmentId, const std::string& content) {
    requirePermission(student, Permission::SubmitWork, "submit work");
    if (trim(content).empty()) throw ValidationError("The submission must not be empty.");
    if (content.size() > kMaxContentBytes) {
        throw ValidationError("The submission is too large (the limit is " +
                              std::to_string(kMaxContentBytes) + " characters).");
    }

    std::optional<Submission> saved;
    unitOfWork_.runInTransaction([&] {
        const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
        if (!assignment) throw NotFoundError("There is no assignment with id " + std::to_string(assignmentId) + ".");
        if (!courses_.isEnrolled(assignment->getCourseId(), student.getId())) {
            throw AuthorizationError("You are not enrolled in the course for this assignment.");
        }

        const DateTime now = clock_.now();
        const DateTime deadline = effectiveDeadlineFor(*assignment, student.getId());
        int version = 1;
        const std::optional<Submission> previous = submissions_.findLatest(assignmentId, student.getId());
        if (previous) {
            if (!previous->canResubmit(now, deadline)) {
                if (previous->getStatus() != SubmissionStatus::Submitted) {
                    throw BusinessRuleError(
                        "Your work was submitted late or has already been marked, so it can no longer be replaced.");
                }
                throw BusinessRuleError("The deadline has passed, so you can no longer resubmit.");
            }
            version = previous->getVersion() + 1;
        }

        const int lateDays = computeLateDays(deadline, now);
        Submission submission(0, assignmentId, student.getId(), version, content, now,
                              lateDays == 0 ? SubmissionStatus::Submitted : SubmissionStatus::Late, lateDays,
                              std::nullopt, std::nullopt, "");
        submissions_.save(submission);
        saved = submission;
    });
    return *saved;
}

std::vector<AssignmentRow> SubmissionService::assignmentsFor(const User& student) const {
    requirePermission(student, Permission::SubmitWork, "view your assignments");
    std::vector<AssignmentRow> rows;
    for (const Course& course : courses_.findByStudent(student.getId())) {
        for (const Assignment& assignment : assignments_.findByCourse(course.getId())) {
            rows.push_back({assignment, course.getCode(),
                            submissions_.findLatest(assignment.getId(), student.getId())});
        }
    }
    return rows;
}

}  // namespace lms
