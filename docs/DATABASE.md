# Database Setup and Connection Guide

## How the program connects
The program uses SQLite through the C library. There is no server, user name or password to configure.
The database is one file (default `lms.db` in the folder you run from; change with `--db`).

Connection flow (`src/data/Database.cpp`):
1. `sqlite3_open_v2` opens or creates the file.
2. Sets a 5-second busy timeout and `PRAGMA foreign_keys = ON` (SQLite leaves foreign keys off by default).
3. If no `app_user` table exists, runs the embedded schema (`db/lms_schema.sql`, compiled in by CMake) and sets `PRAGMA user_version = 1`.
   Otherwise checks `user_version`; a mismatch stops the program instead of corrupting data.
4. Only the `data` layer includes `sqlite3.h`. Services use the repository interfaces, and every query is a
   prepared statement with bound parameters, so user input is never concatenated into SQL.
5. Multi-step operations (marking, moderation, release, submission) run in one transaction
   (`BEGIN IMMEDIATE` ... `COMMIT`; rolled back on any exception).

You do not need to run the SQL by hand. The `.sql` file is supplied so you can inspect, submit or recreate the schema.

## Creating or resetting the database
    ./build/lms --seed-demo                 new file with demo data
    rm lms.db && ./build/lms --seed-demo    reset (deletes all data)

Manual creation with the sqlite3 shell: `sqlite3 lms.db < db/lms_schema.sql && sqlite3 lms.db "PRAGMA user_version=1;"`

## Inspecting the data
    sqlite3 lms.db
    .tables
    SELECT id, username, role, substr(password_hash,1,20) FROM app_user;
    SELECT * FROM audit_entry;
GUI alternative: DB Browser for SQLite (open `lms.db`).

## Tables
| Table | Purpose |
|---|---|
| department | departments |
| app_user | all accounts: username, password_hash, salt, full_name, role |
| student | student_number (subtype of app_user, role STUDENT) |
| staff | lecturer / HOD with department (role LECTURER or HOD; one HOD per department) |
| course | course code, title, department |
| course_lecturer | which lecturers teach which course |
| enrolment | which students are enrolled in which course |
| assignment | title, deadline, max mark |
| extension | individual extended deadline for a student |
| submission | versioned student work, status, late_days, raw_mark, final_mark, feedback |
| audit_entry | append-only log of every mark change (who, when, old, new, reason) |

Integrity enforced by the database itself: foreign keys, CHECK constraints on role/status/marks, unique
(assignment, student, version), triggers that reject marks above the assignment maximum, stop released marks
changing, require extensions to be after the deadline, and block UPDATE/DELETE on `audit_entry`.

## Stored formats
- Dates: ISO 8601 UTC text, e.g. `2026-11-07T23:59:00Z`. The UI converts to local time (UTC+2 by default).
- Passwords: `scrypt$<N>$<r>$<p>$<hex key>` plus a separate hex `salt` column. See SECURITY.md.

## Backup and restore
    sqlite3 lms.db ".backup lms_backup.db"     (safe while the program is running)
Restore by stopping the program and copying the backup over `lms.db`.

## Changing the schema later
Edit `db/lms_schema.sql`, increase `kSchemaVersion` in `Database.cpp`, and provide a migration.
Existing files with the old version are rejected on purpose.
