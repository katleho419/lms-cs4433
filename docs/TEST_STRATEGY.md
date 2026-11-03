# Test Plan and Test Strategy

Moderated Learning Management System, CS4433 Software Engineering. Phase 5: Verification and Validation. Version 1.0. This plan covers the C++17 implementation in the repository (layers: domain, security, data, service, UI).


## 1. Purpose and scope

The plan describes how the system is verified (built right, against the design) and validated (the right system, against the use cases and business rules). It applies to all Must requirements implemented in Phase 4. Out of scope: performance and load testing beyond a sanity check, GUI testing (the interface is a console), and penetration testing (security is checked at the level of password storage, access control and SQL injection).


## 2. Test approach: the V-Model

Each development phase on the left of the V has a test level on the right that checks its artefact. Lower levels run first and faster; a level starts only when the previous level meets its exit criteria.

| V-Model level | Artefact verified | What is tested | Tools and method |
|---|---|---|---|
| Unit testing | Detailed design: class diagrams (Fig 1a, 1b), state machine, Strategy pattern | Single classes in isolation: DateTime, Validation, PasswordHasher, PasswordPolicy, User and permissions, UserFactory, Penalty policies, Submission state machine | GoogleTest; white-box; no database; boundary and equivalence tests |
| Integration testing | Architecture and database design: layers, Repository pattern, services (Fig 1c, 1d), ERD and SQL schema | Services working with real SQLite repositories on an in-memory database; transactions and rollback; SQL triggers and constraints; failure injection with stub repositories | GoogleTest; grey-box; in-memory SQLite; stubs that throw to simulate disk failure |
| System testing | Software requirements (SRS) and the use case specifications UC-01 Submit assignment, UC-02 Mark submission, UC-03 Moderate marks | Complete use cases, main, alternative and exception flows, through the console UI with scripted input; business rules BR-01 to BR-16; security requirements | GoogleTest driving ConsoleApp with streams, plus manual run of the real executable; black-box |
| Acceptance testing | Stakeholder needs and the original requirements (Phase 1) | End-to-end scenarios per role (administrator, lecturer, Head of Department, student) on a seeded database, including the late-submission and moderation story | Scripted acceptance scenarios run on the built executable with --seed-demo and a fixed clock; checked against acceptance criteria |


## 3. Entry and exit criteria

| Level | Entry criteria | Exit criteria |
|---|---|---|
| Unit | Code compiles with -Wall -Wextra -Wpedantic -Wshadow -Wconversion and no errors. Class design reviewed. Test cases written from the design. | All unit tests pass. Every public function of the domain, util and security classes has at least one test. No open Critical or High defect in these classes. |
| Integration | Unit exit criteria met. Schema loads on an empty database. Repository interfaces stable. | All integration tests pass. Every service method has tests for its success path and each failure path. Transaction rollback demonstrated for marking, moderation and submission. Line coverage of the business-logic layer is at least 70% (target 85%), and the actual figure is reported. |
| System | Integration exit criteria met. Executable builds from a clean clone with the documented steps. Use case specifications baselined. | Every flow in UC-01 to UC-03 (main, alternative, exception) has a system test, and all pass or have an accepted, logged defect. No open Critical defect; no open High defect without a workaround. |
| Acceptance | System exit criteria met. Demo data and fixed-clock build available. Acceptance scenarios agreed. | All acceptance scenarios pass. Open defects are Low severity only. Defect log is complete (severity, cause, fix, retest result). Release candidate tagged. |

Suspension: if more than 20% of tests in a level fail, or the build breaks, testing at that level stops until the cause is fixed. Resumption: after the fix, the failed tests and the whole level are rerun (regression).


## 4. Test design techniques

Equivalence partitioning (EP) splits each input into classes that the program should treat the same way, and one test represents each class. Boundary value analysis (BVA) adds tests at and either side of each edge, where programs most often fail. The tables below are the basis of the automated tests.


## 4.1 Deadlines and late days (BR-01, BR-02)

Late days are counted in started 24-hour periods after the effective deadline: late_days = ceil(seconds late / 86400). The assignment deadline in the tests is fixed, so the offsets below are exact.

| Class | Submission time relative to deadline | Expected late days | Status |
|---|---|---|---|
| On time | Far before the deadline (-100 days) | 0 | Submitted |
| Boundary | 1 second before (-1 s) | 0 | Submitted |
| Boundary | Exactly at the deadline (0 s) | 0 | Submitted |
| Late day 1 | 1 s after (+1 s) | 1 | Late |
| Boundary | Exactly 24 h after (+86400 s) | 1 | Late |
| Boundary | 24 h and 1 s after | 2 | Late |
| Boundary | Exactly 72 h after (+3 days) | 3 | Late (last day in window) |
| Beyond window | 72 h and 1 s after | 4 | Late, mark is zero |
| Beyond window | 30 days after | 30 | Late, mark is zero |


