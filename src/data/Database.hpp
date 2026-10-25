// Thin RAII wrapper around SQLite. This is the only place that talks to the
// SQLite C API. All data access uses prepared statements with bound
// parameters, so user text can never become part of an SQL command.
#ifndef LMS_DATA_DATABASE_HPP
#define LMS_DATA_DATABASE_HPP

#include <sqlite3.h>

#include <functional>
#include <optional>
#include <string>

#include "service/IUnitOfWork.hpp"

namespace lms {

class Database : public IUnitOfWork {
public:
    // Opens (or creates) the file. ":memory:" gives a private temporary
    // database, used by tests. Creates the schema when the file is empty.
    // Throws DatabaseError.
    explicit Database(const std::string& path);
    ~Database() override;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return db_; }

    // Runs fixed SQL that contains no user data (schema, pragmas, BEGIN/COMMIT).
    void execute(const std::string& sql);
    long long lastInsertId() const;

    // IUnitOfWork: runs `work` in one transaction. Nested calls join the
    // outer transaction. Any exception rolls everything back.
    void runInTransaction(const std::function<void()>& work) override;

    // Used by TransactionScope.
    void enterTransaction();
    void leaveTransaction();
    void abortTransaction() noexcept;

private:
    sqlite3* db_ = nullptr;
    int depth_ = 0;
    bool failed_ = false;
};

// A prepared statement. Parameters are 1-based, columns are 0-based.
class Statement {
public:
    Statement(const Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, int value);
    Statement& bind(int index, long long value);
    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, const std::optional<int>& value);  // NULL when empty
    Statement& bindNull(int index);

    // Returns true when a row is available, false when finished.
    bool step();
    // Runs a statement that returns no rows (INSERT, UPDATE).
    void run();

    int columnInt(int column) const;
    std::string columnText(int column) const;
    bool columnIsNull(int column) const;
    std::optional<int> columnOptionalInt(int column) const;

private:
    void check(int resultCode) const;
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

// Begins a transaction (or joins the current one). Call commit() on success;
// if the scope ends without commit(), everything is rolled back.
class TransactionScope {
public:
    explicit TransactionScope(Database& db) : db_(db) { db_.enterTransaction(); }
    ~TransactionScope() {
        if (!finished_) db_.abortTransaction();
    }
    TransactionScope(const TransactionScope&) = delete;
    TransactionScope& operator=(const TransactionScope&) = delete;
    void commit() {
        finished_ = true;
        db_.leaveTransaction();
    }

private:
    Database& db_;
    bool finished_ = false;
};

}  // namespace lms

#endif
