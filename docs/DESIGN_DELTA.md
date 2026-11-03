# Differences between the Phase 3 design and the implementation

| # | Design | Implementation | Reason |
|---|---|---|---|
| 1 | Repositories for User, Assignment, Submission, Audit | Added `IDepartmentRepository` and `ICourseRepository` | Courses, departments, enrolment and lecturer links needed persistence |
| 2 | Six services | Added `AdminService` | Account, department, course, staffing and enrolment use cases had no owner |
| 3 | No unit of work | `IUnitOfWork` implemented by `Database::runInTransaction` | Mark + audit entry must commit or fail together |
| 4 | No clock abstraction | `IClock` (`SystemClock`, `FixedClock`) | Late rules testable without waiting |
| 5 | Submission states Submitted/Late -> Marked -> Moderated -> Released | Same, plus Marked -> Submitted/Late when the HOD returns work | Return-to-lecturer path |
| 6 | Extension visible to students | Version 1.0.0: extensions applied to deadline checks but not shown on the student's assignment list. **Version 1.1.0 (CR1):** shown as `due <new> (extended from <original>)` | Left for change request CR1 and delivered in Phase 8 (`CHANGE_REQUEST_CR1.md`) |
| 10 | Extensions granted one student at a time | Version 1.1.0 (CR1): `AssignmentService::grantClassExtension` gives the whole class an extension in one transaction; `AssignmentService` now takes `IUnitOfWork` | Phase 8 change request CR1 |
| 7 | UserFactory | `createUser` for new accounts, `restore` for rows read from the database | Hashing only happens for new accounts |
| 8 | Students see their own work and marks | `Submission::redactedForStudent()`, used by `SubmissionService::assignmentsFor` | BR-05 enforced in the service layer, not only the screen (DEF-01) |
| 9 | Approve and approve-all take only ids | `ModerationService::review`, `approve(..., reviewedFinalMark)`, `approveReviewed` | UC-03 E4 stale-mark protection (DEF-02) |

The Phase 3 class diagram is kept as the original design baseline. This document records every change made during implementation and the reason for it.
