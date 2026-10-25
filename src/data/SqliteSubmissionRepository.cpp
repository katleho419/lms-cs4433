#include "data/SqliteRepositories.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

const char* const kSelectSubmission =
    "SELECT s.id, s.assignment_id, s.student_id, s.version, s.content, s.submitted_at, s.status, "
    "s.late_days, s.raw_mark, s.final_mark, s.feedback FROM submission s ";

// Keeps only the highest version for each (assignment, student) pair.
const char* const kCurrentOnly =
    "s.version = (SELECT MAX(s2.version) FROM submission s2 "
    "WHERE s2.assignment_id = s.assignment_id AND s2.student_id = s.student_id) ";

Submission toSubmission(const Statement& row) {
    return Submission(row.columnInt(0), row.columnInt(1), row.columnInt(2), row.columnInt(3),
                      row.columnText(4), DateTime::parseIso(row.columnText(5)),
                      submissionStatusFromString(row.columnText(6)), row.columnInt(7),
                      row.columnOptionalInt(8), row.columnOptionalInt(9), row.columnText(10));
}

std::vector<Submission> readAll(Statement& query) {
    std::vector<Submission> submissions;
    while (query.step()) submissions.push_back(toSubmission(query));
    return submissions;
}

}  // namespace

std::optional<Submission> SqliteSubmissionRepository::findById(int id) const {
    Statement query(db_, std::string(kSelectSubmission) + "WHERE s.id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toSubmission(query);
}

std::vector<Submission> SqliteSubmissionRepository::findByAssignment(int assignmentId) const {
    Statement query(db_, std::string(kSelectSubmission) +
                             "WHERE s.assignment_id = ? ORDER BY s.student_id, s.version");
    query.bind(1, assignmentId);
    return readAll(query);
}

std::vector<Submission> SqliteSubmissionRepository::findCurrentByAssignment(int assignmentId) const {
    Statement query(db_, std::string(kSelectSubmission) + "WHERE s.assignment_id = ? AND " + kCurrentOnly +
                             "ORDER BY s.student_id");
    query.bind(1, assignmentId);
    return readAll(query);
}

std::optional<Submission> SqliteSubmissionRepository::findLatest(int assignmentId, int studentId) const {
    Statement query(db_, std::string(kSelectSubmission) +
                             "WHERE s.assignment_id = ? AND s.student_id = ? "
                             "ORDER BY s.version DESC LIMIT 1");
    query.bind(1, assignmentId).bind(2, studentId);
    if (!query.step()) return std::nullopt;
    return toSubmission(query);
}

std::vector<Submission> SqliteSubmissionRepository::findCurrentByStudent(int studentId) const {
    Statement query(db_, std::string(kSelectSubmission) + "WHERE s.student_id = ? AND " + kCurrentOnly +
                             "ORDER BY s.assignment_id");
    query.bind(1, studentId);
    return readAll(query);
}

void SqliteSubmissionRepository::save(Submission& submission) {
    const std::optional<int> raw = submission.getRawMark();
    const std::optional<int> final = submission.getFinalMark();
    if (submission.getId() == 0) {
        Statement insert(db_,
                         "INSERT INTO submission (assignment_id, student_id, version, content, "
                         "submitted_at, status, late_days, raw_mark, final_mark, feedback) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, submission.getAssignmentId())
            .bind(2, submission.getStudentId())
            .bind(3, submission.getVersion())
            .bind(4, submission.getContent())
            .bind(5, submission.getSubmittedAt().toIso())
            .bind(6, toString(submission.getStatus()))
            .bind(7, submission.getLateDays())
            .bind(8, raw)
            .bind(9, final);
        if (submission.getFeedback().empty()) {
            insert.bindNull(10);
        } else {
            insert.bind(10, submission.getFeedback());
        }
        insert.run();
        submission.assignId(static_cast<int>(db_.lastInsertId()));
    } else {
        Statement update(db_,
                         "UPDATE submission SET status = ?, late_days = ?, raw_mark = ?, "
                         "final_mark = ?, feedback = ? WHERE id = ?");
        update.bind(1, toString(submission.getStatus()))
            .bind(2, submission.getLateDays())
            .bind(3, raw)
            .bind(4, final);
        if (submission.getFeedback().empty()) {
            update.bindNull(5);
        } else {
            update.bind(5, submission.getFeedback());
        }
        update.bind(6, submission.getId());
        update.run();
    }
}

}  // namespace lms
