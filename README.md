# Moderated LMS (CS4433 project, C++17)

## Build
Requires: C++17 compiler, CMake 3.16+, SQLite3 and OpenSSL 3 development files.
Ubuntu/Debian: `sudo apt install build-essential cmake libsqlite3-dev libssl-dev`

    cmake -S . -B build -DLMS_WARNINGS_AS_ERRORS=ON
    cmake --build build

## Run
    ./build/lms --seed-demo      # first run only: creates lms.db with demo accounts
    ./build/lms                  # later runs
    ./build/lms --db other.db

Demo logins: admin/Admin12345, hod/Hod123456, lecturer/Lect123456, student1/Stud123456.
Without --seed-demo, the program asks you to create the first administrator.

WARNING: the demo accounts have passwords that are published here. Use --seed-demo only for testing and demonstrations,
never on a real installation. The database file is created readable by its owner only.

## Installing and using
- Technician: `docs/INSTALLATION_GUIDE.md` (with the deployment diagram `docs/diagrams/deployment.png`)
- Users, by role: `docs/USER_MANUAL.md`
- What is in this version and what is not: `docs/RELEASE_NOTES.md`
- Maintenance: change request CR1 (extension for the whole class): `docs/CHANGE_REQUEST_CR1.md`

## Layout
- src/domain   entities, polymorphic User, Strategy penalty policies, UserFactory
- src/data     Database wrapper, Repository interfaces and SQLite implementations (parameterised queries only)
- src/service  use-case logic, access control, audit log
- src/security scrypt (OpenSSL) salted password hashing
- src/ui, src/app  console UI and composition root
- db/lms_schema.sql  schema embedded into the program

## Coding standard
C++17, no extensions; 4-space indent; Types CamelCase, members trailing_underscore_, functions camelCase;
RAII for all resources; exceptions derived from lms::LmsError; warnings: -Wall -Wextra -Wpedantic -Wshadow -Wconversion.
