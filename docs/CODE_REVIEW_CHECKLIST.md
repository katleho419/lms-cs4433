# Code Review Checklist

Used for every pull request and for the full self-review of the code base in Phase 6. Each item is answered Pass, Finding
(a problem was found and goes into the record) or N/A, and the answer must say how it was checked. A tick without a
way of checking it is not accepted.

## A. Correctness and requirements
| ID | Question | How to check |
|---|---|---|
| A1 | Does every business rule (BR-xx) and use case flow (UC-xx) have at least one test that would fail if the rule were broken? | Traceability tables in `TEST_STRATEGY.md` and `TEST_PLAN_SYSTEM_ACCEPTANCE.md`; remove a rule on purpose and run the tests |
| A2 | Are all exception flows of the use cases handled with a clear message and no partial change? | System tests SYS05 to SYS41; rollback tests |
| A3 | Are boundaries tested (limits, deadlines, penalty days, text lengths)? | Boundary value tests; read the limits in `util/Limits.hpp` against the tests |
| A4 | Can a submission change state only through the state machine? | Search for assignments to the status outside `Submission::transitionTo` |

## B. Security
| ID | Question | How to check |
|---|---|---|
| B1 | Is every SQL statement prepared with bound values, with no user text in the SQL string? | Search for string joins next to SELECT, INSERT, UPDATE, DELETE; SQL injection tests |
| B2 | Are passwords salted and hashed with a recognised algorithm, compared in constant time, never stored or logged? | Read `PasswordHasher`; search for password text in output and log calls |
| B3 | Are there any secrets, keys or real credentials in the source or the repository? | Search for password, secret, token, key assignments; list what is found |
| B4 | Does every use case operation check the role and the relationship (own course, own department, enrolled)? | List every service function and its permission check |
| B5 | Does a failed login reveal nothing about which part was wrong, including by response time? | Read `AuthService::login`; system tests SYS50 and SYS51 |
| B6 | Is every input validated and limited before use? | Read each service entry point; text length limits in one place |
| B7 | Are files created with the least permission needed? | Run the program and list the permissions of the database file |
| B8 | Are unsafe C functions avoided (strcpy, sprintf, gets, system, rand, atoi)? | Search the source |
| B9 | Is the password hidden while typed? | Read `ConsoleApp::readSecret` |

## C. Error handling and robustness
| ID | Question | How to check |
|---|---|---|
| C1 | Are there empty catch blocks that hide errors? | Search for catch; clang-tidy `bugprone-empty-catch` |
| C2 | Does every catch-all (`catch (...)`) clean up and rethrow? | Read each one |
| C3 | Are multi-step changes all-or-nothing? | Use of the unit of work; rollback tests with failing repositories |
| C4 | Are return values of library and system calls checked or deliberately ignored with a reason? | clang-tidy `cert-err33-c` |
| C5 | Is every optional value checked before it is read? | clang-tidy `bugprone-unchecked-optional-access` |
| C6 | Does the program survive end of input, wrong input and unexpected exceptions without a crash? | System tests SYS24, SYS53, SYS54 |

## D. Resources
| ID | Question | How to check |
|---|---|---|
| D1 | Is every resource owned by an object that releases it (RAII), with no raw new/delete/malloc/free? | Search the source |
| D2 | Are database statements and connections closed on every path, including errors? | Read `Statement` and `Database` destructors and the constructor's error path |

## E. Design and maintainability
| ID | Question | How to check |
|---|---|---|
| E1 | Do the layers depend only on the layers below them (ui, service, domain/data, util)? | List the includes of each layer; search for SQLite types outside the data layer |
| E2 | Is every function within the limits (complexity 10 warning, 15 fail; 50 and 100 lines)? | `tools/metrics.py` |
| E3 | Is there duplicated code? | Search for repeated blocks of 7 or more lines |
| E4 | Are there unexplained numbers in the code? | Search for numeric literals outside constants |
| E5 | Do names follow the coding standard in the README? | clang-tidy `readability-identifier-naming` with the README rules |
| E6 | Is there dead code or an unused function? | Count references of every declared function |
| E7 | Are the design patterns used as designed (Strategy, Repository, Factory, Unit of Work)? | Read the composition root and the services |

## F. Tests
| ID | Question | How to check |
|---|---|---|
| F1 | Is business-logic line coverage at least 70%? | `tools/coverage.py` |
| F2 | Does every defect fix have a regression test that fails without the fix? | Remove the fix on purpose and run the test |
| F3 | Are tests independent of the real clock and of each other? | Fixed clock, in-memory database |

## G. Documentation and build
| ID | Question | How to check |
|---|---|---|
| G1 | Does every header say what it is for? | Check the first lines of each header |
| G2 | Do README, BUILD, SECURITY and DATABASE agree with the code? | Compare each stated fact with the source |
| G3 | Does the build pass with warnings as errors, and do the analysers report nothing? | CI: `build-test` and `static-analysis` |
| G4 | Does the change appear in the right place in the history (branch, small commits, clear messages)? | `git log` against `BRANCHING_STRATEGY.md` |

## Severity of findings

Same scale as the defect log: Critical, High, Medium, Low. An Observation is something noticed that needs no change; it is
recorded with the reason for leaving it.