## 4.2 Late penalty calculation (BR-08)

Rule: 10% of the maximum mark per late day, for up to 3 days, deducted from the raw mark, never below zero; after 3 days the mark is zero. Rounding is to the nearest whole mark.

| Test | Raw / max / late days | Expected final mark | Reason |
|---|---|---|---|
| EP: on time | 80 / 100 / 0 | 80 | No deduction |
| EP: late days 1, 2, 3 | 80 / 100 / 1, 2, 3 | 70, 60, 50 | Deduction of 10, 20, 30 |
| BVA: first day beyond window | 80 / 100 / 4 | 0 | Zero beyond 3 days |
| BVA: raw mark lower bound | 0 / 100 / 0 and 0 / 100 / 2 | 0 and 0 | Never negative |
| BVA: raw mark upper bound | 100 / 100 / 0 and 100 / 100 / 1 | 100 and 90 | Maximum mark |
| BVA: deduction vs mark | 10 / 100 / 1; 11 / 100 / 1; 5 / 100 / 1 | 0; 1; 0 | Deduction equal, one less, larger than mark |
| Rounding | 15 / 15 / 1; 4 / 4 / 1 | 13; 4 | 1.5 rounds to 2; 0.4 rounds to 0 |
| Other maximum marks | 50 / 50 / 3; 1000 / 1000 / 2 | 35; 800 | Penalty scales with the maximum |
| Invalid input | raw -1 or above max; max 0; late days -1 | ValidationError | Rejected, nothing calculated |


## 4.3 Other inputs

| Input and rule | Valid classes | Invalid classes | Boundary tests |
|---|---|---|---|
| Raw mark (BR-06, UC-02 E1) | 0 to maximum mark | Below 0, above maximum | -1, 0, 1, max-1, max, max+1 |
| Maximum mark of an assignment | 1 to 1000 | 0, negative, over 1000 | 0, 1, 1000, 1001 |
| Assignment deadline | After the current time | Equal to or before now | now-1 s, now, now+1 s |
| Extension deadline (BR-01) | After the assignment deadline, enrolled student | Equal or earlier; student not enrolled | deadline, deadline+1 s |
| Submission content (BR-04, UC-01 E1) | 1 to 100,000 characters, not blank | Empty, spaces only, over the limit | 0, 1, 100,000, 100,001 characters |
| Feedback (BR-07, UC-02 E2) | 1 to 5,000 characters | Empty, spaces only, over 5,000 | 0, 1, 5,000, 5,001 |
| Reason for change (BR-09, BR-12) | 1 to 600 characters | Empty, spaces only, over 600 | 0, 1, 600, 601 |
| Username | 3 to 32 characters from letters, digits, . _ - | Too short, too long, spaces or symbols, SQL text | 2, 3, 32, 33 characters |
| Password (security requirement) | 8 to 128 characters, letter and digit, not the username | Too short or long, no digit, no letter, same as username | 7, 8, 128, 129 characters |
| Date and time input | Real date in YYYY-MM-DD HH:MM | Wrong format, month 13, 30 February, hour 25 | 29 Feb in leap and non-leap years, 23:59, 24:00 |
| Resubmission (BR-03) | Before or at the deadline while still Submitted | After the deadline; after a late submission; after marking | deadline-1 s, deadline, deadline+1 s |
| Login | Correct username and password | Wrong password, unknown user, over-long input, SQL text | Username 64 and 65, password 128 and 129 characters |


## 4.4 Decision and state-based tests

- State machine: every one of the 25 pairs of states is tested, for on-time and late work, so allowed transitions succeed and all others are refused without changing the status.
- Role and permission matrix: each of the four roles is tested against all 13 permissions (allowed or denied).
- Relationship checks: lecturer of another course, Head of Department of another department, and a student who is not enrolled are each tested against every restricted function (BR-13).
- Failure injection: a stub repository that throws is used to prove that a mark and its audit entry, a submission, and each moderation action are rolled back together (UC-01 E4, UC-02 E5, UC-03 E5).


## 5. Business rule and use case coverage

Every business rule is traced to at least one automated test. Test names in the code start with the use case or rule number so they can be found by searching.

