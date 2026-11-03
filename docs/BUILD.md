# Build and Run Guide

## Requirements
| Tool | Version | Ubuntu/Debian package |
|---|---|---|
| C++17 compiler | g++ 9+, clang 10+, MSVC 2019+ | build-essential |
| CMake | 3.16+ | cmake |
| SQLite3 dev files | 3.31+ | libsqlite3-dev |
| OpenSSL dev files | 3.0+ | libssl-dev |

Windows: install vcpkg, then `vcpkg install sqlite3 openssl` and add
`-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake` to the configure step.
macOS: `brew install cmake sqlite openssl@3` and add `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)`.

## Build
    cmake -S . -B build -DLMS_WARNINGS_AS_ERRORS=ON
    cmake --build build

CMake options:
- `LMS_WARNINGS_AS_ERRORS` (OFF) treat warnings as errors.
- `LMS_ENABLE_TEST_CLOCK` (OFF) lets the `LMS_FAKE_NOW` environment variable (e.g. `2026-11-08T10:00:00Z`)
  set the clock, so late-submission rules can be demonstrated. Testing only; never use in a release build.
- `LMS_BUILD_TESTS` (OFF) builds the GoogleTest suite (`tests/`). Needs `libgtest-dev` or network access.
- `LMS_ENABLE_COVERAGE` (OFF) adds gcov instrumentation (GCC or Clang).

## Run
    ./build/lms --seed-demo     first run only: creates lms.db with demo accounts
    ./build/lms                 later runs (asks for a first administrator if the database is empty)
    ./build/lms --db path.db    use another database file

Demo logins: admin/Admin12345, hod/Hod123456, lecturer/Lect123456, student1/Stud123456, student2/Stud123456.

## Troubleshooting
| Message | Cause and fix |
|---|---|
| CMake: Could not find SQLite3 / OpenSSL | install the dev packages above |
| "Cannot open the database file" | folder does not exist or is read-only; use `--db` with a writable path |
| "created by a different version of this program" | file's `PRAGMA user_version` is not 1; delete it or use another file |
| "database is locked" | another program has the file open; waits 5 s then fails |

## Run the tests
    sudo apt install libgtest-dev          # or let CMake download GoogleTest
    cmake -S . -B build -G Ninja -DLMS_BUILD_TESTS=ON -DLMS_ENABLE_COVERAGE=ON
    cmake --build build
    ctest --test-dir build --output-on-failure
    python3 tools/coverage.py build        # line coverage per layer (needs gcov)

See `docs/TEST_RESULTS.md`, `docs/TEST_STRATEGY.md` and `docs/DEFECT_LOG.md`.
