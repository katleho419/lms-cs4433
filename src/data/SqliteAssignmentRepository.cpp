#include "data/SqliteRepositories.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

const char* const kSelectAssignment = "SELECT id, course_id, title, deadline, max_mark FROM assignment ";

Assignment toAssignment(const Statement& row) {
    return Assignment(row.columnInt(0), row.columnInt(1), row.columnText(2),
                      DateTime::parseIso(row.columnText(3)), row.columnInt(4));
}

Extension toExtension(const Statement& row) {
    return Extension(row.columnInt(0), row.columnInt(1), row.columnInt(2),
                     DateTime::parseIso(row.columnText(3)), row.columnInt(4));
}

}  // namespace

std::optional<Assignment> SqliteAssignmentRepository::findById(int id) const {
    Statement query(db_, std::string(kSelectAssignment) + "WHERE id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toAssignment(query);
}

std::vector<Assignment> SqliteAssignmentRepository::findByCourse(int courseId) const {
    Statement query(db_, std::string(kSelectAssignment) + "WHERE course_id = ? ORDER BY deadline, id");
    query.bind(1, courseId);
    std::vector<Assignment> assignments;
    while (query.step()) assignments.push_back(toAssignment(query));
    return assignments;
}

void SqliteAssignmentRepository::save(Assignment& assignment) {
    if (assignment.getId() == 0) {
        Statement insert(db_,
                         "INSERT INTO assignment (course_id, title, deadline, max_mark) VALUES (?, ?, ?, ?)");
        insert.bind(1, assignment.getCourseId())
            .bind(2, assignment.getTitle())
            .bind(3, assignment.getDeadline().toIso())
            .bind(4, assignment.getMaxMark());
        insert.run();
        assignment.assignId(static_cast<int>(db_.lastInsertId()));
    } else {
        Statement update(db_,
                         "UPDATE assignment SET title = ?, deadline = ?, max_mark = ? WHERE id = ?");
        update.bind(1, assignment.getTitle())
            .bind(2, assignment.getDeadline().toIso())
            .bind(3, assignment.getMaxMark())
            .bind(4, assignment.getId());
        update.run();
    }
}

std::optional<Extension> SqliteAssignmentRepository::findExtension(int assignmentId, int studentId) const {
    Statement query(db_,
                    "SELECT id, assignment_id, student_id, new_deadline, granted_by FROM extension "
                    "WHERE assignment_id = ? AND student_id = ?");
    query.bind(1, assignmentId).bind(2, studentId);
    if (!query.step()) return std::nullopt;
    return toExtension(query);
}

void SqliteAssignmentRepository::saveExtension(Extension& extension) {
    Statement upsert(db_,
                     "INSERT INTO extension (assignment_id, student_id, new_deadline, granted_by) "
                     "VALUES (?, ?, ?, ?) "
                     "ON CONFLICT (assignment_id, student_id) DO UPDATE SET "
                     "new_deadline = excluded.new_deadline, granted_by = excluded.granted_by");
    upsert.bind(1, extension.getAssignmentId())
        .bind(2, extension.getStudentId())
        .bind(3, extension.getNewDeadline().toIso())
        .bind(4, extension.getGrantedBy());
    upsert.run();
    const std::optional<Extension> stored = findExtension(extension.getAssignmentId(), extension.getStudentId());
    if (stored) extension.assignId(stored->getId());
}

}  // namespace lms
