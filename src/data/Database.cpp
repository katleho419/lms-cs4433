#include "data/Database.hpp"

#include <utility>

#include "SchemaSql.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

constexpr int kSchemaVersion = 1;
constexpr int kBusyTimeoutMs = 5000;

std::string messageFor(sqlite3* db, const std::string& prefix) {
    return prefix + ": " + (db != nullptr ? sqlite3_errmsg(db) : "no connection");
}

}  // namespace

Database::Database(const std::string& path) {
    if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        const std::string message = messageFor(db_, "Cannot open the database file '" + path + "'");
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError(message);
    }
    try {
        sqlite3_busy_timeout(db_, kBusyTimeoutMs);
        execute("PRAGMA foreign_keys = ON;");

        bool hasSchema = false;
        {
            Statement probe(*this, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'app_user'");
            hasSchema = probe.step();
        }
        if (!hasSchema) {
            execute(data::kSchemaSql);
            execute("PRAGMA user_version = " + std::to_string(kSchemaVersion) + ";");
        } else {
            Statement version(*this, "PRAGMA user_version");
            if (!version.step() || version.columnInt(0) != kSchemaVersion) {
                throw DatabaseError("The database file was created by a different version of this program.");
            }
        }
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::~Database() {
    if (db_ != nullptr) sqlite3_close(db_);
}

void Database::execute(const std::string& sql) {
    char* errorText = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorText) != SQLITE_OK) {
        const std::string message = errorText != nullptr ? errorText : "unknown error";
        sqlite3_free(errorText);
        throw DatabaseError("Database error: " + message);
    }
}

long long Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

void Database::runInTransaction(const std::function<void()>& work) {
    TransactionScope scope(*this);
    work();
    scope.commit();
}

void Database::enterTransaction() {
    if (depth_ == 0) {
        execute("BEGIN IMMEDIATE;");
        failed_ = false;
    }
    ++depth_;
}

void Database::leaveTransaction() {
    --depth_;
    if (depth_ == 0) {
        if (failed_) {
            failed_ = false;
            execute("ROLLBACK;");
            throw DatabaseError("The changes were cancelled because part of the operation failed.");
        }
        execute("COMMIT;");
    }
}

void Database::abortTransaction() noexcept {
    failed_ = true;
    if (depth_ > 0) --depth_;
    if (depth_ == 0) {
        failed_ = false;
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

Statement::Statement(const Database& db, const std::string& sql) : db_(db.handle()) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw DatabaseError(messageFor(db_, "Database error"));
    }
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

void Statement::check(int resultCode) const {
    if (resultCode != SQLITE_OK && resultCode != SQLITE_ROW && resultCode != SQLITE_DONE) {
        throw DatabaseError(messageFor(db_, "Database error"));
    }
}

Statement& Statement::bind(int index, int value) {
    check(sqlite3_bind_int(stmt_, index, value));
    return *this;
}

Statement& Statement::bind(int index, long long value) {
    check(sqlite3_bind_int64(stmt_, index, value));
    return *this;
}

Statement& Statement::bind(int index, const std::string& value) {
    check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    return *this;
}

Statement& Statement::bind(int index, const std::optional<int>& value) {
    return value ? bind(index, *value) : bindNull(index);
}

Statement& Statement::bindNull(int index) {
    check(sqlite3_bind_null(stmt_, index));
    return *this;
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DatabaseError(messageFor(db_, "Database error"));
}

void Statement::run() {
    while (step()) {
    }
}

int Statement::columnInt(int column) const { return sqlite3_column_int(stmt_, column); }

std::string Statement::columnText(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    return text != nullptr ? reinterpret_cast<const char*>(text) : std::string();
}

bool Statement::columnIsNull(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }

std::optional<int> Statement::columnOptionalInt(int column) const {
    if (columnIsNull(column)) return std::nullopt;
    return columnInt(column);
}

}  // namespace lms
