# Release Notes: version 1.0.0

Moderated Learning Management System (CS4433 Software Engineering project), National University of Lesotho.
Release date: 2026-11-03. Tag: `v1.0.0`. Previous tags: `v0.5.0` (verification complete), `v0.6.0` (configuration management and quality assurance).

## What this version does

A console program for a school or department to run moderated assignment marking.

| Role | Can do |
|---|---|
| Student | See assignments and released marks; submit work, and resubmit before the deadline |
| Lecturer | Create assignments, grant extensions, view submissions, mark with feedback (late penalty applied automatically) |
| Head of Department | Review, adjust, approve and return marks; release marks to students; read the audit history |
| Administrator | Create departments, courses and users; assign lecturers; enrol students |

Business rules in force: marks are hidden from students until released; every mark is approved by the Head of Department
before release; late work loses 10% of the maximum mark per started day for up to 3 days and scores 0 after that; every
change of a mark is written to a permanent audit log; passwords are stored salted and hashed (scrypt).

## Changes since v0.5.0

| Area | Change |
|---|---|
| Quality | Static analysis clean: clang-tidy 221 findings to 0; cppcheck 3 to 0; both now checked on every pull request |
| Maintainability | The three largest console functions split into small ones (highest complexity 35 to 14); no function above the hard limits |
| Security (DEF-03) | The database file is now created readable by its owner only (mode 0600), and an older file is tightened when opened |
| Correctness (DEF-04) | The reason for changing a mark is checked once, against one 500-character limit |
| Code | All text length limits in one header (`src/util/Limits.hpp`) |
| Tests | 207 tests, all passing; 98.2% line coverage of the business-logic layer; 46 system and acceptance tests |
| Documents | Branching strategy, static analysis report, metrics report, code review checklist and record, deployment diagram, installation guide, user manual |

Defects found over the project (see `DEFECT_LOG.md`): 4 software defects (2 High, 1 Medium, 1 Low), all fixed and
covered by regression tests; no open defects.

## Known limitations

| # | Limitation | Effect |
|---|---|---|
| 1 | An extension granted by a lecturer is applied when the student submits, but the student's assignment list still shows the original deadline. This is **change request CR1**, deliberately left for Phase 8. | A student may think they are late when they are not; the lecturer should tell the student. |
| 2 | No password reset. | The administrator creates a new account for a user who forgot the password. |
| 3 | The data file is not encrypted. | Protect the computer and the backups; the file is owner-only on Linux and macOS. |
| 4 | Times are shown and typed in Lesotho time (UTC+2) only. | Not suitable for other time zones without a change. |
| 5 | One computer, one data file, console interface. No web or network access. | Users sit at the school computer (or log in to it by SSH). |
| 6 | Five wrong passwords close the program, but there is no lock that lasts across restarts. | Not protection against a determined guesser; see `SECURITY.md`. |
| 7 | Tested on Ubuntu 24.04 only (GCC 13, CMake 3.28, OpenSSL 3.0, SQLite 3.45). Not tested on Windows or macOS. | Build instructions for other systems are in `BUILD.md` but unverified. |
| 8 | cppcheck could not be installed in the development environment; it runs on GitHub (Ubuntu runner). | Its results are the pull request checks. |

## Upgrade notes

This is the first release meant for use. There is nothing to migrate from earlier tags, which were development versions
with demo data only. Database format version: 1.

## Installing

See `INSTALLATION_GUIDE.md` (technician) and `USER_MANUAL.md` (each role). Never use `--seed-demo` on a real installation.

## Documents in this release

| Document | Purpose |
|---|---|
| `INSTALLATION_GUIDE.md`, `diagrams/deployment.*` | Install, back up, upgrade; deployment diagram |
| `USER_MANUAL.md` | Instructions for students, lecturers, heads of department, administrators |
| `BUILD.md`, `DATABASE.md`, `SECURITY.md` | Developer build, data and security notes |
| `TEST_STRATEGY.md`, `TEST_PLAN_SYSTEM_ACCEPTANCE.md`, `TEST_RESULTS.md`, `COVERAGE_REPORT.md`, `DEFECT_LOG.md` | Verification and validation |
| `BRANCHING_STRATEGY.md`, `STATIC_ANALYSIS_REPORT.md`, `METRICS_REPORT.md`, `CODE_REVIEW_CHECKLIST.md`, `CODE_REVIEW_RECORD.md` | Configuration management and quality assurance |
