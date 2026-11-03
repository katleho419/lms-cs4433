# Test Results: Unit, Integration, System and Acceptance Tests

Executed 2026-11-02 on Ubuntu 24.04, g++ 13.3, CMake 3.28, GoogleTest 1.14, SQLite 3.45.1, OpenSSL 3.0.13.
Build: `-DLMS_WARNINGS_AS_ERRORS=ON -DLMS_BUILD_TESTS=ON -DLMS_ENABLE_COVERAGE=ON`.

## How to run

    cmake -S . -B build -G Ninja -DLMS_BUILD_TESTS=ON -DLMS_ENABLE_COVERAGE=ON
    cmake --build build
    ctest --test-dir build --output-on-failure
    python3 tools/coverage.py build        # prints the coverage table, exit code 1 if business logic < 70%

GoogleTest is taken from the system (`sudo apt install libgtest-dev`) or downloaded by CMake. The tests use an in-memory
SQLite database and a fixed clock, so they need no files, network or real time.

## Results

| Run | Tests | Passed | Failed | Notes |
|---|---:|---:|---:|---|
| First full run | 151 | 149 | 2 | One software defect (DEF-01) and one error in a test script (TE-01) |
| After fixing DEF-01, DEF-02 and TE-01, with regression tests added | 159 | 159 | 0 | Unit and integration only, about 7.5 s |
| Unit, integration, system and acceptance, first run | 204 | 204 | 0 | 45 new system and acceptance tests; no new software defect |
| After adding the missing boundary test SYS22b (TG-01) | 205 | 205 | 0 | Final run, about 73 s (scrypt hashing in the system tests) |

| Level | Test files | Tests |
|---|---|---:|
| Unit | test_util, test_security, test_domain | 59 |
| Integration | test_data, test_services | 100 |
| System (SYS-01..SYS-57) | test_system | 41 |
| Acceptance (ACC-01..ACC-05) | test_system | 5 |
| Total | | 205 |

Unit tests run with no database. Integration tests run the services against real SQLite repositories, check the schema
triggers and constraints, and use stub repositories that throw to prove that transactions roll back.

## Coverage (line coverage, gcov)

| Layer | Lines | Covered | Coverage |
|---|---:|---:|---:|
| util | 110 | 110 | 100.0% |
| security | 91 | 84 | 92.3% |
| domain | 276 | 271 | 98.2% |
| service | 412 | 410 | 99.5% |
| data | 466 | 446 | 95.7% |
| **Business logic (util, security, domain, service)** | **889** | **875** | **98.4%** |
| All measured source | 1,355 | 1,321 | 97.5% |

Target in the brief: at least 70% line coverage of the business-logic layer. **Result: 98.4%, target met.**
Per-file figures are in `COVERAGE_REPORT.md`. The console UI (`src/ui`) and `main.cpp` are not part of this figure; they are
covered by the system and acceptance tests (they run the real `ConsoleApp`).

Lines not covered (14 of 889 in the business-logic layer) are code that normal use cannot reach: the `throw` or `return`
that follows an exhaustive `switch` on an enum (Role, SubmissionStatus, UserFactory, CourseService), and the error paths for
a failed random-number or scrypt call inside OpenSSL. Header-only code that runs only from the test files themselves
(parts of `PasswordPolicy.hpp`) is not instrumented, so its figure is understated. A missing test for acting on an older
version of a submission (ModerationService) was found this way and added.

The figures were cross-checked with plain `gcov` for two files (ModerationService.cpp 99.28% of 138 lines before the extra test,
SubmissionService.cpp 100% of 51 lines), which agree with `tools/coverage.py`.

## Continuous integration

`.github/workflows/ci.yml` builds with warnings as errors, runs all tests with `ctest`, runs `tools/coverage.py`
(the step fails if business-logic coverage drops below 70%) and uploads the coverage report, on every push and pull request.
Evidence (a green run) comes from the GitHub Actions tab once the workflow has been pushed.

## System and acceptance tests

Plan and traceability to each use case flow: `TEST_PLAN_SYSTEM_ACCEPTANCE.md`.

| Group | Tests | Passed |
|---|---:|---:|
| UC-01 Submit assignment (main, A1-A3, E1-E5, size limit) | 11 | 11 |
| UC-02 Mark submission (main, A1-A3, E1-E5, role separation) | 10 | 10 |
| UC-03 Moderate marks (main, A1-A4, E1-E5, approve-all) | 12 | 12 |
| Login, access control, administration, passwords | 8 | 8 |
| Acceptance scenarios, one per role plus restart | 5 | 5 |
| **Total** | **46** | **46** |

Real executable: a scripted run of `build/lms --seed-demo` (student logs in and submits, exit code 0), then a second process
on the same file (lecturer sees the stored submission) passed.

**Check that the tests can fail.** To see whether the system tests would notice a fault, one fault was introduced on purpose
(late window changed so that work exactly 3 days late scored zero). All 45 system tests still passed, which showed a gap:
no system test marked work at the 3-day boundary (the unit tests did). Test SYS22b was added; it fails with the fault and
passes without it. The fault was then removed. Recorded as TG-01 in `DEFECT_LOG.md`.

## Update after the Phase 6 code review

Two regression tests were added for the review findings DEF-03 (database file permissions) and DEF-04 (limit on the reason for a
mark change). Both were checked the same way as in the system test stage: the fix was removed on purpose, the new test failed,
the fix was put back, the test passed.

| Run | Tests | Passed | Failed |
|---|---:|---:|---:|
| Final run after the code review fixes | 207 | 207 | 0 |

Business-logic line coverage: 98.2%. clang-tidy: 0 findings. Metrics: no function over the hard limits.

## Update for version 1.1.0 (change request CR1)

14 tests were added for the class-wide extension and the deadline shown to students (see `CHANGE_REQUEST_CR1.md`).

| Group | New tests |
|---|---:|
| Service and integration (`CR1_ClassExtension`, `CR1_AssignmentList`) | 10 |
| System (SYS58 to SYS61) | 4 |
| **New** | **14** |

| Run | Tests | Passed | Failed |
|---|---:|---:|---:|
| Before the change (version 1.0.0) | 207 | 207 | 0 |
| After the change, with a stand-in test runner in the development environment | 221 | 221 | 0 |
| After the change, GoogleTest in CI (`build-test` on the pull request) | 221 | see the pull request | |

The stand-in runner was used because GoogleTest could not be downloaded in the development environment; it is not part of the
project. The CI run with the real framework is the authoritative result. Each new rule was also removed on purpose to check
that a test fails (four checks, listed in `CHANGE_REQUEST_CR1.md`).

Business-logic line coverage: 98.6%. clang-tidy: 0 findings. Metrics: no function over the hard limits.
