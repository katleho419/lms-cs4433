#include <memory>

#include "data/SqliteRepositories.hpp"
#include "domain/UserFactory.hpp"
#include "util/Errors.hpp"

namespace lms {
namespace {

const char* const kSelectUser =
    "SELECT u.id, u.username, u.password_hash, u.salt, u.full_name, u.role, "
    "s.student_number, st.department_id "
    "FROM app_user u "
    "LEFT JOIN student s ON s.user_id = u.id "
    "LEFT JOIN staff st ON st.user_id = u.id ";

UserPtr toUser(const Statement& row) {
    UserRecord record;
    record.id = row.columnInt(0);
    record.username = row.columnText(1);
    record.passwordHash = row.columnText(2);
    record.salt = row.columnText(3);
    record.fullName = row.columnText(4);
    record.role = roleFromString(row.columnText(5));
    record.studentNumber = row.columnText(6);
    record.departmentId = row.columnIsNull(7) ? 0 : row.columnInt(7);
    return UserPtr(UserFactory::restore(record));
}

}  // namespace

std::optional<UserPtr> SqliteUserRepository::findById(int id) const {
    Statement query(db_, std::string(kSelectUser) + "WHERE u.id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return toUser(query);
}

std::optional<UserPtr> SqliteUserRepository::findOne(const std::string& whereClause,
                                                     const std::string& value) const {
    Statement query(db_, std::string(kSelectUser) + whereClause);
    query.bind(1, value);
    if (!query.step()) return std::nullopt;
    return toUser(query);
}

std::optional<UserPtr> SqliteUserRepository::findByUsername(const std::string& username) const {
    return findOne("WHERE u.username = ?", username);
}

std::optional<UserPtr> SqliteUserRepository::findByStudentNumber(const std::string& studentNumber) const {
    return findOne("WHERE s.student_number = ?", studentNumber);
}

std::optional<UserPtr> SqliteUserRepository::findHeadOfDepartment(int departmentId) const {
    Statement query(db_, std::string(kSelectUser) + "WHERE st.role = 'HOD' AND st.department_id = ?");
    query.bind(1, departmentId);
    if (!query.step()) return std::nullopt;
    return toUser(query);
}

std::vector<UserPtr> SqliteUserRepository::findAll() const {
    Statement query(db_, std::string(kSelectUser) + "ORDER BY u.id");
    std::vector<UserPtr> users;
    while (query.step()) users.push_back(toUser(query));
    return users;
}

int SqliteUserRepository::countAll() const {
    Statement query(db_, "SELECT COUNT(*) FROM app_user");
    query.step();
    return query.columnInt(0);
}

void SqliteUserRepository::save(UserPtr& user) {
    TransactionScope scope(db_);
    int newId = 0;
    if (user->getId() == 0) {
        Statement insert(db_,
                         "INSERT INTO app_user (username, password_hash, salt, full_name, role) "
                         "VALUES (?, ?, ?, ?, ?)");
        insert.bind(1, user->getUsername())
            .bind(2, user->getPasswordHash())
            .bind(3, user->getSalt())
            .bind(4, user->getFullName())
            .bind(5, toString(user->getRole()));
        insert.run();
        newId = static_cast<int>(db_.lastInsertId());

        if (const auto* student = dynamic_cast<const Student*>(user.get())) {
            Statement child(db_, "INSERT INTO student (user_id, student_number) VALUES (?, ?)");
            child.bind(1, newId).bind(2, student->getStudentNumber());
            child.run();
        } else if (user->getRole() == Role::Lecturer || user->getRole() == Role::Hod) {
            Statement child(db_, "INSERT INTO staff (user_id, role, department_id) VALUES (?, ?, ?)");
            child.bind(1, newId).bind(2, toString(user->getRole())).bind(3, user->getDepartmentId());
            child.run();
        }
    } else {
        Statement update(db_, "UPDATE app_user SET password_hash = ?, salt = ?, full_name = ? WHERE id = ?");
        update.bind(1, user->getPasswordHash())
            .bind(2, user->getSalt())
            .bind(3, user->getFullName())
            .bind(4, user->getId());
        update.run();
    }
    scope.commit();
    if (newId != 0) user->assignId(newId);
}

}  // namespace lms
