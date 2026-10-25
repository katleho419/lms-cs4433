#include "service/AssignmentService.hpp"

#include "service/AccessControl.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {

Assignment AssignmentService::createAssignment(const User& actor, int courseId, const std::string& title,
                                               const DateTime& deadline, int maxMark) {
    requirePermission(actor, Permission::CreateAssignment, "create assignments");
    access_.requireCourse(courseId);
    access_.requireLecturerOf(actor, courseId);

    Assignment assignment(0, courseId, requireText(title, "The title", 100), deadline,
                          requireRange(maxMark, "The maximum mark", 1, kMaxMarkLimit));
    if (deadline <= clock_.now()) throw ValidationError("The deadline must be in the future.");
    assignments_.save(assignment);
    return assignment;
}

Extension AssignmentService::grantExtension(const User& actor, int assignmentId, int studentId,
                                            const DateTime& newDeadline) {
    requirePermission(actor, Permission::GrantExtension, "grant extensions");
    const Assignment assignment = getAssignment(actor, assignmentId);
    if (!courses_.isEnrolled(assignment.getCourseId(), studentId)) {
        throw BusinessRuleError("That student is not enrolled in this course.");
    }
    if (newDeadline <= assignment.getDeadline()) {
        throw ValidationError("An extension must be later than the assignment deadline.");
    }
    Extension extension(0, assignmentId, studentId, newDeadline, actor.getId());
    assignments_.saveExtension(extension);
    return extension;
}

std::vector<Assignment> AssignmentService::assignmentsForCourse(const User& actor, int courseId) const {
    access_.requireCanSee(actor, access_.requireCourse(courseId));
    return assignments_.findByCourse(courseId);
}

Assignment AssignmentService::getAssignment(const User& actor, int assignmentId) const {
    const std::optional<Assignment> assignment = assignments_.findById(assignmentId);
    if (!assignment) throw NotFoundError("There is no assignment with id " + std::to_string(assignmentId) + ".");
    access_.requireCanSee(actor, access_.requireCourse(assignment->getCourseId()));
    return *assignment;
}

}  // namespace lms
