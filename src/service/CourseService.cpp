#include "service/CourseService.hpp"

#include "util/Errors.hpp"

namespace lms {

Course CourseService::requireCourse(int courseId) const {
    const std::optional<Course> course = courses_.findById(courseId);
    if (!course) throw NotFoundError("There is no course with id " + std::to_string(courseId) + ".");
    return *course;
}

void CourseService::requireLecturerOf(const User& lecturer, int courseId) const {
    if (lecturer.getRole() != Role::Lecturer || !courses_.isLecturerOf(courseId, lecturer.getId())) {
        throw AuthorizationError("You do not teach this course.");
    }
}

void CourseService::requireHodOf(const User& hod, const Course& course) const {
    if (hod.getRole() != Role::Hod || hod.getDepartmentId() != course.getDepartmentId()) {
        throw AuthorizationError("This course is not in your department.");
    }
}

void CourseService::requireCanSee(const User& actor, const Course& course) const {
    switch (actor.getRole()) {
        case Role::Admin:
            return;
        case Role::Lecturer:
            requireLecturerOf(actor, course.getId());
            return;
        case Role::Hod:
            requireHodOf(actor, course);
            return;
        case Role::Student:
            if (!courses_.isEnrolled(course.getId(), actor.getId())) {
                throw AuthorizationError("You are not enrolled in this course.");
            }
            return;
    }
}

std::vector<Course> CourseService::coursesFor(const User& actor) const {
    switch (actor.getRole()) {
        case Role::Admin: return courses_.findAll();
        case Role::Lecturer: return courses_.findByLecturer(actor.getId());
        case Role::Hod: return courses_.findByDepartment(actor.getDepartmentId());
        case Role::Student: return courses_.findByStudent(actor.getId());
    }
    return {};
}

std::vector<UserPtr> CourseService::studentsOf(const User& actor, int courseId) const {
    const Course course = requireCourse(courseId);
    if (actor.getRole() != Role::Admin) {
        if (actor.getRole() == Role::Lecturer) {
            requireLecturerOf(actor, courseId);
        } else if (actor.getRole() == Role::Hod) {
            requireHodOf(actor, course);
        } else {
            throw AuthorizationError("You are not allowed to see the students of a course.");
        }
    }
    std::vector<UserPtr> students;
    for (int id : courses_.studentIdsOf(courseId)) {
        const std::optional<UserPtr> student = users_.findById(id);
        if (student) students.push_back(*student);
    }
    return students;
}

}  // namespace lms
