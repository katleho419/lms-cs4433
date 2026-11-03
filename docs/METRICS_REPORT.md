# Metrics Report

Date: 2026-11-03. Scope: production code in `src/` (53 files). Test code is measured for size only.
Full generated tables: `docs/metrics/metrics_after.md` (current code) and `docs/metrics/metrics_before.md` (code at `v0.5.0`).

## 1. Method

Both metrics come from `tools/metrics.py`, a small script with no dependencies. It runs in CI (`static-analysis` job)
and locally with `python3 tools/metrics.py`.

| Metric | Definition |
|---|---|
| Code lines (LOC) | Lines that contain code. Blank lines and lines holding only a comment are counted separately. |
| NLOC of a function | Code lines from the function name to its closing brace. |
| Cyclomatic complexity (CC) | 1 plus the number of decision points in the function: `if`, `for`, `while`, `case`, `catch`, `&&`, `\|\|` and `?:`. This is McCabe's measure, with the counting rule that the `lizard` tool uses by default. A lambda counts towards the function that contains it. |

Limits used in this project:

| | Warning | Fail (CI stops) |
|---|---:|---:|
| Cyclomatic complexity | above 10 | above 15 |
| NLOC | above 50 | above 100 |

A complexity of 10 is the figure McCabe himself proposed as a sensible upper limit; 15 is this project's hard ceiling.

**How the numbers were checked.** The script's output was verified by hand on two functions. `PasswordHasher::verify` (before
the change) has 16 decision points (6 `if` and 10 `||`), so CC 17, which is what the script reported. `Submission::transitionTo`
has 5 `case`, 5 `||`/`&&` inside the rules, 1 `||` in the `Moderated` rule and 1 `if`, plus the base 1: CC 13, as reported.
The `switch` dispatchers also match (`hodAction` has 11 cases, so CC 12). The script was not compared with `lizard` itself,
which could not be installed in the development environment (no access to the package index); `pip install lizard && lizard src`
on another machine gives an independent cross-check, and small differences in how a tool counts are normal.

## 2. Size

| Layer | Files | Code lines | Comment lines | Functions |
|---|---:|---:|---:|---:|
| app (composition root) | 1 | 102 | 2 | 3 |
| data (SQLite, repositories) | 10 | 852 | 32 | 80 |
| domain (entities, rules, penalty policies) | 12 | 617 | 52 | 91 |
| security (hashing, password policy) | 3 | 199 | 17 | 13 |
| service (use cases) | 19 | 858 | 70 | 53 |
| ui (console) | 2 | 766 | 8 | 47 |
| util | 6 | 267 | 25 | 28 |
| **Total** | **53** | **3,661** | **206** | **315** |

Test code: 7 files, 1,997 code lines, 217 functions. Test code is 0.55 of the size of the production code.

The production code grew from 3,173 to 3,661 code lines between `v0.5.0` and now. Almost all of that is formatting: the
static analysis clean-up put braces around 153 single-line `if` and loop bodies (each brace on a line of its own) and
split long lines. No feature was added.

The layers are balanced: the domain and service layers (the business rules) hold 40% of the code, the console interface
holds 21%. The `ui` layer has the most code per file because the menus live there.

## 3. Complexity

| | Before (`v0.5.0`) | Now |
|---|---:|---:|
| Functions measured | 286 | 315 |
| Average CC | 2.64 | 2.47 |
| **Maximum CC** | **35** | **14** |
| Functions with CC 1 to 5 | 258 (90.2%) | 283 (89.8%) |
| Functions with CC 6 to 10 | 19 (6.6%) | 26 (8.3%) |
| Functions with CC 11 to 15 | 5 (1.7%) | 6 (1.9%) |
| **Functions with CC above 15** | **4 (1.4%)** | **0** |
| Longest function (NLOC) | 105 (`hodMenu`) | 62 (`main`) |

Nine in ten functions are simple (CC 5 or less). The average of 2.5 is low, which is expected from small repository and
accessor methods and a service layer written as short use case steps.

### What changed, and why

Before the clean-up, four functions were above the hard limit:

| Function | CC before | Action | CC now |
|---|---:|---|---:|
| `ConsoleApp::hodMenu` | 35 (105 lines) | Split into a dispatcher and one method per action | 12 (`hodAction`) |
| `ConsoleApp::adminMenu` | 35 (77 lines) | Same | 10 (`adminAction`) |
| `ConsoleApp::lecturerMenu` | 26 (66 lines) | Same | 9 (`lecturerAction`) |
| `PasswordHasher::verify` | 17 | Split into `parseStoredHash` (parse and validate the stored hash), `costIsAcceptable` and a short `verify` | 3, 10 and 7 |

