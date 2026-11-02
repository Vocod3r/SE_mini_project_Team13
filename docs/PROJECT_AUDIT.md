# Project Audit

**Repository:** `Vocod3r/SE_mini_project_Team13`
**Audited revision:** `fa3b1b0` (`main`, 2026-11-02)
**Audit baseline:** Initial read-only inspection of source, tests, configuration, Git history, SRS, Software Test Plan, and component diagram, before Sprint 1 work. See the Sprint 1 update at the end for current findings and executed tests.

## Executive summary

This remains an in-progress C11/SQLite data-access prototype, not a complete desktop or LAN HMS. Sprint 1 corrected the core schema, staff authentication path, selected data-access role checks, transactional audit writes, and backup validation. There is still no console entry point or TCP service/client. Eight test programs now pass in `make ci` on Ubuntu 24.04, including AddressSanitizer and UndefinedBehaviorSanitizer runs.

At audit start there was no README, sprint/backlog record, or requirements traceability document. A baseline README and planning documents were added. Sprint 1 found that the second doctors declaration was guarded by `IF NOT EXISTS`, so schema initialization did not fail; it was silently ignored, leaving `doctor_manager.c` expecting columns absent from the canonical table. That mismatch is now fixed.

## Repository structure

```text
.
├── SRS_SE_miniproject_Team13 (1).pdf
├── HMS_Software_Test_Plan .pdf
├── se_component_dia.jpeg
├── .gitignore
├── .github/workflows/ci.yml
└── src/
    ├── Makefile
    ├── db.c, db.h                  SQLite data-access implementation/API
    ├── schema.sql                  SQLite tables and indexes
    ├── patient.c, patient.h        thin patient service wrappers
    ├── consult.c, consult.h        role-checking consultation wrappers
    ├── appt_view.c, appt_view.h    role-checking schedule/slip wrappers
    ├── doctor_manager.c, .h        separate direct-SQL doctor implementation
    ├── auth.c, auth.h              separate users-table authentication code
    ├── system_compliance.c, .h     availability/menu checks
    ├── seed10k.sql, perf.sql       seed and manual query timing scripts
    ├── crash.c                     process termination experiment helper
    └── test_*.c                    eight standalone test programs
```

There is no `main` application, UI implementation, network module, or user-facing restore command. A README is present. No generated database or private patient data is tracked. SQLite and OpenSSL headers/libraries are external build dependencies; no dependency manifest pins their versions.

## Technology and architecture

- C11, compiled with GCC; SQLite through the SQLite3 C API.
- Local embedded SQLite database; `db_open` enables foreign keys, WAL journaling, and a 3-second busy timeout.
- `db.c` is a global-connection data-access layer and contains most business rules, SQL, printing, audit calls, backup, and health checks.
- Patient, consultation, and appointment-view files provide small service wrappers; these do not form a complete UI/business/data separation.
- Doctor management remains a low-level module receiving a SQLite handle; its SQL is now parameterized and it maps working hours to the canonical `work_start`/`work_end` columns. Authentication now uses the canonical `staff` table and OpenSSL PBKDF2-HMAC-SHA256.
- The component diagram describes a console UI, seven manager components, a data-access layer, audit logger, and SQLite DB. In the checked-in sources, the UI, report manager, TCP client/server, and dedicated audit component are absent; the diagram is therefore a target design, not a faithful picture of the implementation.

## Data model

The intended `schema.sql` model has:

- `staff`: username, password hash, salt, role, failed login count, lock expiry.
- `patients`: demographics and archive flag; indexes on name and phone.
- `doctors`: optional staff link, name, specialization, work hours, consultation fee.
- `appointments`: patient/doctor foreign keys, date, time slot, status; a partial unique index prevents active double-booking and an index supports doctor/date lookup.
- `visits`: appointment/patient links, diagnosis, visit date.
- `prescriptions`: visit/patient links, medicine text, dosage, notes.
- `bills`: visit link, amount, payment status and creation time.
- `audit_log`: user ID, action, table, record ID and timestamp.

