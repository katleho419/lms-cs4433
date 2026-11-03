# Professional, Legal and Ethical Issues

Moderated Learning Management System, version 1.1.0. Written 2026-11-04.

This section looks at the system as it is built: what personal data it holds, which laws and licences apply, and the
ethical questions that come with software that decides and records students' marks. The legal parts are an engineering
reading of the text of the law, not legal advice. A school that installs the system should ask its own legal officer.

## 1. Who is affected

| Group | What the system does to them | What they need |
|---|---|---|
| Students | Stores their work, marks and feedback; applies a late penalty | Fair marks, privacy, a way to see and question their record |
| Lecturers | Records their marking and every change to a mark | Fair treatment, a clear record of what they did |
| Heads of Department | Gives them the power to approve, adjust and return marks | Accountability for that power |
| Administrator | Creates every account | Trust, and limits on what they can see |
| The school (data controller) | Holds all of the above on its computer | Compliance with the law, a system it can explain |
| The developer | Responsible for the quality and honesty of the software | Professional standards |

## 2. Personal data held by the system

| Data | Where | Purpose | Sensitivity |
|---|---|---|---|
| Username, full name | `app_user` | Login, display | Personal data |
| Student number | `student` | Identify a student | Personal data (identifier) |
| Department and role | `staff`, `app_user` | Access control | Personal data |
| Password (salted scrypt hash, never the password) | `app_user` | Login | Security data |
| Submitted work (text) | `submission` | Marking | Personal data; may contain anything the student wrote; student's own work |
| Raw and final marks, feedback | `submission` | Assessment | Personal data with real consequences for the person |
| Who changed a mark, when, from what to what, and why | `audit_entry` | Accountability | Personal data about staff and students; cannot be edited or deleted |
| Extensions | `extension` | Deadlines | Personal data |

The system does **not** store: health data, religion, ethnicity, political views, contact details, photographs, location,
login history or IP addresses. Not collecting these was a design choice (data minimisation).

**Check for minors.** The law treats anyone under 18 as a child. The system was developed for a university course, but its
installation guide is written for a school. If the learners at a school are under 18, the rules on children's data (section
3.2) apply and the school must handle parental consent. The software cannot do that for the school.

## 3. Legal issues

### 3.1 Data protection law of Lesotho

