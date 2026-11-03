// Lecturer use cases: create assignments and grant extensions (one student or the whole class).
#ifndef LMS_SERVICE_ASSIGNMENTSERVICE_HPP
#define LMS_SERVICE_ASSIGNMENTSERVICE_HPP

#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "service/CourseService.hpp"
#include "service/IUnitOfWork.hpp"
#include "util/Clock.hpp"

namespace lms {

// Outcome of a class-wide extension.
struct ClassExtensionResult {
    int extended = 0;   // students whose deadline is now the new one
    int unchanged = 0;  // students who already had the same or a later extension
};

class AssignmentService {
public:
    static constexpr int kMaxMarkLimit = 1000;

    AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses, CourseService& access,
                      const IClock& clock, IUnitOfWork& unitOfWork)
        : assignments_(assignments),
          courses_(courses),
          access_(access),
          clock_(clock),
          unitOfWork_(unitOfWork) {}

    // The lecturer must teach the course; the deadline must be in the future.
    Assignment createAssignment(const User& actor, int courseId, const std::string& title,
                                const DateTime& deadline, int maxMark);

    // Gives one student a later deadline. The student must be enrolled and
    // the new deadline must be later than the assignment's own deadline.
    // Granting again replaces the earlier extension.
    Extension grantExtension(const User& actor, int assignmentId, int studentId, const DateTime& newDeadline);

    // Change request CR1: gives every student enrolled in the course the same later deadline, in one
    // all-or-nothing step. The lecturer must teach the course and the new deadline must be later than the
    // assignment's own deadline. A student who already has the same or a later extension keeps it
    // (an extension is never shortened). Fails if nobody is enrolled.
    ClassExtensionResult grantClassExtension(const User& actor, int assignmentId,
                                             const DateTime& newDeadline);

    // Visible to anyone allowed to see the course.
    std::vector<Assignment> assignmentsForCourse(const User& actor, int courseId) const;
    Assignment getAssignment(const User& actor, int assignmentId) const;

private:
    IAssignmentRepository& assignments_;
    ICourseRepository& courses_;
    CourseService& access_;
    const IClock& clock_;
    IUnitOfWork& unitOfWork_;
};

}  // namespace lms

#endif