The baseline schema contained a second `CREATE TABLE IF NOT EXISTS doctors` declaration with incompatible `working_hours`/`is_active` columns. It did not stop fresh initialization; SQLite ignored it because the canonical table already existed. Sprint 1 removed the duplicate and updated the manager to use the canonical columns. The legacy `users` table remains in the schema so its account rows can be migrated into `staff`; source rows are retained. The model still does not store separate prescription frequency, duration, or prescribing doctor fields, and `audit_log.user_id` has no foreign key to staff.

## Existing functionality assessment

### Present in source

- Patient registration, demographic update, search by ID/name/phone, and archive. Search uses bound parameters; archived patients are hidden from search and appointment booking.
- Appointment booking with an active-slot unique index, rescheduling, cancellation, daily/weekly schedules, slip display, and queue counts.
- Doctor-only visit/prescription recording in a transaction; patient visit and prescription history queries.
- Consultation billing and payment status; daily appointment report; database copy via SQLite backup API; database/disk health check.
- Staff creation, Admin actor checks, salted PBKDF2 authentication, legacy `users` account migration on schema initialization, legacy hash upgrade after successful login, and three-failure lockout.
- Data-access role checks compare the claimed role with the persisted staff role for the protected write paths; role checks remain incomplete for APIs that do not accept an actor identity.
- Patient, appointment, and prescription writes covered by SR-003 are audited transactionally; failed audit inserts roll back the corresponding write.

### Missing or incomplete relative to the SRS

- No executable console UI or user workflow; no TCP/LAN server/client or authenticated protocol.
- Account creation/authentication implementations disagree on table and password format; legacy `users` records use the older unsalted format; `auth.c` now authenticates through `staff` using salted PBKDF2 and upgrades migrated credentials after successful login.
- The application still has no integrated UI or authenticated session boundary; read APIs without a user identity cannot enforce roles themselves.
- Prescription update/delete APIs and a mechanism preventing audit-log modification are still absent.
- Backup is validated using SQLite integrity checks, and restoration was verified by opening a separate test copy. There is still no Admin-facing backup/restore workflow or authorization wrapper.
- No automatic idle logout, uptime log/week-long availability evidence, CSV report export, or administrator-only health check wrapper.
- Doctor APIs remain split between manager and DAL entry points, but both now use the canonical schema; the manager binds SQL parameters and does not depend on the removed legacy columns.
- Input validation remains incomplete across all fields. Authentication and doctor-manager SQL are now parameterized. `crash.c` still calls `_exit` without including its declaration's standard header and targets a shared `hms.db` filename.
- Patient, appointment, prescription, and staff-account changes now check transaction/audit outcomes. Other mutators, including billing and doctor CRUD, still ignore some audit failures.
- `db_get_patient_history` remains newest-first as required by AC-013. Prescription history (`HMS-F-041`) now sorts oldest-to-newest per AC-041.

## Test coverage in the checkout

There are eight test programs: the original six plus `test_auth.c` and `test_core.c`. New assertions cover schema shape and existing-record preservation, Admin-only account creation, salted hashes and legacy-account migration, role spoof rejection, chronological history, backup validation/restore opening, and audit-failure rollback. `test_patient.c` bulk-seeds 10,000 fixtures in a single test transaction, then measures two patient lookups. `test_db.c` remains a print-based smoke program; `test_db2.c` now also asserts persisted-role enforcement. Crash durability, audit tamper prevention, full input fuzzing, Linux/Windows parity, and TCP are still untested.

The Makefile offers `make compile`, `make test`, and `make ci` (`compile`, `test`, then ASan/UBSan rebuild and test). GitHub Actions installs SQLite and OpenSSL development headers on Ubuntu before `make ci`. Sprint 1 ran `make test CC=gcc` successfully in MSYS2 UCRT64 and ran the complete `make ci` successfully in Ubuntu 24.04; exact commands and results are recorded in `docs/TEST_EXECUTION_REPORT.md`.

## Requirements and documentation status

The SRS lists 23 functional requirements (F-001..003, F-010..014, F-020..024, F-030..032, F-040..041, F-050..051, F-060..062), six non-functional requirements, two security objectives, and five security requirements. The SRS note incorrectly says 22 functional requirements; the Software Test Plan correctly calls out the 23 listed FRs. The SRS RTM maps only eight requirements; the STP expands coverage to all listed requirements. Neither provides sprint allocation.

