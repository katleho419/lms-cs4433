// Student use cases: submit, resubmit, and view assignments and released marks.
#ifndef LMS_SERVICE_SUBMISSIONSERVICE_HPP
#define LMS_SERVICE_SUBMISSIONSERVICE_HPP

#include <optional>
#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "service/IUnitOfWork.hpp"
#include "util/Clock.hpp"

namespace lms {

// One line of a student's assignment list.
struct AssignmentRow {
    Assignment assignment;
    std::string courseCode;
    std::optional<Submission> current;  // the student's current version, if any
};

class SubmissionService {
public:
    static constexpr std::size_t kMaxContentBytes = 100000;
    static constexpr std::int64_t kSecondsPerDay = 86400;

    SubmissionService(ISubmissionRepository& submissions, IAssignmentRepository& assignments,
                      ICourseRepository& courses, const IClock& clock, IUnitOfWork& unitOfWork)
        : submissions_(submissions),
          assignments_(assignments),
          courses_(courses),
          clock_(clock),
          unitOfWork_(unitOfWork) {}

    // Whole days late: 0 when now <= deadline, otherwise every started
    // 24-hour period counts as one day (BR-08).
    static int computeLateDays(const DateTime& deadline, const DateTime& now);

    // The student's extension if they have one that is later than the
    // assignment deadline, otherwise the assignment deadline (BR-01).
    DateTime effectiveDeadlineFor(const Assignment& assignment, int studentId) const;

    // First submission, or a resubmission before the deadline (BR-03).
    // Late work is accepted and flagged LATE (BR-02). Returns the saved version.
    Submission submit(const User& student, int assignmentId, const std::string& content);

    // Every assignment in the student's courses, with the student's current submission.
    std::vector<AssignmentRow> assignmentsFor(const User& student) const;

private:
    ISubmissionRepository& submissions_;
    IAssignmentRepository& assignments_;
    ICourseRepository& courses_;
    const IClock& clock_;
    IUnitOfWork& unitOfWork_;
};

}  // namespace lms

#endif
