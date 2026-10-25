-- ============================================================
-- Moderated Learning Management System - SQLite schema (v1.0)
-- CS4433 Software Engineering, Phase 3: Database design
-- Dates are stored as ISO 8601 UTC text, e.g. 2026-11-07T23:59:00Z
-- Run with:  sqlite3 school.db < lms_schema.sql
-- ============================================================
PRAGMA foreign_keys = ON;

-- ---------- Organisation ----------
CREATE TABLE department (
    id    INTEGER PRIMARY KEY,
    name  TEXT NOT NULL UNIQUE CHECK (length(trim(name)) > 0)
);

-- ---------- Users and role subtypes ----------
-- app_user holds what every user has. The composite key (id, role) lets the
-- subtype tables prove that a row belongs to the right kind of user.
CREATE TABLE app_user (
    id             INTEGER PRIMARY KEY,
    username       TEXT NOT NULL UNIQUE CHECK (length(username) BETWEEN 3 AND 32),
    password_hash  TEXT NOT NULL,
    salt           TEXT NOT NULL,
    full_name      TEXT NOT NULL CHECK (length(trim(full_name)) > 0),
    role           TEXT NOT NULL CHECK (role IN ('ADMIN','LECTURER','HOD','STUDENT')),
    UNIQUE (id, role)
);

CREATE TABLE student (
    user_id         INTEGER PRIMARY KEY,
    role            TEXT NOT NULL DEFAULT 'STUDENT' CHECK (role = 'STUDENT'),
    student_number  TEXT NOT NULL UNIQUE,
    FOREIGN KEY (user_id, role) REFERENCES app_user (id, role)
);

-- Lecturers and Heads of Department both belong to exactly one department.
CREATE TABLE staff (
    user_id        INTEGER PRIMARY KEY,
    role           TEXT NOT NULL CHECK (role IN ('LECTURER','HOD')),
    department_id  INTEGER NOT NULL REFERENCES department (id),
    FOREIGN KEY (user_id, role) REFERENCES app_user (id, role)
);
-- At most one Head of Department per department.
CREATE UNIQUE INDEX ux_one_hod_per_department ON staff (department_id) WHERE role = 'HOD';
CREATE INDEX ix_staff_department ON staff (department_id);

-- ---------- Courses ----------
CREATE TABLE course (
    id             INTEGER PRIMARY KEY,
    code           TEXT NOT NULL UNIQUE,
    title          TEXT NOT NULL CHECK (length(trim(title)) > 0),
    department_id  INTEGER NOT NULL REFERENCES department (id)
);
CREATE INDEX ix_course_department ON course (department_id);

CREATE TABLE course_lecturer (
    course_id    INTEGER NOT NULL REFERENCES course (id),
    lecturer_id  INTEGER NOT NULL REFERENCES staff (user_id),
    PRIMARY KEY (course_id, lecturer_id)
) WITHOUT ROWID;

CREATE TABLE enrolment (
    student_id  INTEGER NOT NULL REFERENCES student (user_id),
    course_id   INTEGER NOT NULL REFERENCES course (id),
    PRIMARY KEY (student_id, course_id)
) WITHOUT ROWID;
CREATE INDEX ix_enrolment_course ON enrolment (course_id);

-- ---------- Assignments and extensions ----------
CREATE TABLE assignment (
    id         INTEGER PRIMARY KEY,
    course_id  INTEGER NOT NULL REFERENCES course (id),
    title      TEXT NOT NULL CHECK (length(trim(title)) > 0),
    deadline   TEXT NOT NULL CHECK (datetime(deadline) IS NOT NULL),
    max_mark   INTEGER NOT NULL CHECK (max_mark > 0),
    UNIQUE (course_id, title)
);
CREATE INDEX ix_assignment_course ON assignment (course_id);

CREATE TABLE extension (
    id             INTEGER PRIMARY KEY,
    assignment_id  INTEGER NOT NULL REFERENCES assignment (id),
    student_id     INTEGER NOT NULL REFERENCES student (user_id),
    new_deadline   TEXT NOT NULL CHECK (datetime(new_deadline) IS NOT NULL),
    granted_by     INTEGER NOT NULL REFERENCES staff (user_id),
    UNIQUE (assignment_id, student_id)
);
CREATE INDEX ix_extension_student ON extension (student_id);

