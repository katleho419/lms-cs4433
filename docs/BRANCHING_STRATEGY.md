# Git Branching Strategy

## Decision

The project uses **GitHub Flow with release tags**: one permanent branch (`main`) and short-lived topic branches that are merged
back through a pull request once CI is green.

## Why this and not the alternatives

| Option | Why it was not chosen (or chosen) |
|---|---|
| Git Flow (`main`, `develop`, `release/*`, `hotfix/*`) | Built for teams that ship several supported versions at once. For one developer and one release line it adds branches that carry no information. |
| Trunk-based (commit straight to `main`) | Fast, but nothing stops a broken commit reaching `main`, and the history shows no unit of work. This is how Phases 1 to 5 were committed; it is replaced from Phase 6 on. |
| **GitHub Flow** | Small, easy to follow alone, and it matches the CI already in place (`ci.yml` runs on every push and pull request). Every change gets a branch, a CI run and a recorded review before it reaches `main`. |

The rest of the course work also fits: a change request such as CR1 in Phase 8 is one branch, one pull request and one tag.

## Rules

1. `main` always builds and passes all tests. It is the only long-lived branch. Nothing is committed to it directly.
2. Every change starts as a branch from an up-to-date `main`. Branches live for hours or a few days, not weeks.
3. A branch is merged only through a pull request, after the CI run is green and the self code review checklist
   (`docs/CODE_REVIEW_CHECKLIST.md`) is filled in on the pull request.
4. Merge with a **merge commit** (GitHub "Create a merge commit"), not squash, so the small commits and the branch stay visible in
   the history.
5. Delete the branch after merging.
6. Releases are marked with an annotated tag on `main` (see below). A tag is never moved or deleted.
7. A defect found after a release is fixed on a `fix/` branch the same way and gets the next patch tag.

## Branch names

| Prefix | Use | Example |
|---|---|---|
| `feature/` | New behaviour, including change requests | `feature/cr1-class-extension` |
| `fix/` | Defect repair | `fix/stale-approval-check` |
| `test/` | Tests only | `test/system-acceptance` |
| `docs/` | Documents only | `docs/user-manual` |
| `chore/` | Build, CI, tooling, static analysis fixes | `chore/static-analysis-findings` |

Lower case, words joined by hyphens, a short description of the purpose.

## Commit messages

Imperative mood, a short first line of at most 72 characters, a blank line, then the reason if it is not obvious.
Start with a type, in the style of Conventional Commits:

    fix: refuse approval when the mark changed after review

    UC-03 E4: the Head of Department could approve a mark that the lecturer
    had changed since the review. Adds reviewedFinalMark to approve().
    Fixes DEF-02.

Types: `feat`, `fix`, `test`, `docs`, `refactor`, `chore`. One logical change per commit; do not mix a fix with unrelated
formatting. Refer to defect IDs (DEF-xx), use cases (UC-xx) and change requests (CR-x) where they apply.

## Tags and versions

Semantic versioning, `vMAJOR.MINOR.PATCH`, with annotated tags (`git tag -a`).

| Tag | Meaning | Phase |
|---|---|---|
| `v0.4.0` | Implementation complete | 4 |
| `v0.5.0` | Unit, integration, system and acceptance tests passing (205 tests, 98.4% coverage) | 5 |
| `v0.6.0` | Static analysis findings fixed, metrics and review record added | 6 |
| `v1.0.0` | Release: installation guide, user manuals and release notes | 7 |
| `v1.1.0` | CR1 (extensions for a whole class) | 8 |

## Day-to-day commands

    git switch main && git pull
    git switch -c chore/static-analysis-findings
    # edit, then commit in small steps
    git add -p
    git commit -m "chore: fix cppcheck warnings in PasswordHasher"
    git push -u origin chore/static-analysis-findings
    # open the pull request on GitHub, wait for the green CI run, tick the checklist, merge, then:
    git switch main && git pull
    git branch -d chore/static-analysis-findings

## Branch protection (GitHub settings)

Settings, Branches, add a rule for `main`: require a pull request before merging, and require the status check
"build-test" to pass. This makes rules 1 and 3 enforced and not only agreed.
