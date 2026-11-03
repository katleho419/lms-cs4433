# Static Analysis Report

Date: 2026-11-03. Scope: all production code in `src/` (24 source files, about 4,500 lines). The test code in `tests/` was not analysed.

## 1. Tools and configuration

| Tool | Version | Configuration | Why |
|---|---|---|---|
| clang-tidy (includes the Clang Static Analyzer) | 18 | `.clang-tidy`: bugprone, cert, clang-analyzer, concurrency, misc, modernize, performance, portability and a selected set of readability and cppcoreguidelines checks | Finds bugs, unsafe patterns and complexity. Needs the compile commands, so it sees the code exactly as the compiler does. |
| cppcheck | version installed on the GitHub runner (Ubuntu 24.04) | `--enable=warning,style,performance,portability --std=c++17` | A second analyser with different checks. It found three things clang-tidy missed. |
| clang-format | 18 | `.clang-format` | Not an analyser. Applied once so the automatic fixes did not leave untidy code. |

Both analysers run in CI on every push and pull request (`static-analysis` job in `.github/workflows/ci.yml`). Any finding
fails the build. Locally: `tools/static_analysis.sh`.

Checks switched off on purpose (noise for this code base, not hidden defects): `bugprone-easily-swappable-parameters`
(flags every constructor taking several ints or strings), `cert-err58-cpp`, `misc-non-private-member-variables-in-classes`
(plain data structs), `misc-include-cleaner`, `modernize-use-trailing-return-type`, `modernize-use-nodiscard`.
No inline suppressions (`NOLINT`) were used anywhere.

## 2. Results

| Run | clang-tidy | cppcheck |
|---|---:|---:|
| Before (baseline) | 221 | 3 (first CI run) |
| After | 0 | 0 (CI run of the fix, green) |

Evidence: `docs/static_analysis/clang-tidy_before.txt` (raw baseline output), `clang-tidy_after.txt`, the green CI runs of
pull requests #2 and #3, and the cppcheck output from the first CI run (`cppcheck.txt` artifact).

### clang-tidy findings before the fixes, by check

| Check | Count | What it means | Action |
|---|---:|---|---|
| readability-braces-around-statements | 153 | `if (x) return;` without braces, a classic source of bugs when a line is added later | Fixed automatically |
| misc-const-correctness | 13 | Variables that are never changed but not declared `const` | Fixed automatically |
| readability-isolate-declaration | 10 | Several variables declared on one line | Fixed automatically |
| modernize-return-braced-init-list | 8 | Repeated return type | Fixed automatically |
| bugprone-unchecked-optional-access | 8 | Reading a `std::optional` without checking it holds a value | Fixed by hand (see below) |
| readability-function-cognitive-complexity | 6 | Menu functions with complexity 32 to 79 against a limit of 25 | Refactored (see below) |
| bugprone-implicit-widening-of-multiplication-result | 5 | An `int` multiplication whose result is stored in a 64-bit type | Made the arithmetic 64-bit explicitly |
| readability-implicit-bool-conversion | 3 | `std::isalnum` and `isspace` return int, used as bool | Added `!= 0` |
| performance-enum-size | 3 | Enums using 4 bytes where 1 is enough | Gave the enums an 8-bit type |
| modernize-use-auto | 3 | Type written twice | Fixed automatically |
| modernize-avoid-c-arrays | 3 | `char buffer[32]` | Replaced with `std::array` |
| cert-err33-c | 2 | The return value of `snprintf` ignored | Result now used |
| bugprone-empty-catch | 1 | Empty `catch` block hides an error | Now returns a fallback explicitly |
| bugprone-branch-clone | 1 | Two identical `switch` branches | Merged |
| bugprone-optional-value-conversion | 1 | Optional unwrapped and wrapped again | Passed on directly |
| readability-simplify-boolean-expr | 1 | Condition that can be simplified | Simplified |
| **Total** | **221** | | |

### cppcheck findings (found by the first CI run)

| Location | Finding | Fix |
|---|---|---|
| `src/app/main.cpp` | Parameter `submissions` of `seedDemo` never used (`constParameterReference`) | Removed the parameter |
| `src/app/main.cpp` | Variable `hod` assigned and never read (`unreadVariable`) | The user is still created; the unused variable is gone |
| `src/service/ModerationService.cpp` | Hand-written search loop in `approveReviewed` (`useStlAlgorithm`) | Replaced by `std::any_of` |

## 3. Triage: real findings against noise

Counted honestly, the findings fall into three groups.

**Worth fixing for correctness or safety (16).** Eight unchecked optional reads: five in the service layer (the value is
always set when a transaction commits, but nothing said so; a new helper, `takeResult`, now checks and throws a clear error
otherwise) and three in the console screens, which called `getRawMark()` or `getFinalMark()` twice on a temporary and read
the second result without checking it. Also the empty `catch` in `ConsoleApp::nameOf`, the two ignored `snprintf` results,
the three int-to-bool conversions, the optional that was unwrapped and wrapped again, and the duplicated `switch` branch.

**Maintainability, and the most valuable change in this report.** `adminMenu` (complexity 69) and `hodMenu` (79) were each
one function with every action written inline inside a lambda. They are now one shared `runMenu` loop, one short dispatcher
per role (a single `switch`) and one small method per action (`hodApproveAll`, `lecturerMark`, and so on). All screen text
and behaviour are unchanged.

**False alarms.** The two "implicit widening" findings in `main.cpp` and `PasswordHasher.cpp` multiply small constants such as
`7 * 24 * 3600`, which cannot overflow an `int`. They were changed anyway (`7LL`, `2ULL`) because that costs nothing and
silences the check without a suppression. The 153 brace findings are style, not defects, but they are cheap to fix and
prevent a known class of mistake.

## 4. What static analysis did not find

The two real defects of this project, DEF-01 (students could receive unreleased marks from the service layer) and DEF-02
(stale approval not detected), were found by the integration tests, not by either analyser. Both are errors against the
requirements, and a tool that does not know the requirements cannot see them. The two techniques are complementary:
analysers catch coding mistakes, tests catch wrong behaviour.

## 5. Verification after the changes

| Check | Result |
|---|---|
| Build with warnings as errors | Clean |
| All tests (unit, integration, system, acceptance) | 205 of 205 pass, including 46 system tests that drive the refactored menus |
| Business-logic line coverage | 98.2% (98.4% before; the drop is the one new safety check in `takeResult` that normal use cannot reach) |
| Real program, scripted session | Starts, saves, exits with code 0; demo accounts work |
| clang-tidy on `src/` | 0 findings |
| cppcheck on `src/` | 0 findings (CI) |

## 6. Changes, in Git

| Pull request | Commits | Purpose |
|---|---|---|
| #2 `chore/static-analysis-findings` | `5cd2d71` style: apply clang-format; fix clang-tidy findings in src and split console menu functions. `4a8f421` chore: add clang-tidy config, static analysis script and CI job, refresh coverage report | clang-tidy findings and the CI job |
| #3 `chore/cppcheck-findings` | `f77c377` fix: resolve cppcheck findings and make cppcheck a blocking CI check | cppcheck findings |

## 7. Limits

- Tests (`tests/`) were not analysed. Only production code is in scope.
- A clean result means these two tools found nothing with the chosen checks. It is not a proof of absence of defects.
- The first CI run of cppcheck was the first time it ran on this code; it could not be installed in the development environment.
