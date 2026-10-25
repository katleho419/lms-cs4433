// Course queries and the "is this person allowed to touch this course" checks
// shared by the other services.
#ifndef LMS_SERVICE_COURSESERVICE_HPP
#define LMS_SERVICE_COURSESERVICE_HPP

#include <vector>

#include "data/RepositoryInterfaces.hpp"

namespace lms {

class CourseService {
public:
    CourseService(IUserRepository& users, IDepartmentRepository& departments, ICourseRepository& courses)
        : users_(users), departments_(departments), courses_(courses) {}

    // Throws NotFoundError.
    Course requireCourse(int courseId) const;

    // Throws AuthorizationError.
    void requireLecturerOf(const User& lecturer, int courseId) const;
    void requireHodOf(const User& hod, const Course& course) const;
    // Admins see every course, lecturers the courses they teach, heads of
    // department their department's courses, students their enrolled courses.
    void requireCanSee(const User& actor, const Course& course) const;

    std::vector<Course> coursesFor(const User& actor) const;
    // Enrolled students. Allowed for the course's lecturers, the department's head, and admins.
    std::vector<UserPtr> studentsOf(const User& actor, int courseId) const;
    std::vector<Department> departments() const { return departments_.findAll(); }

private:
    IUserRepository& users_;
    IDepartmentRepository& departments_;
    ICourseRepository& courses_;
};

}  // namespace lms

#endif