| Rule or flow | Requirement | Test level | Test name prefix |
|---|---|---|---|
| BR-01 Effective deadline and extensions | Extension replaces the deadline for one student | Integration, system | UC01_BR01_Submit, AssignmentService |
| BR-02 Late window of 3 days | Late accepted, zero after 3 days | Unit, integration | LateDaysBoundary, PenaltyBoundary, UC01_A2_A3_Submit |
| BR-03 Resubmission | Only before deadline and while unmarked | Integration | UC01_A1_Submit, UC01_E3_Submit |
| BR-04 Valid submissions | Non-empty, within size limit | Integration | UC01_E1_Submit |
| BR-05 Hidden until release | Students see no mark before release | Integration, system | UC03_BR05 |
| BR-06, BR-07 Mark range and feedback | Range check, feedback required | Integration | UC02_E1_Mark, UC02_E2_Mark |
| BR-08 Late penalty | 10% per day for 3 days | Unit, integration | PenaltyBoundary, UC02_A2_A3_Mark |
| BR-09 Reason for changes | Required to change an existing mark | Integration | UC02_A1_Mark |
| BR-10 After moderation | Only the Head of Department may change marks | Integration | UC02_E3_Mark |
| BR-12 Adjustments audited | Reason and audit entry | Integration | UC03_A1_Adjust |
| BR-13, BR-14 Department scope and review | Own department only, all marks visible | Integration | UC03_Review |
| BR-15 Release | Only moderated marks are released | Integration | UC03_Release |
| BR-16 Audit trail | Append-only, in the application and in the database | Integration | Schema.AuditEntriesAreAppendOnly, AuditRepository |
| UC-01 Submit assignment | Main, A1 to A3, E1 to E4 | System | UC01_* |
| UC-02 Mark submission | Main, A1 to A3, E1 to E5 | System | UC02_* |
| UC-03 Moderate marks | Main, A1 to A4, E1 to E5 | System | UC03_* |
| Security: password storage | Salted scrypt hash, no plain text | Unit | PasswordHasher, UserFactory |
| Security: SQL injection | Parameterised queries only | Integration | UserRepository.SqlInjectionInUsernameIsHarmless |
| Robustness: invalid input does not crash | Errors are reported, program continues | System | Console tests |

Not testable by automation, and handled by review or manual run: UC-01 E5 (session expiry, the console has no timed session) and A4 (cancel). UC-03 E4 (concurrent change) is tested by simulating the sequence lecturer re-marks after the Head of Department has reviewed.


## 6. Test environment and data

- Platform: Ubuntu Linux, g++ 13, CMake 3.28 with Ninja, SQLite 3.45, OpenSSL 3.0. The same build is run by the CI workflow on each push.
- Framework: GoogleTest 1.14 (found on the system or downloaded by CMake). Coverage: gcc --coverage with gcov, reported per layer.
- Database: unit and integration tests use an in-memory SQLite database created fresh for every test, so tests are independent and repeatable. System and acceptance tests use the real schema.
- Time: a FixedClock sets the current time to 2026-11-01 08:00 UTC and the assignment deadline to exactly 7 days later, so no test depends on the real date.
- Test data: one department structure with a Head of Department, two lecturers, three students (one not enrolled), a second department and Head of Department for access tests, and one assignment worth 100 marks. Passwords use a fast fake hash except where password behaviour is under test.
- Seeded demo database: admin, hod, lecturer, student1 and student2, created with ./build/lms --seed-demo, used for acceptance scenarios.


## 7. Defect management

Every failure found at any level is recorded in the defect log with an identifier, the test that found it, severity, cause, fix, and the result of the retest. A defect is closed only after the failing test passes and the full suite is rerun.

| Severity | Definition | Example |
|---|---|---|
| Critical | Data loss or corruption, security breach, crash, or wrong marks released | Student sees an unreleased mark; audit entry missing |
| High | A use case flow or business rule does not work and there is no workaround | Late penalty miscalculated |
| Medium | A rule or flow works only partly, or a workaround exists | A rule is not enforced in one path |
| Low | Cosmetic or message wording, no effect on results | Unclear error message |


## 8. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Tests depend on the clock and become flaky | All time comes from an injected clock; no test reads the system time. |
| Slow password hashing makes the suite slow | Fixtures use a fake hash; real scrypt is used only in the tests that need it. |
| Late-penalty rounding disputes | Rounding rule fixed in the requirement and covered by explicit tests. |
| Database errors are hard to cause | Stub repositories throw on save to prove rollback. |
| Coverage looks high but misses failure paths | Exception flows of each use case are listed and tested by name; coverage is checked per file. |
| Acceptance is performed by the author, not real users | Scenarios are scripted from the use cases and stakeholder needs, and the limitation is stated in the test report. |


## 9. Deliverables and reporting

- Test plan and strategy (this document).
- Test code in the tests folder, built by CMake with -DLMS_BUILD_TESTS=ON and run with ctest.
- Test results: the output of the test run, with pass and fail counts per level.
- Coverage report with the actual line coverage per layer and for the business-logic layer.
- Defect log and retest record.
- CI evidence: the workflow that builds and runs the tests on every push.
