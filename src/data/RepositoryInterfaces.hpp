// Repository interfaces (design: Figure 1d). Services depend on these, never
// on the SQLite classes. Two of them, IDepartmentRepository and
// ICourseRepository, were added during implementation; see docs/DESIGN_DELTA.md.
#ifndef LMS_DATA_REPOSITORYINTERFACES_HPP
#define LMS_DATA_REPOSITORYINTERFACES_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/IRepository.hpp"
#include "domain/Entities.hpp"
#include "domain/Submission.hpp"
#include "domain/User.hpp"

namespace lms {

using UserPtr = std::shared_ptr<User>;

class IUserRepository : public IRepository<UserPtr> {
public:
    virtual std::optional<UserPtr> findByUsername(const std::string& username) const = 0;
    virtual std::optional<UserPtr> findByStudentNumber(const std::string& studentNumber) const = 0;
    virtual std::optional<UserPtr> findHeadOfDepartment(int departmentId) const = 0;
    virtual std::vector<UserPtr> findAll() const = 0;
    virtual int countAll() const = 0;
};

class IDepartmentRepository : public IRepository<Department> {
public:
    virtual std::optional<Department> findByName(const std::string& name) const = 0;
    virtual std::vector<Department> findAll() const = 0;
};

class ICourseRepository : public IRepository<Course> {
public:
    virtual std::optional<Course> findByCode(const std::string& code) const = 0;
    virtual std::vector<Course> findAll() const = 0;
    virtual std::vector<Course> findByDepartment(int departmentId) const = 0;
    virtual std::vector<Course> findByLecturer(int lecturerId) const = 0;
    virtual std::vector<Course> findByStudent(int studentId) const = 0;
    virtual void addLecturer(int courseId, int lecturerId) = 0;
    virtual bool isLecturerOf(int courseId, int lecturerId) const = 0;
    virtual void enrol(int courseId, int studentId) = 0;
    virtual bool isEnrolled(int courseId, int studentId) const = 0;
    virtual std::vector<int> studentIdsOf(int courseId) const = 0;
};

class IAssignmentRepository : public IRepository<Assignment> {
public:
    virtual std::vector<Assignment> findByCourse(int courseId) const = 0;
    virtual std::optional<Extension> findExtension(int assignmentId, int studentId) const = 0;
    // Inserts, or replaces the existing extension for the same student and assignment.
    virtual void saveExtension(Extension& extension) = 0;
};

class ISubmissionRepository : public IRepository<Submission> {
public:
    // Every version of every student's work, oldest first.
    virtual std::vector<Submission> findByAssignment(int assignmentId) const = 0;
    // Only the current (highest) version for each student.
    virtual std::vector<Submission> findCurrentByAssignment(int assignmentId) const = 0;
    virtual std::optional<Submission> findLatest(int assignmentId, int studentId) const = 0;
    // The current version for each assignment this student has answered.
    virtual std::vector<Submission> findCurrentByStudent(int studentId) const = 0;
};

class IAuditRepository : public IRepository<AuditEntry> {
public:
    virtual std::vector<AuditEntry> findBySubmission(int submissionId) const = 0;
};

}  // namespace lms

#endif
