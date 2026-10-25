#include "service/AdminService.hpp"

#include "service/AccessControl.hpp"
#include "util/Errors.hpp"
#include "util/Validation.hpp"

namespace lms {

UserPtr AdminService::createUser(const User& actor, Role role, const UserData& data) {
    requirePermission(actor, Permission::ManageAccounts, "manage user accounts");

    UserPtr user(UserFactory::createUser(role, data));  // validates and hashes the password
    if (users_.findByUsername(user->getUsername())) {
        throw ValidationError("That username is already taken.");
    }
    if (role == Role::Lecturer || role == Role::Hod) {
        if (!departments_.findById(data.departmentId)) throw NotFoundError("That department does not exist.");
    }
    if (role == Role::Hod && users_.findHeadOfDepartment(data.departmentId)) {
        throw BusinessRuleError("That department already has a Head of Department.");
    }
    if (const auto* student = dynamic_cast<const Student*>(user.get())) {
        if (users_.findByStudentNumber(student->getStudentNumber())) {
            throw ValidationError("That student number is already in use.");
        }
    }
    users_.save(user);
    return user;
}

std::vector<UserPtr> AdminService::listUsers(const User& actor) const {
    requirePermission(actor, Permission::ManageAccounts, "view user accounts");
    return users_.findAll();
}

Department AdminService::createDepartment(const User& actor, const std::string& name) {
    requirePermission(actor, Permission::ManageDepartments, "manage departments");
    Department department(0, requireText(name, "The department name", 100));
    if (departments_.findByName(department.getName())) {
        throw ValidationError("A department with that name already exists.");
    }
    departments_.save(department);
    return department;
}

Course AdminService::createCourse(const User& actor, const std::string& code, const std::string& title,
                                  int departmentId) {
    requirePermission(actor, Permission::ManageCourses, "manage courses");
    Course course(0, requireText(code, "The course code", 20), requireText(title, "The course title", 100),
                  departmentId);
    if (!departments_.findById(departmentId)) throw NotFoundError("That department does not exist.");
    if (courses_.findByCode(course.getCode())) throw ValidationError("A course with that code already exists.");
    courses_.save(course);
    return course;
}

UserPtr AdminService::requireUserWithRole(int userId, Role role) const {
    const std::optional<UserPtr> user = users_.findById(userId);
    if (!user || (*user)->getRole() != role) {
        throw NotFoundError("There is no " + displayName(role) + " with id " + std::to_string(userId) + ".");
    }
    return *user;
}

void AdminService::assignLecturer(const User& actor, int courseId, int lecturerId) {
    requirePermission(actor, Permission::AssignLecturers, "assign lecturers to courses");
    const std::optional<Course> course = courses_.findById(courseId);
    if (!course) throw NotFoundError("There is no course with id " + std::to_string(courseId) + ".");
    const UserPtr lecturer = requireUserWithRole(lecturerId, Role::Lecturer);
    if (lecturer->getDepartmentId() != course->getDepartmentId()) {
        throw BusinessRuleError("The lecturer works in a different department from the course.");
    }
    if (courses_.isLecturerOf(courseId, lecturerId)) {
        throw BusinessRuleError("That lecturer already teaches this course.");
    }
    courses_.addLecturer(courseId, lecturerId);
}

void AdminService::enrolStudent(const User& actor, int courseId, int studentId) {
    requirePermission(actor, Permission::EnrolStudents, "enrol students");
    if (!courses_.findById(courseId)) throw NotFoundError("There is no course with id " + std::to_string(courseId) + ".");
    requireUserWithRole(studentId, Role::Student);
    if (courses_.isEnrolled(courseId, studentId)) {
        throw BusinessRuleError("That student is already enrolled in this course.");
    }
    courses_.enrol(courseId, studentId);
}

}  // namespace lms
