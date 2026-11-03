# User Manual

Moderated Learning Management System, version 1.1.0. Pick your role:
[Everyone](#1-for-everyone) · [Student](#2-student) · [Lecturer](#3-lecturer) ·
[Head of Department](#4-head-of-department) · [Administrator](#5-administrator) · [Messages](#6-messages-and-what-to-do)

## 1. For everyone

### Starting and logging in
Start the program (your technician will tell you the command, usually just `lms`). You see:

    === Moderated Learning Management System ===

    Log in (leave the username empty to quit)
    Username:

Type your username, press Enter, then your password. The password does not appear as you type. After five wrong
attempts the program closes; start it again. The message for a wrong username and a wrong password is the same on
purpose: `Invalid username or password.`

### How to use the menus
- Each menu is a numbered list. Type the number and press Enter. `0` always logs you out.
- Things are chosen by their **id number**, shown in square brackets, for example `[3] CS4433 Software Engineering`.
  Look the id up in a list first, then type it when asked.
- Dates and times are typed as `YYYY-MM-DD HH:MM`, for example `2026-11-20 17:00`, and are Lesotho time (SAST).
- Long text (your answer to an assignment) is typed over several lines and finished with a line containing only a full stop `.`
- If you type something wrong you get a message starting with `!` and nothing is changed. Just try again.

### Your password
The administrator gives you a first password. Change it the first time you log in: choose **Change my password**
(the last number before `0` on your menu). A password has 8 to 128 characters, at least one letter and one digit, and
must not be the same as your username. If you forget your password, ask the administrator for a new account.

### How a mark reaches a student
Every mark is checked by the Head of Department before the student can see it.

    Student submits  →  Lecturer marks  →  Head of Department approves  →  Head of Department releases  →  Student sees mark
       SUBMITTED/LATE        MARKED               MODERATED                      RELEASED
                                ↑                                     
                                └──────────  Head of Department can return the work to the lecturer

### The late rule
A submission after the deadline is accepted but marked LATE. A late day is **every started 24 hours**: 1 hour late is 1
late day; 2.5 days late is 3 late days. The **final mark** is the raw mark minus 10% of the **maximum** mark for each
late day, for up to 3 days. More than 3 days late gives a final mark of 0.

| Maximum 100, raw mark 80 | Final mark |
|---|---:|
| On time | 80 |
| 1 day late | 70 |
| 2 days late | 60 |
| 3 days late | 50 |
| More than 3 days late | 0 |

---

## 2. Student

Your menu:

    -- Student --
     1 My assignments and marks
     2 Submit work
     3 Change my password
     0 Log out

### See your assignments and marks (option 1)
Each assignment is listed with its id, course, title and deadline, and one of:
- `not submitted`
- `submitted (v1), awaiting marks`: your work is in; the mark is not released yet
- `MARK 70/100 feedback: ...`: released by the Head of Department

You do **not** see a mark, or whether it has been marked, until it is released. That is the rule, not an error.

### Submit work (option 2)
1. Choose `2`.
2. Type the assignment id.
3. Type your answer. Press Enter for a new line. Finish with a line containing only `.`
4. The program answers `Submitted as version 1 at <time>.` If you were late it adds `(LATE by N day(s))`.

Rules you should know:
- **Before the deadline** you may submit again. The newest version replaces the older ones (`version 2`, ...).
- **After the deadline**, or once your work has been marked, you cannot replace it. Late work is accepted once.
- The answer cannot be empty and is limited to 100,000 characters.
- You can only submit to courses you are enrolled in.
- Your lecturer may give you more time (an extension), for you alone or for the whole class. When you have one, your list shows
  the deadline that applies to you and the original one, for example
  `due 2026-11-13 12:00 (extended from 2026-11-10 23:47)`. Work submitted before the extended time is on time.

---

## 3. Lecturer

Your menu:

    -- Lecturer --
     1 My courses
     2 Create assignment
     3 List assignments
     4 Grant extension (one student or the whole class)
     5 View submissions
     6 Mark a submission
     7 Change my password
     0 Log out

### Create an assignment (option 2)
Type the course id, a title (up to 100 characters), the deadline (`YYYY-MM-DD HH:MM`) and the maximum mark. You can only
create assignments for courses you teach.

### Grant an extension (option 4)
Type the assignment id; the program lists the enrolled students. Then:

    Student user id (0 for the whole class):

- **One student:** type the student's user id, then the new deadline. Only that student is affected.
- **Whole class:** type `0`, then the new deadline. Every student enrolled in the course gets it, all together or not at all.
  The program answers `Extension granted to 18 student(s) until 2026-11-13 12:00.`

The new deadline must be later than the assignment's own deadline. An extension is never shortened: a student who already
has the same or a later extension keeps it, and the program tells you how many did
(`... 2 already had the same or a later extension and kept it.`). Students enrolled after you granted a class extension do
not get it automatically; run option 4 again. Work that was already handed in late stays late.

### View submissions (option 5)
Type the assignment id. Each line shows the submission id, the student, the version, the status, the late days and any marks:

    [12] Thabo Student (student1)  v1  SUBMITTED  late days 0

Only the newest version of each student's work is listed.

### Mark a submission (option 6)
1. Note the submission id from option 5.
2. Choose `6` and type: the submission id, the **raw mark** (0 up to the maximum), your feedback, and a reason.
3. The program confirms with `Marked. Raw 85, final after late penalty 85.` **You enter the raw mark only: the penalty
   is applied for you**, so for late work the final mark is lower than the raw mark.

The reason is needed only when you change a mark that is already `MARKED` (it is kept in the audit log, 500 characters
at most). For a first mark you can leave it empty. You cannot change work after the Head of Department has approved it
(`MODERATED`); only the Head of Department can then adjust the mark. If the Head of Department returns work to you, it
shows again as `SUBMITTED` or `LATE` and you mark it again.

---

## 4. Head of Department

Your menu:

    -- Head of Department --
     1 Department courses
     2 List assignments
     3 Review marks
     4 Adjust a mark
     5 Approve one submission
     6 Approve all marked
     7 Return to lecturer
     8 Release marks to students
     9 Audit history
    10 Change my password
     0 Log out

You can only act on courses in your own department.

**Review marks (3).** Type the assignment id to see every student's newest submission with raw and final marks.

**Approve one (5).** Type the submission id. The program shows the student, raw mark, final mark and feedback and asks
`Approve this mark? (y/n):`. Type `y` to approve. The work becomes `MODERATED`. Only `MARKED` work can be approved.

**Approve all marked (6).** Type the assignment id. The program shows the list, then asks
`Approve the N marked submission(s) shown above? (y/n):`. It approves exactly what you were shown. If a lecturer changed
a mark while you were looking, that one is skipped and the program says so
(`... changed after you reviewed them and were skipped`). Review it again and approve it separately.

**Adjust a mark (4).** Type the submission id, the new **final** mark and a reason (500 characters at most). Use it
for `MARKED` or `MODERATED` work. The new mark must differ from the current one. The change is written to the audit
log with your name and the time.

**Return to lecturer (7).** Type the submission id and a comment (450 characters at most). The work goes back to the
lecturer to be marked again. Only `MARKED` work can be returned.

**Release marks (8).** Type the assignment id. Only `MODERATED` work is released, and only then can students see it:
`Released 18 mark(s); 2 still hidden (not moderated).` Releasing is final for those marks.

**Audit history (9).** Type the submission id to see every change of its mark: when, who, old value, new value and
the reason. The log cannot be edited or deleted.

---

## 5. Administrator

Your menu:

    -- Administrator --
     1 Create department
     2 Create course
     3 Create user
     4 List users
     5 Assign lecturer to course
     6 Enrol student
     7 List courses
     8 Change my password
     0 Log out

Set up a new term in this order, because each step needs ids from the one before:

1. **Create department** (1): type the name. Note its id.
2. **Create course** (2): course code (up to 20 characters), title, department id. Note the course id.
3. **Create user** (3): choose the role (`1` Administrator, `2` Lecturer, `3` Head of Department, `4` Student), then
   the username (3 to 32 characters), full name and an initial password. A student also needs a student number.
   Lecturers and Heads of Department need their department id. A department has only one Head of Department.
4. **List users** (4): shows every user id, username, name and role.
5. **Assign lecturer to course** (5): course id, then lecturer user id.
6. **Enrol student** (6): course id, then student user id.

Tell each person their username and first password in private, and ask them to change it at once. There is no
"forgotten password" button: create a new account for someone who has lost theirs. The administrator cannot read
anyone's password.

---

## 6. Messages and what to do

| Message | Meaning and what to do |
|---|---|
| `Invalid username or password.` | One of them is wrong. Try again; five failures close the program |
| `Please enter a whole number.` | A number was expected (an id or a mark); type digits only |
| `There is no assignment with id N.` | The id is wrong. Look it up in a list first |
| `You are not enrolled in the course for this assignment.` | Ask the administrator to enrol you |
| `No students are enrolled in this course.` | Lecturer: a class extension needs enrolled students; ask the administrator to enrol them |
| `An extension must be later than the assignment deadline.` | Lecturer: type a later date and time than the assignment's own deadline |
| `The deadline has passed, so you can no longer resubmit.` | You submitted on time before, but now it is after the deadline |
| `Your work was submitted late or has already been marked, so it can no longer be replaced.` | Late or marked work cannot be changed; ask your lecturer |
| `The submission must not be empty.` | Type your answer before the line with the full stop |
| `A reason is required when changing a mark.` | Lecturer: give a reason when re-marking marked work |
| `This work has already been moderated, so only the Head of Department can change its mark.` | Lecturer: ask the Head of Department |
| `Only marked work can be approved (this work is ...)` | The work is not yet `MARKED`, or already approved |
| `The new mark is the same as the current mark.` | Head of Department: type a different mark |
| `! An unexpected error occurred. Nothing was changed.` | Nothing was saved. Try again; if it repeats, tell the technician the time and what you typed |
