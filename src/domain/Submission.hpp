// A student's work for one assignment and its life cycle (design: Figures 1b
// and the Submission state machine). The state rules live here so that no
// service can put a submission into an illegal state.
#ifndef LMS_DOMAIN_SUBMISSION_HPP
#define LMS_DOMAIN_SUBMISSION_HPP

#include <optional>
#include <string>

#include "util/DateTime.hpp"

namespace lms {

enum class SubmissionStatus { Submitted, Late, Marked, Moderated, Released };

std::string toString(SubmissionStatus status);
SubmissionStatus submissionStatusFromString(const std::string& text);

class Submission {
public:
    Submission(int id, int assignmentId, int studentId, int version, std::string content,
               DateTime submittedAt, SubmissionStatus status, int lateDays,
               std::optional<int> rawMark, std::optional<int> finalMark, std::string feedback);

    int getId() const { return id_; }
    int getAssignmentId() const { return assignmentId_; }
    int getStudentId() const { return studentId_; }
    int getVersion() const { return version_; }
    const std::string& getContent() const { return content_; }
    const DateTime& getSubmittedAt() const { return submittedAt_; }
    SubmissionStatus getStatus() const { return status_; }
    int getLateDays() const { return lateDays_; }
    const std::optional<int>& getRawMark() const { return rawMark_; }
    const std::optional<int>& getFinalMark() const { return finalMark_; }
    // Empty when the work has not been marked.
    const std::string& getFeedback() const { return feedback_; }
    void assignId(int id) { id_ = id; }

    // Moves to `next` if the state machine allows it; otherwise throws
    // BusinessRuleError. Allowed moves:
    //   Submitted -> Marked        Late -> Marked        Marked -> Marked
    //   Marked -> Moderated        Moderated -> Moderated (HoD adjustment)
    //   Moderated -> Released
    //   Marked -> Submitted        (returned to lecturer, work was on time)
    //   Marked -> Late             (returned to lecturer, work was late)
    void transitionTo(SubmissionStatus next);

    // Stores the lecturer's marks and moves to Marked.
    void applyMark(int rawMark, int finalMark, const std::string& feedback);
    // Changes only the final mark (HoD adjustment). Allowed when Marked or Moderated.
    void adjustFinalMark(int newFinalMark);
    // Marked -> Submitted or Late, depending on lateDays.
    void returnForRemarking();

    // A student may resubmit only while the work is unmarked and the
    // effective deadline has not passed.
    bool canResubmit(const DateTime& now, const DateTime& effectiveDeadline) const;
    // Students see marks and feedback only after release.
    bool isVisibleToStudent() const { return status_ == SubmissionStatus::Released; }

private:
    int id_;
    int assignmentId_;
    int studentId_;
    int version_;
    std::string content_;
    DateTime submittedAt_;
    SubmissionStatus status_;
    int lateDays_;
    std::optional<int> rawMark_;
    std::optional<int> finalMark_;
    std::string feedback_;
};

}  // namespace lms

#endif