`studentMenu` (15) also fell, to 5. All changes keep behaviour: the 205 tests, including the 46 system tests that drive
the menus and the password tests that feed malformed hashes to `verify`, pass unchanged, and business-logic line coverage
is 98.2%.

### Functions still above a warning limit (7)

None is above the hard limit. Each was looked at and the decision is recorded here.

| Function | Value | Decision |
|---|---|---|
| `ConsoleApp::run` | CC 14, NLOC 57 | Accepted for now. The login loop with three kinds of failure and the five-attempt lockout. Covered by system tests SYS50 to SYS53. The first candidate for splitting (`attemptLogin`) if the interface is extended. |
| `Submission::transitionTo` | CC 13 | Accepted. It holds the state machine of the specification in one place, one `case` per state and one condition per allowed move. Splitting would scatter the rules. All 25 state pairs are tested. |
| `buildEpoch` | CC 13 | Accepted. Range checks for month, day, hour, minute and second; covered by boundary value tests. |
| `ConsoleApp::hodAction` | CC 12 | Accepted. A `switch` over 11 menu options with no logic in the branches. Its complexity is the number of menu options. |
| `ConsoleApp::adminCreateUser` | CC 12 | Accepted. The prompts depend on the role chosen (student number, department). Covered by system tests SYS55 and SYS56 and acceptance test ACC04. |
| `MarkingService::mark` | CC 11 | Accepted. The chain of rules of UC-02 (permission, status, range, reason for re-marking, penalty). One above the warning limit; covered by the UC-02 tests for E1 to E5. |
| `main` | NLOC 62 (CC 10) | Accepted. The composition root creates and connects every object. Long but straight-line code. |

## 4. Reading the results

- **Complexity and defects.** The two defects found in testing sat in simple functions: DEF-01 in
  `SubmissionService::assignmentsFor` (CC 4) and DEF-02 in `ModerationService::approve` (CC 4). They were missing rules, not
  tangled code, which fits the finding in the static analysis report that metrics and analysers do not find wrong behaviour.
- **Where the risk sits.** The functions with the highest complexity are either validation chains (`buildEpoch`, `mark`) or
  rule tables (`transitionTo`, the menu dispatchers). Both kinds are fully covered by tests.
- **Maintainability.** The largest improvement of this phase is in the console layer: the three biggest functions of the
  project are now many short ones. Adding a menu option (for example for change request CR1) now means adding one method
  and one `case`.

## 5. Limits of these figures

- Cyclomatic complexity is a rough indicator of how many paths there are to test, not a quality score. A `switch` with many
  trivial cases scores high without being hard to read.
- The script finds functions by matching braces after the formatter has laid out the code. It is reliable for this code
  base, but it is not a C++ parser and could miscount unusual constructs.
- Lines of code depend on the formatting style; the figures before and after are comparable for complexity but not for size.

## 6. Update for version 1.1.0 (change request CR1)

Full table: `docs/metrics/metrics_v1.1.0.md` (generated by `tools/metrics.py`).

| | Version 1.0.0 | Version 1.1.0 |
|---|---:|---:|
| Production files | 54 | 54 |
| Production code lines | 3,706 | 3,760 |
| Functions measured | 316 | 317 |
| Average CC | 2.48 | 2.50 |
| Maximum CC | 14 | 14 |
| Functions above the hard limits | 0 | 0 |
| Test code lines | 2,031 | 2,191 |

The first tables of this report (53 files, 3,661 lines) are the snapshot taken when the metrics were introduced; `util/Limits.hpp`
was added by the Phase 6 code review (54 files, 3,706 lines at version 1.0.0). Correction to section 3: after the review fix
for DEF-04, `MarkingService::mark` has complexity 12, not 11 (one more check, for the length of the reason). It is still above the
warning limit and below the hard limit, and the decision in the table stands.

Functions touched by CR1:

| Function | Complexity before | Complexity now |
|---|---:|---:|
| `AssignmentService::grantClassExtension` (new) | - | 6 |
| `ConsoleApp::lecturerGrantExtension` | 5 | 7 |
| `ConsoleApp::studentShowAssignments` | 6 | 7 |
| `SubmissionService::assignmentsFor` | 4 | 4 |

No function crossed a limit.
