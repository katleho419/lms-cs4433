#include "data/SqliteRepositories.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

const char* const kSelectAudit =
    "SELECT id, submission_id, changed_by, changed_at, old_value, new_value, reason FROM audit_entry ";

AuditEntry toEntry(const Statement& row) {
    return AuditEntry(row.columnInt(0), row.columnInt(1), row.columnInt(2),
                      DateTime::parseIso(row.columnText(3)), row.columnOptionalInt(4), row.columnInt(5),
                      row.columnText(6));
}

}  // namespace

std::optional<AuditEntry> SqliteAuditRepository::findById(int id) const {
    Statement query(db_, std::string(kSelectAudit) + "WHERE id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toEntry(query);
}

std::vector<AuditEntry> SqliteAuditRepository::findBySubmission(int submissionId) const {
    Statement query(db_, std::string(kSelectAudit) + "WHERE submission_id = ? ORDER BY changed_at, id");
    query.bind(1, submissionId);
    std::vector<AuditEntry> entries;
    while (query.step()) entries.push_back(toEntry(query));
    return entries;
}

void SqliteAuditRepository::save(AuditEntry& entry) {
    if (entry.getId() != 0) throw BusinessRuleError("Audit entries cannot be changed once written.");
    Statement insert(db_,
                     "INSERT INTO audit_entry (submission_id, changed_by, changed_at, old_value, "
                     "new_value, reason) VALUES (?, ?, ?, ?, ?, ?)");
    insert.bind(1, entry.getSubmissionId())
        .bind(2, entry.getChangedBy())
        .bind(3, entry.getChangedAt().toIso())
        .bind(4, entry.getOldValue())
        .bind(5, entry.getNewValue())
        .bind(6, entry.getReason());
    insert.run();
    entry.assignId(static_cast<int>(db_.lastInsertId()));
}

}  // namespace lms
