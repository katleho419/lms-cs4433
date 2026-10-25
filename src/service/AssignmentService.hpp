// Lecturer use cases: create assignments and grant individual extensions.
#ifndef LMS_SERVICE_ASSIGNMENTSERVICE_HPP
#define LMS_SERVICE_ASSIGNMENTSERVICE_HPP

#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "service/CourseService.hpp"
#include "util/Clock.hpp"

namespace lms {

class AssignmentService {
public:
    static constexpr int kMaxMarkLimit = 1000;

    AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses, CourseService& access,
                      const IClock& clock)
        : assignments_(assignments), courses_(courses), access_(access), clock_(clock) {}

    // The lecturer must teach the course; the deadline must be in the future.
    Assignment createAssignment(const User& actor, int courseId, const std::string& title,
                                const DateTime& deadline, int maxMark);

    // Gives one student a later deadline. The student must be enrolled and
    // the new deadline must be later than the assignment's own deadline.
    // Granting again replaces the earlier extension.
    Extension grantExtension(const User& actor, int assignmentId, int studentId, const DateTime& newDeadline);

    // Visible to anyone allowed to see the course.
    std::vector<Assignment> assignmentsForCourse(const User& actor, int courseId) const;
    Assignment getAssignment(const User& actor, int assignmentId) const;

private:
    IAssignmentRepository& assignments_;
    ICourseRepository& courses_;
    CourseService& access_;
    const IClock& clock_;
};

}  // namespace lms

#endif