The Data Protection Act, 2011 (Act No. 5 of 2012; the text on LesothoLII is titled "Data Protection Act, 2012") applies to
whoever decides why and how personal data is processed. That is the **school**, which is the "data controller". The
developer supplies a tool and is not the controller, but the tool should make compliance possible. Section numbers below are
from the text published on [LesothoLII](https://lesotholii.org/akn/ls/act/2012/5/eng@2012-02-22).

| Requirement | Section | How the system helps | What is missing or left to the school |
|---|---|---|---|
| Lawful basis for processing | 15 | Data is used only for teaching and assessment | The school must record its basis (its educational duty or the student's agreement) |
| Collect only what is needed | 16 | Only the fields in section 2 are stored | None |
| Tell people what is collected and why | 25 | Nothing in the program | The school must give students and staff a privacy notice |
| Specific purpose, no incompatible use | 18 | Data is only shown to the roles that need it | School policy must forbid other uses (for example, selling or advertising) |
| Accuracy | 24 | Heads of Department can correct marks and every change is logged | Names and numbers can only be corrected by creating a new account (no edit feature) |
| Keep no longer than needed, then destroy | 19 | **No retention feature.** The audit log cannot be deleted even through SQL | The school needs a retention rule, and a documented procedure (a technician using SQL) for old terms |
| Security measures | 20 | scrypt salted password hashes; role and relationship checks in every service; parameterised SQL only; data file readable by its owner only; marks hidden until released | The data file is **not encrypted**. The school must protect the computer, the backups and the single shared operating-system account |
| Tell the Commission and people about a breach | 23 | None | The school needs a procedure for a lost computer or stolen backup |
| People may see their data | 26 | **No export feature.** A student can see their released marks | The school must be able to answer a request, using the data file, within the time the Act sets |
| People may correct or delete their data | 27 | Marks can be corrected (Head of Department, logged). **No delete feature** | See 19 above. The right to deletion conflicts with an audit log that is deliberately permanent: the school should decide in its policy which records it must keep (marks and their history) |
| Automated decisions | 51 | The late penalty is automatic, but a Head of Department approves every mark and can adjust it with a recorded reason | None. This human step is what keeps the system outside "decision based only on automated processing" |
| Sending data abroad | 52 | Everything stays in one file on one computer; no network | Backups on a USB stick or a cloud drive abroad would be a transfer. Keep backups in the country or protect them as the Act requires |
| Tell the Commission about the processing | 53 | None | The school (controller) must notify the Commission, including a description of its security measures. Section 2 of this document and `SECURITY.md` can be used for that description |
| Penalties | 55 | | Offences carry a fine up to M50,000, up to 5 years in prison, or both |

### 3.2 Children's data

Section 29 and the rest of that part of the Act restrict processing of the data of children (under 18) unless a permitted
ground applies, parental consent being one. A university installation (adults) is not affected. A school installation with
learners under 18 should get parental consent or rely on another permitted ground, and should not enable any extra data
collection without legal advice.

### 3.3 Software licences

| Component | Used for | Licence | Effect on this project |
|---|---|---|---|
| SQLite | The data file | Public domain | No conditions |
| OpenSSL 3.x | scrypt password hashing | Apache License 2.0 (OpenSSL 3.0 and later; earlier versions used a different licence) | Keep the licence notice if the library is distributed with the program. The project links to it from the operating system and does not ship it |
| GoogleTest 1.14 | Tests only | BSD 3-Clause | Not part of the installed program |
| Own code | Everything in `src/`, `tests/`, `tools/`, `docs/` | **No licence file yet** | Without a licence, nobody else is legally allowed to use or change the code. See below |

**Decision for the developer (open).** The repository has no `LICENSE` file. A permissive licence such as MIT would allow the
school and others to use the system. Before choosing, check the university's rules on who owns coursework, because some
institutions own student work or require a particular licence. The repository is public, so this matters.

### 3.3.1 Copyright of student work

Students own the work they submit. The system stores it only for marking, and shows it only to the lecturer, the Head of
Department and (after release) the student. It does not check for plagiarism and makes no claim over the text. A
school using it should say in its policy how long work is kept.

### 3.4 Security law and good practice

The code follows common practice (salted memory-hard hashing, least privilege, no secrets in the source, parameterised
queries). Deliberate gaps are listed in `SECURITY.md`: no encryption of the data file, no password reset, no lock-out that
survives a restart. These are stated openly in the release notes so a school can decide whether they are acceptable.

## 4. Professional and ethical issues

The work was checked against the [ACM Code of Ethics and Professional Conduct](https://www.acm.org/code-of-ethics)
(the Software Engineering Code of Ethics of ACM and IEEE-CS says much the same). The most relevant principles and what was done:

| Principle | What it asks | What the project did |
|---|---|---|
| Contribute to society and avoid harm | Think about who is hurt if the software is wrong | A wrong mark affects a student's future, so marks follow a state machine, are approved by a second person, hidden until released, and every change is logged. Defects in marking and release rules were found by testing and fixed (`DEFECT_LOG.md`, DEF-01 and DEF-02) |
| Be honest and trustworthy | Do not overstate what the software does | Known limitations are written in the release notes and `SECURITY.md`. The test reports record the faults and gaps found, including gaps in the tests themselves (TG-01) |
| Be fair and take action not to discriminate | Rules must treat people alike | The late rule is one rule for everyone, applied by the program, not by a person's mood. Extensions are visible to the student who gets one, and a class extension treats everybody enrolled the same |
| Respect privacy | Collect only what is needed | Section 2: no contact details, no health data, no tracking |
| Honour confidentiality | Show data only to those who need it | Role and relationship checks (a lecturer sees only their own course; a student sees only their own work; marks are hidden until released). Passwords are never stored or shown |
| Perform work only in areas of competence; give thorough evaluation | Test and review | Unit, integration, system and acceptance tests; static analysis; a code review with a checklist; coverage and metrics reports |
| Be accountable | Be able to show what happened | The permanent audit log records who changed a mark, when and why |
| Respect the work required to produce new ideas and software | Credit sources and licences | Section 3.3; third-party libraries are used under their licences and not copied into the source |

### 4.1 Ethical questions specific to this system

| Question | Discussion | Position taken |
|---|---|---|
| Is an automatic late penalty fair? | It is the same for everyone, but it relies on the computer's clock. If the clock is wrong, students are penalised unfairly | The school's technician must keep the clock correct (installation guide, checklist). A Head of Department can adjust a mark and the change is recorded. The penalty rule is a replaceable policy (Strategy pattern), so a school can change it |
| Who watches the watchers? | The Head of Department can change any mark in their department | Every change needs a reason and is written to a log that cannot be edited or deleted, even by SQL |
| Can the administrator see too much? | The administrator creates every account | The administrator cannot read anyone's password (only a hash is stored). They can list accounts. There is no password reset, so a lost password means a new account, which is safe but inconvenient |
| Shared operating-system account | The installation guide has everyone run the program as one Linux account, so the operating system cannot tell users apart | The **program's** audit log identifies the person who changed a mark, which is what matters for accountability. The school must not share program passwords |
| Extensions and equal treatment | An extension for one student could be seen as favouritism | The extension is stored with who granted it. In version 1.1.0 the student sees it on their list. A class extension (CR1) is the fair way to give everyone the same help |
| Access and inclusion | The program is a text console in English. Time is Lesotho time only | Listed as a limitation. A web or Sesotho version, and screen-reader testing, would be future work |
| Retention versus deletion | People have a right to correction and deletion, but marks and their history must be kept to be trustworthy | Section 3.1 leaves the decision to school policy and says so openly |
| Publishing demo passwords | The README lists demo accounts | A warning says never to use `--seed-demo` on a real installation, and the installation guide repeats it (code review finding R-04) |

### 4.2 Use of AI assistance

This project was developed with an AI assistant (Claude, by Anthropic) that helped to write and review code, tests and
documents. The author directed the work, ran and checked the results, and is responsible for the submitted work.
**The student must check the course rules on AI assistance and declare it in the form the course requires.** This
paragraph is a starting point, not a substitute for that declaration.

## 5. What the school should do before going live

- [ ] Decide who the data controller is and notify the Data Protection Commission (section 53).
- [ ] Give staff and students a privacy notice (section 25).
- [ ] Decide how long records are kept and how an old term is archived or destroyed (section 19).
- [ ] Decide how a request to see or correct data is answered (sections 26 and 27).
- [ ] Protect the computer, the data file and the backups; keep the clock correct.
- [ ] Prepare a procedure for a breach (section 23).
- [ ] If learners are under 18, arrange parental consent or another lawful ground.
- [ ] Never use `--seed-demo`; give every person their own account.

## 6. Possible future work that would close the gaps

| Gap | Idea |
|---|---|
| No export of a person's data | A report for one student or staff member (supports section 26) |
| No retention tool | An archive-and-remove procedure for finished terms, designed with the audit log in mind |
| Data file not encrypted | Encrypted storage (for example SQLCipher) or disk encryption |
| No password reset | A reset by the administrator that forces a change at the next login |
| Names cannot be corrected | An "edit user" function with an audit entry |
| No list of extensions for the lecturer | A report of who has an extension (see `CHANGE_REQUEST_CR1.md`) |

## 7. Sources

- [Data Protection Act (Lesotho), text on LesothoLII](https://lesotholii.org/akn/ls/act/2012/5/eng@2012-02-22), read 2026-11-04. Section numbers in this document are from that text.
- [ACM Code of Ethics and Professional Conduct](https://www.acm.org/code-of-ethics).
- Licences: OpenSSL 3.x is licensed under Apache 2.0 (see [openssl.org/source/license.html](https://www.openssl.org/source/license.html); the page could not be opened automatically, so the statement is from the OpenSSL project's announcements and should be checked); SQLite is in the public domain (sqlite.org/copyright.html); GoogleTest is BSD 3-Clause (its repository `LICENSE` file). The last two were stated from general knowledge and were not re-checked online for this document.
