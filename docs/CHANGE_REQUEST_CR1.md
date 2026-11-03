# Change Request CR1: Extension for the whole class

| | |
|---|---|
| Change request | CR1 |
| Title | A lecturer can give a deadline extension to the whole class, and every student sees their own deadline |
| Raised by | Course staff (requirement added after release 1.0.0) |
| Date raised | 2026-11-03 |
| Type of maintenance | Perfective (new capability that users asked for). It also removes known limitation 1 of release 1.0.0 |
| Priority | Medium: staff can work around it by extending students one by one |
| Status | Implemented, verified, released as version 1.1.0 |
| Branch / pull request | `feature/cr1-class-extension` |

## 1. The request

Two things were missing in version 1.0.0:

1. **Whole-class extension.** A lecturer could give an extension to one student at a time. When a whole class needs more time
   (a power cut, a faulty submission link, an unclear question) the lecturer had to repeat the step for every student.
2. **Visible deadline.** An extension was applied when a student submitted, but the student's assignment list still showed the
   original deadline, so a student with an extension could believe they were late.

This was recorded as deliberately unimplemented in `DESIGN_DELTA.md` (row 6) and as known limitation 1 in the 1.0.0 release notes.

## 2. Requirements and acceptance criteria

| ID | Requirement | Acceptance criterion | Test |
|---|---|---|---|
| CR1-1 | A lecturer of the course can give every enrolled student the same new deadline in one action | Every enrolled student has the extension; students not enrolled do not | `EveryEnrolledStudentGetsTheNewDeadlineAndNobodyElse`, SYS58 |
| CR1-2 | Only the lecturer of that course may do it | Student, Head of Department and a lecturer of another course are refused and nothing changes | `OnlyTheLecturerOfTheCourseMayGrantIt`, SYS60 |
| CR1-3 | The new deadline must be later than the assignment's deadline | An equal or earlier deadline is refused; one second later is accepted | `NewDeadlineMustBeLaterThanTheAssignmentDeadline` |
| CR1-4 | An extension is never shortened | A student who already has a later (or equal) extension keeps it and the lecturer is told how many were kept | `ALongerIndividualExtensionIsNeverShortened`, SYS61 |
| CR1-5 | All or nothing | If saving fails half way, no student gets an extension | `IsAllOrNothingWhenTheDatabaseFailsHalfWay` |
| CR1-6 | A course with no students is refused with a clear message | `No students are enrolled in this course.` | `ACourseWithNoStudentsIsRefused` |
| CR1-7 | Granting twice is harmless | The second grant changes nothing and reports 0 extended | `ARepeatedGrantIsHarmless` |
| CR1-8 | Work submitted inside the extension is on time for every student | Late days are 0 for all enrolled students | `MakesLateWorkOnTimeForEveryEnrolledStudent`, SYS58 |
| CR1-9 | The student's assignment list shows the deadline that applies to that student | Shows the extended deadline and the original one; students without an extension see no change | `EachStudentSeesTheirOwnEffectiveDeadline`, SYS59 |
| CR1-10 | Nothing that worked before changes | All 207 earlier tests pass without changing what they check | regression run |

## 3. Options considered

| Option | How | Verdict |
|---|---|---|
| A. A "class deadline" column on the assignment | One field, one place | Rejected. It changes the assignment's own deadline for everyone, so nobody can tell later which students had an extension, and it needs a database change |
| B. One extension row per enrolled student, written together | Reuses the existing `extension` table and the existing rule that finds a student's deadline | **Chosen.** No database change, so installed data files keep working. Every rule that already uses the student's deadline (late days, resubmission) works for class extensions with no change |
| C. The console loops over the students and calls the one-student function | No service change | Rejected. Not all-or-nothing, and the rule "never shorten" would live in the user interface |

## 4. Impact analysis