-- ---------- Submissions ----------
CREATE TABLE submission (
    id             INTEGER PRIMARY KEY,
    assignment_id  INTEGER NOT NULL REFERENCES assignment (id),
    student_id     INTEGER NOT NULL REFERENCES student (user_id),
    version        INTEGER NOT NULL DEFAULT 1 CHECK (version >= 1),
    content        TEXT NOT NULL CHECK (length(content) > 0),
    submitted_at   TEXT NOT NULL CHECK (datetime(submitted_at) IS NOT NULL),
    status         TEXT NOT NULL
                   CHECK (status IN ('SUBMITTED','LATE','MARKED','MODERATED','RELEASED')),
    late_days      INTEGER NOT NULL DEFAULT 0 CHECK (late_days >= 0),
    raw_mark       INTEGER CHECK (raw_mark >= 0),
    final_mark     INTEGER CHECK (final_mark >= 0),
    feedback       TEXT,
    UNIQUE (assignment_id, student_id, version),
    CHECK (status <> 'SUBMITTED' OR late_days = 0),
    CHECK (status <> 'LATE'      OR late_days >= 1),
    -- A mark exists, with feedback, before work can be MARKED or later (BR-07).
    CHECK (status NOT IN ('MARKED','MODERATED','RELEASED')
           OR (raw_mark IS NOT NULL AND final_mark IS NOT NULL
               AND feedback IS NOT NULL AND length(trim(feedback)) > 0))
);
CREATE INDEX ix_submission_assignment ON submission (assignment_id, status);
CREATE INDEX ix_submission_student    ON submission (student_id);

-- ---------- Audit trail ----------
CREATE TABLE audit_entry (
    id             INTEGER PRIMARY KEY,
    submission_id  INTEGER NOT NULL REFERENCES submission (id),
    changed_by     INTEGER NOT NULL REFERENCES app_user (id),
    changed_at     TEXT NOT NULL CHECK (datetime(changed_at) IS NOT NULL),
    old_value      INTEGER,                       -- NULL for the first mark
    new_value      INTEGER NOT NULL,
    reason         TEXT NOT NULL CHECK (length(trim(reason)) > 0)
);
CREATE INDEX ix_audit_submission ON audit_entry (submission_id, changed_at);

-- ============================================================
-- Triggers for rules that span more than one table
-- ============================================================

-- Marks may never exceed the assignment's maximum mark (BR-06).
CREATE TRIGGER trg_submission_mark_max_insert
BEFORE INSERT ON submission
WHEN NEW.raw_mark   > (SELECT max_mark FROM assignment WHERE id = NEW.assignment_id)
  OR NEW.final_mark > (SELECT max_mark FROM assignment WHERE id = NEW.assignment_id)
BEGIN
    SELECT RAISE(ABORT, 'mark exceeds the assignment maximum');
END;

CREATE TRIGGER trg_submission_mark_max_update
BEFORE UPDATE OF raw_mark, final_mark ON submission
WHEN NEW.raw_mark   > (SELECT max_mark FROM assignment WHERE id = NEW.assignment_id)
  OR NEW.final_mark > (SELECT max_mark FROM assignment WHERE id = NEW.assignment_id)
BEGIN
    SELECT RAISE(ABORT, 'mark exceeds the assignment maximum');
END;

-- Released submissions are final, as in the state machine.
CREATE TRIGGER trg_submission_released_is_final
BEFORE UPDATE ON submission
WHEN OLD.status = 'RELEASED'
BEGIN
    SELECT RAISE(ABORT, 'released submissions cannot be changed');
END;

-- An extension must move the deadline later, never earlier.
CREATE TRIGGER trg_extension_after_deadline_insert
BEFORE INSERT ON extension
WHEN datetime(NEW.new_deadline) <= (SELECT datetime(deadline) FROM assignment WHERE id = NEW.assignment_id)
BEGIN
    SELECT RAISE(ABORT, 'extension must be later than the assignment deadline');
END;

CREATE TRIGGER trg_extension_after_deadline_update
BEFORE UPDATE OF new_deadline ON extension
WHEN datetime(NEW.new_deadline) <= (SELECT datetime(deadline) FROM assignment WHERE id = NEW.assignment_id)
BEGIN
    SELECT RAISE(ABORT, 'extension must be later than the assignment deadline');
END;

-- The audit trail is append-only (BR-16).
CREATE TRIGGER trg_audit_no_update
BEFORE UPDATE ON audit_entry
BEGIN
    SELECT RAISE(ABORT, 'audit entries cannot be changed');
END;

CREATE TRIGGER trg_audit_no_delete
BEFORE DELETE ON audit_entry
BEGIN
    SELECT RAISE(ABORT, 'audit entries cannot be deleted');
END;
