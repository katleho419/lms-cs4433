#include "domain/Submission.hpp"

#include <utility>

#include "util/Errors.hpp"

namespace lms {

std::string toString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Submitted: return "SUBMITTED";
        case SubmissionStatus::Late: return "LATE";
        case SubmissionStatus::Marked: return "MARKED";
        case SubmissionStatus::Moderated: return "MODERATED";
        case SubmissionStatus::Released: return "RELEASED";
    }
    throw ValidationError("Unknown submission status.");
}

SubmissionStatus submissionStatusFromString(const std::string& text) {
    if (text == "SUBMITTED") return SubmissionStatus::Submitted;
    if (text == "LATE") return SubmissionStatus::Late;
    if (text == "MARKED") return SubmissionStatus::Marked;
    if (text == "MODERATED") return SubmissionStatus::Moderated;
    if (text == "RELEASED") return SubmissionStatus::Released;
    throw ValidationError("Unknown submission status: " + text);
}

Submission::Submission(int id, int assignmentId, int studentId, int version, std::string content,
                       DateTime submittedAt, SubmissionStatus status, int lateDays,
                       std::optional<int> rawMark, std::optional<int> finalMark, std::string feedback)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      version_(version),
      content_(std::move(content)),
      submittedAt_(submittedAt),
      status_(status),
      lateDays_(lateDays),
      rawMark_(rawMark),
      finalMark_(finalMark),
      feedback_(std::move(feedback)) {}

void Submission::transitionTo(SubmissionStatus next) {
    using S = SubmissionStatus;
    bool allowed = false;
    switch (status_) {
        case S::Submitted:
            allowed = (next == S::Marked);
            break;
        case S::Late:
            allowed = (next == S::Marked);
            break;
        case S::Marked:
            allowed = (next == S::Marked || next == S::Moderated ||
                       (next == S::Submitted && lateDays_ == 0) || (next == S::Late && lateDays_ >= 1));
            break;
        case S::Moderated:
            allowed = (next == S::Moderated || next == S::Released);
            break;
        case S::Released:
            allowed = false;
            break;
    }
    if (!allowed) {
        throw BusinessRuleError("A submission that is " + toString(status_) + " cannot become " +
                                toString(next) + ".");
    }
    status_ = next;
}

void Submission::applyMark(int rawMark, int finalMark, const std::string& feedback) {
    transitionTo(SubmissionStatus::Marked);
    rawMark_ = rawMark;
    finalMark_ = finalMark;
    feedback_ = feedback;
}

void Submission::adjustFinalMark(int newFinalMark) {
    if (status_ != SubmissionStatus::Marked && status_ != SubmissionStatus::Moderated) {
        throw BusinessRuleError("Only marked or moderated work can have its mark adjusted.");
    }
    finalMark_ = newFinalMark;
}

void Submission::returnForRemarking() {
    transitionTo(lateDays_ == 0 ? SubmissionStatus::Submitted : SubmissionStatus::Late);
}

bool Submission::canResubmit(const DateTime& now, const DateTime& effectiveDeadline) const {
    return status_ == SubmissionStatus::Submitted && now <= effectiveDeadline;
}

}  // namespace lms