| Area | Affected? | Detail | Risk |
|---|---|---|---|
| Database schema | No | The `extension` table already holds one row per assignment and student. `PRAGMA user_version` stays 1 | None. A 1.0.0 data file opens in 1.1.0 and the other way round |
| Domain (`Entities`, `Submission`, penalty rules) | No | | None |
| Data layer (repositories) | No | `findExtension`, `saveExtension` and `studentIdsOf` already exist | None |
| Service: `AssignmentService` | Yes | New `grantClassExtension` and result type `ClassExtensionResult`; the constructor takes the unit of work (`IUnitOfWork`) so the change is all-or-nothing | Low. One call site in `main.cpp` and two in the tests |
| Service: `SubmissionService` | Yes | `AssignmentRow` gains `effectiveDeadline`, filled with the rule that already decides lateness. Submitting is unchanged | Low. The list now shows the same deadline the submit rule uses, so the two cannot disagree |
| Console (`ConsoleApp`) | Yes | Lecturer option 4 accepts `0` for the whole class; the student list adds `(extended from ...)`. The menu numbers do not change | Low. The prompt text changed: `Student user id (0 for the whole class): ` |
| Permissions | No | Uses the existing `GrantExtension` permission (lecturers only) and the existing course check | None |
| Audit log | No | Extensions were never audited (they do not change marks) | None. See limitation 2 below |
| Build / CI | No | No new files, no new dependencies | None |
| Tests | Yes | 14 new tests. Two existing test files changed only to pass the extra constructor argument | Low |
| Documents | Yes | Manual (lecturer and student sections), release notes, test results, metrics, coverage, design differences, this document | None |
| Deployment | No | Same installation steps; build, install over the old program, no data migration | None |

Regression risk was judged low because the change adds one function and one field, and reuses the data and rules that
already existed. The whole existing suite was run to check that.

## 5. What was changed

| File | Change |
|---|---|
| `src/service/AssignmentService.hpp/.cpp` | `grantClassExtension`, `ClassExtensionResult`, unit of work parameter |
| `src/service/SubmissionService.hpp/.cpp` | `effectiveDeadline` in `AssignmentRow` |
| `src/ui/ConsoleApp.cpp` | Option 4 accepts `0`; student list shows the extension; menu text |
| `src/app/main.cpp` | Passes the unit of work |
| `CMakeLists.txt` | Version 1.1.0 |
| `tests/TestSupport.hpp`, `tests/test_system.cpp` | Pass the unit of work (no change to what is tested) |
| `tests/test_services.cpp`, `tests/test_system.cpp` | 14 new tests |

About 80 lines of production code changed and about 190 lines of tests were added.

## 6. Design decisions and edge cases

| Situation | Decision | Reason |
|---|---|---|
| A student already has a later extension | Keep it; count it as "unchanged" | An extension that was granted must not be taken away by a later, shorter one |
| A student already submitted late, then the class gets an extension | The submission stays LATE with its late days | Marks and penalties already decided are not changed behind the lecturer's back. The lecturer can ask the Head of Department to adjust a mark (this keeps an audit entry) |
| Student enrolled after the class extension was granted | Does not get it | The extension is a record for the students enrolled at that moment. The lecturer can repeat the action; already extended students are left unchanged |
| Menu numbering | Kept; `0` inside option 4 means "whole class" | Keeps every existing menu number, manual page and test valid |
| Message when nobody changes | `Extension granted to 0 student(s) ...` followed by how many kept theirs | Honest and short |

## 7. Verification

| Check | Result |
|---|---|
| New tests (14: 9 service, 1 assignment list, 4 system) | All pass |
| All earlier tests (207) | All pass, unchanged assertions |
| Total | 221 of 221 |
| Business-logic line coverage | 98.6% (was 98.2%); `AssignmentService.cpp` and `SubmissionService.cpp` 100% |
| clang-tidy | 0 findings |
| clang-format | Clean |
| Metrics | Highest complexity unchanged (14); no function over the hard limits |
| Real program | Run through the console: class extension granted, both students' lists show `(extended from ...)`, repeated shorter grant kept both |

**Tests that can fail.** Each rule was removed on purpose to check that a test notices:

| Fault introduced | Tests that failed |
|---|---|
| Class extension overwrites a longer extension | 3 (`ALongerIndividualExtensionIsNeverShortened`, `ARepeatedGrantIsHarmless`, SYS61) |
| No transaction around the loop | 1 (`IsAllOrNothingWhenTheDatabaseFailsHalfWay`) |
| Student list shows the original deadline | 2 (`EachStudentSeesTheirOwnEffectiveDeadline`, SYS59) |
| Last student skipped | 10 |

Each fault was then removed and the suite passed again.

**How the tests were run.** The development environment could no longer download GoogleTest, so the tests above were run
with a small stand-in runner (not part of the project) that supports the macros the tests use. The real GoogleTest
run is the `build-test` check on the pull request, which is the evidence for this change request.

## 8. Remaining limitations

1. The lecturer cannot see a list of who has an extension. The student's own list shows it.
2. Extensions are not written to the audit log. Marks are; deadlines were never part of that log.
3. A student enrolled after the class extension must be extended separately (repeat the class extension).

## 9. Approval

| | |
|---|---|
| Impact analysis reviewed by | [Mafeta Katleho] |
| Date | 2026-11-03 |
| Decision | Approved and implemented in version 1.1.0 |
