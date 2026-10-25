# Differences between the Phase 3 design and the implementation

| # | Design | Implementation | Reason |
|---|---|---|---|
| 1 | Repositories for User, Assignment, Submission, Audit | Added `IDepartmentRepository` and `ICourseRepository` | Courses, departments, enrolment and lecturer links needed persistence |
| 2 | Six services | Added `AdminService` | Account, department, course, staffing and enrolment use cases had no owner |
| 3 | No unit of work | `IUnitOfWork` implemented by `Database::runInTransaction` | Mark + audit entry must commit or fail together |
| 4 | No clock abstraction | `IClock` (`SystemClock`, `FixedClock`) | Late rules testable without waiting |
| 5 | Submission states Submitted/Late -> Marked -> Moderated -> Released | Same, plus Marked -> Submitted/Late when the HOD returns work | Return-to-lecturer path |
| 6 | Extension visible to students | Extensions applied to deadline checks but not shown on the student's assignment list | Deliberately left for change request CR1 (Phase 8) |
| 7 | UserFactory | `createUser` for new accounts, `restore` for rows read from the database | Hashing only happens for new accounts |

Update the Phase 3 class diagram with items 1-4 and 7 before submission.
