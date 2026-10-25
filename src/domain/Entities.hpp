// Plain domain entities (design: Figure 1b). Ids are 0 until the repository
// inserts the entity and calls assignId().
#ifndef LMS_DOMAIN_ENTITIES_HPP
#define LMS_DOMAIN_ENTITIES_HPP

#include <optional>
#include <string>
#include <utility>

#include "util/DateTime.hpp"

namespace lms {

class Department {
public:
    Department(int id, std::string name) : id_(id), name_(std::move(name)) {}
    int getId() const { return id_; }
    const std::string& getName() const { return name_; }
    void assignId(int id) { id_ = id; }

private:
    int id_;
    std::string name_;
};

class Course {
public:
    Course(int id, std::string code, std::string title, int departmentId)
        : id_(id), code_(std::move(code)), title_(std::move(title)), departmentId_(departmentId) {}
    int getId() const { return id_; }
    const std::string& getCode() const { return code_; }
    const std::string& getTitle() const { return title_; }
    int getDepartmentId() const { return departmentId_; }
    void assignId(int id) { id_ = id; }

private:
    int id_;
    std::string code_;
    std::string title_;
    int departmentId_;
};

class Assignment {
public:
    Assignment(int id, int courseId, std::string title, DateTime deadline, int maxMark)
        : id_(id), courseId_(courseId), title_(std::move(title)), deadline_(deadline), maxMark_(maxMark) {}
    int getId() const { return id_; }
    int getCourseId() const { return courseId_; }
    const std::string& getTitle() const { return title_; }
    const DateTime& getDeadline() const { return deadline_; }
    int getMaxMark() const { return maxMark_; }
    void assignId(int id) { id_ = id; }

    // True while the original deadline has not passed.
    bool isOpenAt(const DateTime& t) const { return t <= deadline_; }

private:
    int id_;
    int courseId_;
    std::string title_;
    DateTime deadline_;
    int maxMark_;
};

// An individual deadline extension for one student on one assignment.
class Extension {
public:
    Extension(int id, int assignmentId, int studentId, DateTime newDeadline, int grantedBy)
        : id_(id),
          assignmentId_(assignmentId),
          studentId_(studentId),
          newDeadline_(newDeadline),
          grantedBy_(grantedBy) {}
    int getId() const { return id_; }
    int getAssignmentId() const { return assignmentId_; }
    int getStudentId() const { return studentId_; }
    const DateTime& getNewDeadline() const { return newDeadline_; }
    int getGrantedBy() const { return grantedBy_; }
    void assignId(int id) { id_ = id; }

private:
    int id_;
    int assignmentId_;
    int studentId_;
    DateTime newDeadline_;
    int grantedBy_;
};

// One line of the append-only audit log: who changed what, when, and why.
class AuditEntry {
public:
    AuditEntry(int id, int submissionId, int changedBy, DateTime changedAt, std::optional<int> oldValue,
               int newValue, std::string reason)
        : id_(id),
          submissionId_(submissionId),
          changedBy_(changedBy),
          changedAt_(changedAt),
          oldValue_(oldValue),
          newValue_(newValue),
          reason_(std::move(reason)) {}
    int getId() const { return id_; }
    int getSubmissionId() const { return submissionId_; }
    int getChangedBy() const { return changedBy_; }
    const DateTime& getChangedAt() const { return changedAt_; }
    const std::optional<int>& getOldValue() const { return oldValue_; }
    int getNewValue() const { return newValue_; }
    const std::string& getReason() const { return reason_; }
    void assignId(int id) { id_ = id; }

private:
    int id_;
    int submissionId_;
    int changedBy_;
    DateTime changedAt_;
    std::optional<int> oldValue_;
    int newValue_;
    std::string reason_;
};

}  // namespace lms

#endif
