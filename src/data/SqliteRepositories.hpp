// SQLite implementations of the repository interfaces. The only code outside
// Database.cpp that knows table and column names.
#ifndef LMS_DATA_SQLITEREPOSITORIES_HPP
#define LMS_DATA_SQLITEREPOSITORIES_HPP

#include "data/Database.hpp"
#include "data/RepositoryInterfaces.hpp"

namespace lms {

class SqliteUserRepository : public IUserRepository {
public:
    explicit SqliteUserRepository(Database& db) : db_(db) {}
    std::optional<UserPtr> findById(int id) const override;
    void save(UserPtr& user) override;
    std::optional<UserPtr> findByUsername(const std::string& username) const override;
    std::optional<UserPtr> findByStudentNumber(const std::string& studentNumber) const override;
    std::optional<UserPtr> findHeadOfDepartment(int departmentId) const override;
    std::vector<UserPtr> findAll() const override;
    int countAll() const override;

private:
    std::optional<UserPtr> findOne(const std::string& whereClause, const std::string& value) const;
    Database& db_;
};

class SqliteDepartmentRepository : public IDepartmentRepository {
public:
    explicit SqliteDepartmentRepository(Database& db) : db_(db) {}
    std::optional<Department> findById(int id) const override;
    void save(Department& department) override;
    std::optional<Department> findByName(const std::string& name) const override;
    std::vector<Department> findAll() const override;

private:
    Database& db_;
};

class SqliteCourseRepository : public ICourseRepository {
public:
    explicit SqliteCourseRepository(Database& db) : db_(db) {}
    std::optional<Course> findById(int id) const override;
    void save(Course& course) override;
    std::optional<Course> findByCode(const std::string& code) const override;
    std::vector<Course> findAll() const override;
    std::vector<Course> findByDepartment(int departmentId) const override;
    std::vector<Course> findByLecturer(int lecturerId) const override;
    std::vector<Course> findByStudent(int studentId) const override;
    void addLecturer(int courseId, int lecturerId) override;
    bool isLecturerOf(int courseId, int lecturerId) const override;
    void enrol(int courseId, int studentId) override;
    bool isEnrolled(int courseId, int studentId) const override;
    std::vector<int> studentIdsOf(int courseId) const override;

private:
    Database& db_;
};

class SqliteAssignmentRepository : public IAssignmentRepository {
public:
    explicit SqliteAssignmentRepository(Database& db) : db_(db) {}
    std::optional<Assignment> findById(int id) const override;
    void save(Assignment& assignment) override;
    std::vector<Assignment> findByCourse(int courseId) const override;
    std::optional<Extension> findExtension(int assignmentId, int studentId) const override;
    void saveExtension(Extension& extension) override;

private:
    Database& db_;
};

class SqliteSubmissionRepository : public ISubmissionRepository {
public:
    explicit SqliteSubmissionRepository(Database& db) : db_(db) {}
    std::optional<Submission> findById(int id) const override;
    void save(Submission& submission) override;
    std::vector<Submission> findByAssignment(int assignmentId) const override;
    std::vector<Submission> findCurrentByAssignment(int assignmentId) const override;
    std::optional<Submission> findLatest(int assignmentId, int studentId) const override;
    std::vector<Submission> findCurrentByStudent(int studentId) const override;

private:
    Database& db_;
};

class SqliteAuditRepository : public IAuditRepository {
public:
    explicit SqliteAuditRepository(Database& db) : db_(db) {}
    std::optional<AuditEntry> findById(int id) const override;
    // Audit entries are append-only: saving an entry that already has an id is an error.
    void save(AuditEntry& entry) override;
    std::vector<AuditEntry> findBySubmission(int submissionId) const override;

private:
    Database& db_;
};

}  // namespace lms

#endif
