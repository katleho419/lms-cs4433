# Code Review Record

| | |
|---|---|
| Subject | All production code in `src/` (53 files, 3,661 code lines) and the build files |
| Version reviewed | `main` at `00aedc3` (after pull request #5), plus the fixes made in this review |
| Date | 2026-11-03 |
| Type | Self review against `CODE_REVIEW_CHECKLIST.md` (35 items) |
| Reviewer | [Mafeta Katleho] |
| Tools | Source searches (grep), clang-tidy 18, cppcheck (CI), `tools/metrics.py`, `tools/coverage.py`, the test suite, a real run of the program |

## 1. Outcome

| | Count |
|---|---:|
| Checklist items | 35 |
| Pass | 29 (five of them with an observation, section 4) |
| Finding, fixed and retested | 6 items, 5 findings (R-01 to R-05) |
| Open findings | 0 |

Two of the five findings were defects in the software (DEF-03, DEF-04 in `DEFECT_LOG.md`); three were maintainability
and documentation problems. None of them was found by the earlier tests or analysers.

## 2. Checklist results

| ID | Result | Evidence |
|---|---|---|
| A1 | Pass | Traceability tables in the test documents. Rules removed on purpose to check the tests notice: late window (fails SYS22b), file permissions and reason limit (section 3). |
| A2 | Pass | UC exception flows SYS05 to SYS41 all pass; rollback tests with failing repositories. |
| A3 | Finding R-02 | The lecturer's reason limit was applied to the wrong text. Fixed. All other limits are tested at the boundary. |
| A4 | Pass | The only assignment to the status field is `status_ = next` in `Submission::transitionTo`; every other hit is a comparison. |
| B1 | Pass | Every query is prepared and bound. The only string joins are constant `SELECT` fragments plus a fixed `WHERE ... = ?`; no user text enters SQL. Injection tests SYS54 and the repository tests pass. |
| B2 | Pass | scrypt (OpenSSL), 16-byte random salt, `CRYPTO_memcmp`. No password text is passed to any output call (search of `ConsoleApp.cpp`). |
| B3 | Finding R-04 | Only secrets in the source are the demo accounts of `--seed-demo`, published in the README with no warning. Fixed. |
| B4 | Pass | All use case operations of all seven services check role and relationship (list produced from the code). The functions without a check are helpers or own-account operations; see observation R-08. |
| B5 | Pass | One message for both failures; `spendHashingTime` hashes a dummy password for an unknown user so timing does not reveal it; SYS50 and SYS51. |
| B6 | Finding R-02, R-03 | Limits were repeated as bare numbers in eight places and one was checked on the wrong text. Fixed: one header, `util/Limits.hpp`. |
| B7 | Finding R-01 | The database file was created `-rw-r--r--`. Fixed. |
| B8 | Pass | Search for strcpy, strcat, sprintf, gets, system, popen, rand, atoi, strtok: no use (the hits were a variable named `system` and comments). |
| B9 | Pass | `readSecret` switches terminal echo off and restores it; see observation R-07. |
| C1 | Pass | The one empty `catch` (`ConsoleApp::nameOf`) was fixed in the static analysis step; none remain (clang-tidy 0). |
| C2 | Pass | Two catch-alls (`Database` constructor, `AuthService::changePassword`); both clean up and rethrow. See observation R-06. |
| C3 | Pass | Services use `IUnitOfWork::runInTransaction`; rollback tests (`FailingAuditRepository`, `FailingSubmissionRepository`). |
| C4 | Pass | clang-tidy `cert-err33-c` reports nothing. One deliberate exception: the result of `chmod` is ignored on purpose (a file system without permissions cannot be helped) and the code says why. |
| C5 | Pass | clang-tidy `bugprone-unchecked-optional-access`: 0 findings (8 fixed earlier). |
| C6 | Pass | SYS24 (wrong input), SYS53 (end of input), SYS54 (hostile text). |
| D1 | Pass | No raw `new`, `delete`, `malloc` or `free` in `src/` (search). |
| D2 | Pass | `Statement::~Statement` calls `sqlite3_finalize`; the `Database` constructor closes the connection on every failure path. |
| E1 | Pass | `ui` includes only `service` and `util`; `domain` only `domain`, `security`, `util`; no SQLite type outside `data` and `app`. `data` includes `service/IUnitOfWork.hpp` on purpose (dependency inversion). See observation R-09. |
| E2 | Pass | `tools/metrics.py`: nothing over the hard limits; 7 functions over a warning limit, each decided in `METRICS_REPORT.md`. |
| E3 | Pass | No repeated block of 7 or more lines (script over `src/`). |
| E4 | Finding R-03 | Date algorithm constants are the published civil-date algorithm and fixed text positions; the text length limits were scattered. Fixed. |
| E5 | Pass | clang-tidy `readability-identifier-naming` with the README rules: 0 findings. |
| E6 | Pass | Every declared function is referenced; the few referenced only from tests are trivial accessors. |
| E7 | Pass | Strategy (`IPenaltyPolicy`), Repository (`I*Repository`), Factory (`UserFactory`), Unit of Work (`IUnitOfWork`) are wired in `main.cpp` and used as designed. |
| F1 | Pass | 98.2% line coverage of the business-logic layer (target 70%). |
| F2 | Pass | The two new regression tests fail when their fix is removed and pass with it (section 3). DEF-01 and DEF-02 have regression tests; they were not removed-and-rerun again in this review. |
| F3 | Pass | Fixed clock and in-memory database; no test depends on the date it is run. |
| G1 | Finding R-05 | `PenaltyPolicies.hpp` had no description (the only one of 30 headers). Fixed. |
| G2 | Pass | SECURITY.md checked fact by fact against the code (password policy 8 to 128, five attempts, scrypt parameters, access control). Corrected after R-01 and R-04. |
| G3 | Pass | CI `build-test` and `static-analysis` were green on pull request #5 (4 of 4 checks). The run for the review fixes is shown by the pull request that carries them. |
| G4 | Pass | See observation R-10. |

## 3. Findings

| ID | Severity | Where | What was found | Action | Retest |
|---|---|---|---|---|---|
| R-01 (DEF-03) | Medium | `Database` constructor | The database file is readable by every user of the computer (`-rw-r--r--`) and holds the password hashes and all marks. | Owner-only permissions (0600) set on every open | `Database.FileIsReadableAndWritableOnlyByItsOwner` fails without the fix (0644) and passes with it; a real run creates `-rw-------` |
| R-02 (DEF-04) | Low | `MarkingService::mark` | The reason for changing a mark was length-checked only after an automatic suffix was added, against 600; a 590-character reason was refused with a message naming 600. | Reason checked first against 500, the same as the Head of Department's | `UC02_A1_Mark.TheReasonForChangingAMarkHasOneClearLimitOf500Characters` fails without the fix and passes with it |
| R-03 | Low | 8 files in `service` and `domain` | Text length limits written as bare numbers in many places (5000, 600, 500, 450, 100, 64, 128, 32, 20). | New `util/Limits.hpp`; every check uses the names | Full suite 207/207; clang-tidy 0 |
| R-04 | Low | README, `main.cpp` | The demo accounts have published passwords and nothing warned against using `--seed-demo` on a real installation. | Warning added to README and SECURITY.md | Documentation check |
| R-05 | Low | `PenaltyPolicies.hpp` | Header without a description. | Header comment added | Search: 30 of 30 headers now have one |

## 4. Observations (no change needed)

| ID | Observation | Why it is left |
|---|---|---|
| R-06 | `AuthService::changePassword` reloads the user inside its `catch (...)`. If that reload also failed, its exception would replace the original one. | Both would be database errors shown to the user as an error either way; the original cause is not lost for the user. |
| R-07 | `ConsoleApp::readSecret` restores terminal echo after reading, but not with RAII, so an exception between the two calls would leave echo off. | `readLine` throws nothing in this code. Candidate for a small guard object if the console is extended. |
| R-08 | `AuditService::history` has no permission check of its own. | The console reaches it only through `ModerationService::auditHistory`, which checks the Head of Department's department. |
| R-09 | The console holds a reference to `IUserRepository` to print a person's name in lists, which is a small step around the service layer. | Read-only and used for display only. A `displayNameOf` service method would remove it. |
| R-10 | Phases 1 to 5 were committed straight to `main`, before the branching strategy was adopted. | Pull requests with CI start from #1 (Phase 5); tags `v0.5.0` and later mark the releases. History is not rewritten. |

## 5. Verification after the fixes

| Check | Result |
|---|---|
| Build with warnings as errors | Clean |
| Tests | 207 of 207 pass (205 + 2 new regression tests) |
| Business-logic line coverage | 98.2% |
| clang-tidy | 0 findings |
| Metrics | No function over the hard limits |
| Real run | Program starts and exits normally; database file `-rw-------` |
| cppcheck | Runs in CI on the pull request (could not be installed in the development environment) |

## 6. Limits of this review

A person reviewing their own code sees what they expect to see. The checks above were chosen to be things a tool or a test
can confirm (searches, analysers, a mutation of the code, a real run) and not only a reading, but a second reviewer would
find other things. The review covers the production code; the test code was only checked for independence (F3).

## 7. Sign-off

| | |
|---|---|
| Reviewer | [Mafeta Katleho] |
| Date | 2026-11-03 |
| Decision | Accepted: all findings fixed and retested; observations recorded |