The user confirmed that the original SRS and Software Test Plan are authoritative: HMS-F-040 is prescription recording; HMS-F-041 is prescription-history retrieval; AC-041 requires chronological order; TC-Rx-02 tests retrieval. Traceability and implementation use this mapping.

## Git branches and commits

Working tree was clean at audit start and is on `main`, tracking `origin/main`. Remote `origin` is `https://github.com/Vocod3r/SE_mini_project_Team13.git`. Branches: local `main`; remote `origin/main` and `origin/smpt-21-22-schedule-slip`. At audit start the checkout was on `main`; Sprint 1 work is on `feat/sprint-1-core` pending its dedicated commit.

History from oldest to newest:

| Commit | Date | Subject / evidence |
|---|---|---|
| `b2a8b12` | 2026-09-05 | Initial SRS submission |
| `5e73b6e` | 2026-09-06 | Removed an earlier SRS PDF copy |
| `4966664` | 2026-09-06 | Added current-named SRS PDF |
| `1d28f3d` | 2026-09-30 | Added component diagram |
| `7b96f56` | 2026-10-02 | Added Software Test Plan PDF |
| `0abec32` | 2026-10-29 | Initial SQLite DAL, schema, patient/consult modules, tests |
| `ec2d74c` | 2026-10-29 | Makefile and Ubuntu GitHub Actions workflow |
| `ebd32ec` | 2026-10-29 | Weekly schedule and appointment slip, on feature branch |
| `bc6b67f` | 2026-10-29 | Merged schedule/slip work into main |
| `e0abfc7` | 2026-11-02 | Added separate auth/doctor implementations and schema additions |
| `fa3b1b0` | 2026-11-02 | Added two compliance checks and compliance test |

Commits mention “sprint” deliverables, but no sprint number, sprint goal, or definition of done is recorded. Therefore this audit counts **zero reliably documented completed sprints** and does not infer a sprint history from commit wording.

## Potential bugs and risks

1. Resolved in Sprint 1: removed the duplicate incompatible doctors declaration and aligned the manager with the canonical schema.
2. New account creation/authentication use `staff`; legacy `users` rows are retained and copied into `staff` on schema initialization. Legacy rows upgrade their password hash after a successful login.
3. Resolved for the active auth path: PBKDF2-HMAC-SHA256 with random salts and parameterized account lookup/writes. Existing legacy `users` rows remain unsalted until they next log in successfully.
4. Resolved: HMS-F-041 prescription retrieval is oldest-to-newest, with tests for dates, fields, and no-history behavior.
5. Backup step/finalization and integrity are now checked; a test opens the copied database and compares schema/data. No user-facing restore workflow exists.
6. Required patient/appointment/prescription audit writes now roll back their data changes on audit failure and use create/update/delete actions. No mechanism prevents audit-log modification; prescription update/delete APIs are absent.
7. Patient, appointment, prescription, account-create, and backup transactions now check completion; other write paths still need review.
8. Some older smoke tests still do not check `db_init_schema` results; the new core/auth tests do.
9. Platform support is unproven: the checked-in CI only targets Ubuntu, while shell recipes and POSIX sanitizer options in the Makefile are not directly portable to native Windows make.
10. The 10,000-record test covers two patient searches only; it does not measure appointment lookups, repeat runs, or control timing noise.

## Sprint 1 update

The current code differs from the initial audit snapshot: the schema/manager mismatch is resolved without dropping tables or records; auth, selected DAL role checks, F-041 ordering, transactional required audit writes, and backup validation were implemented and tested. Remaining requirement gaps are tracked in `docs/REQUIREMENT_TRACEABILITY.md`.

## Audit and sprint documents

- `docs/PROJECT_AUDIT.md` — this repository and implementation audit.
- `docs/REQUIREMENT_TRACEABILITY.md` — SRS/STP/request mapping and conflicts.
- `docs/SPRINT_PLAN.md` — proposed work sequence; no sprint is approved or started.
- `docs/TEST_EXECUTION_REPORT.md` — audit-stage test execution status; no tests were run.
