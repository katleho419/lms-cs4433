// Departments and courses (including who teaches and who is enrolled).
#include "data/SqliteRepositories.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

const char* const kSelectCourse = "SELECT c.id, c.code, c.title, c.department_id FROM course c ";

Course toCourse(const Statement& row) {
    return Course(row.columnInt(0), row.columnText(1), row.columnText(2), row.columnInt(3));
}

Department toDepartment(const Statement& row) { return Department(row.columnInt(0), row.columnText(1)); }

std::vector<Course> readAll(Statement& query) {
    std::vector<Course> courses;
    while (query.step()) courses.push_back(toCourse(query));
    return courses;
}

}  // namespace

// ---------------------------------------------------------------- departments

std::optional<Department> SqliteDepartmentRepository::findById(int id) const {
    Statement query(db_, "SELECT id, name FROM department WHERE id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toDepartment(query);
}

std::optional<Department> SqliteDepartmentRepository::findByName(const std::string& name) const {
    Statement query(db_, "SELECT id, name FROM department WHERE name = ?");
    query.bind(1, name);
    if (!query.step()) return std::nullopt;
    return toDepartment(query);
}

std::vector<Department> SqliteDepartmentRepository::findAll() const {
    Statement query(db_, "SELECT id, name FROM department ORDER BY name");
    std::vector<Department> departments;
    while (query.step()) departments.push_back(toDepartment(query));
    return departments;
}

void SqliteDepartmentRepository::save(Department& department) {
    if (department.getId() == 0) {
        Statement insert(db_, "INSERT INTO department (name) VALUES (?)");
        insert.bind(1, department.getName());
        insert.run();
        department.assignId(static_cast<int>(db_.lastInsertId()));
    } else {
        Statement update(db_, "UPDATE department SET name = ? WHERE id = ?");
        update.bind(1, department.getName()).bind(2, department.getId());
        update.run();
    }
}

// -------------------------------------------------------------------- courses

std::optional<Course> SqliteCourseRepository::findById(int id) const {
    Statement query(db_, std::string(kSelectCourse) + "WHERE c.id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toCourse(query);
}

std::optional<Course> SqliteCourseRepository::findByCode(const std::string& code) const {
    Statement query(db_, std::string(kSelectCourse) + "WHERE c.code = ?");
    query.bind(1, code);
    if (!query.step()) return std::nullopt;
    return toCourse(query);
}

std::vector<Course> SqliteCourseRepository::findAll() const {
    Statement query(db_, std::string(kSelectCourse) + "ORDER BY c.code");
    return readAll(query);
}

std::vector<Course> SqliteCourseRepository::findByDepartment(int departmentId) const {
    Statement query(db_, std::string(kSelectCourse) + "WHERE c.department_id = ? ORDER BY c.code");
    query.bind(1, departmentId);
    return readAll(query);
}

std::vector<Course> SqliteCourseRepository::findByLecturer(int lecturerId) const {
    Statement query(db_, std::string(kSelectCourse) +
                             "JOIN course_lecturer cl ON cl.course_id = c.id "
                             "WHERE cl.lecturer_id = ? ORDER BY c.code");
    query.bind(1, lecturerId);
    return readAll(query);
}

std::vector<Course> SqliteCourseRepository::findByStudent(int studentId) const {
    Statement query(db_, std::string(kSelectCourse) +
                             "JOIN enrolment e ON e.course_id = c.id "
                             "WHERE e.student_id = ? ORDER BY c.code");
    query.bind(1, studentId);
    return readAll(query);
}

void SqliteCourseRepository::save(Course& course) {
    if (course.getId() == 0) {
        Statement insert(db_, "INSERT INTO course (code, title, department_id) VALUES (?, ?, ?)");
        insert.bind(1, course.getCode()).bind(2, course.getTitle()).bind(3, course.getDepartmentId());
        insert.run();
        course.assignId(static_cast<int>(db_.lastInsertId()));
    } else {
        Statement update(db_, "UPDATE course SET code = ?, title = ?, department_id = ? WHERE id = ?");
        update.bind(1, course.getCode())
            .bind(2, course.getTitle())
            .bind(3, course.getDepartmentId())
            .bind(4, course.getId());
        update.run();
    }
}

void SqliteCourseRepository::addLecturer(int courseId, int lecturerId) {
    Statement insert(db_, "INSERT INTO course_lecturer (course_id, lecturer_id) VALUES (?, ?)");
    insert.bind(1, courseId).bind(2, lecturerId);
    insert.run();
}

bool SqliteCourseRepository::isLecturerOf(int courseId, int lecturerId) const {
    Statement query(db_, "SELECT 1 FROM course_lecturer WHERE course_id = ? AND lecturer_id = ?");
    query.bind(1, courseId).bind(2, lecturerId);
    return query.step();
}

void SqliteCourseRepository::enrol(int courseId, int studentId) {
    Statement insert(db_, "INSERT INTO enrolment (student_id, course_id) VALUES (?, ?)");
    insert.bind(1, studentId).bind(2, courseId);
    insert.run();
}

bool SqliteCourseRepository::isEnrolled(int courseId, int studentId) const {
    Statement query(db_, "SELECT 1 FROM enrolment WHERE course_id = ? AND student_id = ?");
    query.bind(1, courseId).bind(2, studentId);
    return query.step();
}

std::vector<int> SqliteCourseRepository::studentIdsOf(int courseId) const {
    Statement query(db_, "SELECT student_id FROM enrolment WHERE course_id = ? ORDER BY student_id");
    query.bind(1, courseId);
    std::vector<int> ids;
    while (query.step()) ids.push_back(query.columnInt(0));
    return ids;
}

}  // namespace lms
