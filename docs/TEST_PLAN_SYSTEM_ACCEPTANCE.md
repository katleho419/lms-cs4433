# System and Acceptance Test Plan

Code: `tests/test_system.cpp` (46 tests). Results: `TEST_RESULTS.md`. Defects: `DEFECT_LOG.md`.

## 1. Approach

**System tests (SYS-xx)** run the whole application as a user would. Each test types a script of keystrokes into the real
`ConsoleApp` (menus, prompts, input checking), which calls the real services, which write to a real SQLite database
(in memory, or a file for the restart test). The test then checks two things: what the screen said, and what the database
holds. Nothing in the path is stubbed. Passwords are hashed with the real scrypt, which is why this suite is the slowest
(about 65 of the 73 seconds of the full run).

The clock is fixed (`FixedClock`, 2026-11-01 08:00 UTC, assignment due 7 days later), so late-penalty results do not depend
on the day the test is run.

**Acceptance tests (ACC-xx)** are stories, one per role, taken from the Phase 1 requirements: a student submits and sees a
mark only after release; a lecturer sets and marks work; the Head of Department moderates, adjusts, releases and checks the
audit trail; the administrator sets up a term from an empty system; and marks survive closing and reopening the program.
A story passes only if the person in it can reach the goal using the menus alone.

**Smoke test of the real executable.** Besides the in-process tests, the built `lms` program was run with piped input and
`--seed-demo` against a database file: a student logs in and submits, the program exits with code 0, and a second run
(new process, same file) shows the lecturer the stored submission.

## 2. Test data

Seeded by `World` (tests/TestSupport.hpp): one administrator, two Heads of Department (`hod` owns the course's department,
`hodOther` does not), two lecturers (`lec1` teaches CS4433, `lec2` does not), three students (`stu1`, `stu2` enrolled,
`stu3` not enrolled), course CS4433, "Assignment 1" due at start + 7 days, maximum mark 100, late penalty 10% of the maximum
per late day for up to 3 days, then zero.

## 3. Traceability: use case flow to system test

### UC-01 Submit assignment

| Flow | Test | What is checked | Result |
|---|---|---|---|
| Main | SYS01 | On-time answer stored as version 1, status Submitted, no late days | Pass |
| A1 resubmit before deadline | SYS02 | New version created, old kept | Pass |
| A2 late submission | SYS03, SYS04 | 1 hour late = 1 day; 2.5 days late = 3 days (rounded up) | Pass |
| A3 extension | SYS10 | Extension for one student makes late work on time, others unaffected | Pass |
| E1 empty answer | SYS05 | Refused, nothing stored | Pass |
| E2 not enrolled | SYS06 | Refused | Pass |
| E3 unknown assignment | SYS07 | Message shown, no crash | Pass |
| E4 resubmit after deadline | SYS08 | Refused | Pass |
| E5 resubmit after marking | SYS09 | Refused | Pass |
| Size limit | SYS11 | Oversized answer refused | Pass |

### UC-02 Mark submission

| Flow | Test | What is checked | Result |
|---|---|---|---|
| Main | SYS20 | Raw 80 on time gives final 80, status Marked, one audit entry | Pass |
| A1 late penalty | SYS21, SYS22b | 2 days late: 80 becomes 60. Exactly 3 days late: 80 becomes 50 | Pass |
| A2 beyond window | SYS22 | More than 3 days late: final 0 | Pass |
| E1 mark above maximum | SYS23 | Refused | Pass |
| E2 not a number | SYS24 | Program asks again | Pass |
| E3 other course's lecturer | SYS25 | Refused | Pass |
| E4 / A3 re-mark | SYS26 | Reason required; with a reason it is accepted and audited | Pass |
| E5 unknown submission | SYS27 | Message shown | Pass |
| Role separation | SYS28 | Student cannot reach the marking menu | Pass |

### UC-03 Moderate marks

| Flow | Test | What is checked | Result |
|---|---|---|---|
| Main | SYS30 | HoD approves, then releases; student sees the mark | Pass |
| BR-05 | SYS31 | Student sees no mark or feedback before release | Pass |
| A1 decline | SYS32 | Answering "n" changes nothing | Pass |
| A2 adjust | SYS33, SYS34 | Adjusted with reason and audited; adjustment without reason refused | Pass |
| E2 other department | SYS35 | HoD of another department refused | Pass |
| E3 unmarked work | SYS36 | Approval refused | Pass |
| E4 stale approval | SYS37, SYS41 | A mark changed during review is not approved; approve-all approves only what was shown | Pass |
| A3 return to lecturer | SYS38 | Work becomes markable again | Pass |
| A4 partial release | SYS39 | Unmoderated work stays hidden after release | Pass |
| E5 after moderation | SYS40 | Lecturer cannot change a moderated mark | Pass |

### Security and general behaviour

| Test | What is checked | Result |
|---|---|---|
| SYS50, SYS51 | Wrong password and unknown user give the same message | Pass |
| SYS52 | Five failed logins end the program | Pass |
| SYS53 | End of input inside a menu exits cleanly | Pass |
| SYS54 | SQL injection text in login and answers is harmless | Pass |
| SYS55 | Administrator creates and enrols a student, who can then submit | Pass |
| SYS56 | Weak password for a new user refused | Pass |
| SYS57 | After a password change the old password no longer works | Pass |

## 4. Acceptance scenarios

| ID | Role | Scenario | Acceptance criterion | Result |
|---|---|---|---|---|
| ACC01 | Student | Open list, submit, wait for marking, moderation and release | Sees "awaiting marks" until release, then the mark and feedback | Pass |
| ACC02 | Lecturer | Create "Assignment 2", receive a late submission, mark it | Assignment appears in the list; marking shows the late days and the penalised final mark (90 becomes 80) | Pass |
| ACC03 | Head of Department | Review two marks, adjust one with a reason, approve all, release, read audit | Students see 50 and 85; audit shows the HoD and the reason | Pass |
| ACC04 | Administrator | Create department, course and lecturer from an empty system | All three exist and can be listed | Pass |
| ACC05 | Any | Close the program and reopen the same database file | Submission, marks and audit entry are still there | Pass |

## 5. Entry and exit criteria

- Entry: unit and integration tests pass (159/159); build has no compiler warnings.
- Exit: every test above passes; no open Critical or High defect; business-logic line coverage still at least 70%.
  All three were met on 2026-11-02 (see `TEST_RESULTS.md`).

## 6. Limits

- The console is the only interface, so these tests cover it completely. A future web interface would need its own system tests.
- Two users working at the same instant are simulated by running two scripted sessions one after the other (SYS37), not by true parallel threads.
- Non-functional checks (response time, large numbers of users) were not run; the brief does not set targets for them.
