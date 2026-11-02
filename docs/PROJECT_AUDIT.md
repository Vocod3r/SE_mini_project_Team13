# Project Audit

**Repository:** `Vocod3r/SE_mini_project_Team13`
**Audited revision:** `fa3b1b0` (`main`, 2026-11-02)
**Audit baseline:** Initial read-only inspection of source, tests, configuration, Git history, SRS, Software Test Plan, and component diagram, before Sprint 1 work. See the Sprint 1 update at the end for current findings and executed tests.

## Executive summary

This remains an in-progress C11/SQLite console HMS, not a complete desktop or LAN product. Sprints 1 through 5 corrected core persistence/security and added reliability checks. Sprint 6 now provides a role-based console entry point and service boundary; some requirements and layered-design gaps remain. TCP service/client support is optional in the SRS and is not implemented. Eleven test programs pass in the latest `make ci` on Ubuntu 24.04, including AddressSanitizer and UndefinedBehaviorSanitizer runs.

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
    ├── auth.c, auth.h              canonical staff authentication and PBKDF2
    ├── system_compliance.c, .h     availability/menu checks
    ├── seed10k.sql, perf.sql       seed and manual query timing scripts
    ├── crash.c                     process termination experiment helper
    └── test_*.c                    eleven standalone test programs
```

A `main` application and role-based console UI now exist, including Admin backup/restore commands. TCP/network client/server support remains absent. A README is present. No generated database or private patient data is tracked. SQLite and OpenSSL headers/libraries are external build dependencies; no dependency manifest pins their versions.

## Technology and architecture

- C11, compiled with GCC; SQLite through the SQLite3 C API.
- Local embedded SQLite database; `db_open` enables foreign keys, WAL journaling, and a 3-second busy timeout.
- `db.c` is a global-connection data-access layer and contains most business rules, SQL, printing, audit calls, backup, and health checks.
- `console.c` owns terminal interaction, `app_service.c` provides authenticated workflow entry points, and patient/consultation/appointment-view files provide domain wrappers; direct console-to-domain calls and mixed responsibilities in `db.c` remain.
- Doctor management remains a low-level module receiving a SQLite handle; its SQL is now parameterized and it maps working hours to the canonical `work_start`/`work_end` columns. Authentication now uses the canonical `staff` table and OpenSSL PBKDF2-HMAC-SHA256.
- The component diagram describes a console UI, seven manager components, a data-access layer, audit logger, and SQLite DB. In the checked-in sources, the console UI now exists, while the report manager, TCP client/server, and dedicated audit component are absent; the diagram remains a target design rather than a faithful picture of the implementation.

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
- Patient, appointment, and prescription create/update/delete writes have timestamped user audit events in checked transactions. Doctor, staff, and bill writes also check their audit inserts. SQLite triggers reject audit-log UPDATE/DELETE while installed; file/schema owners can remove them.

### Missing or incomplete relative to the SRS

- A role-based console now supports core patient, appointment, clinical, and Admin workflows; billing and several functional workflows still need UI integration. No TCP/LAN server/client or authenticated protocol.
- Account creation/authentication implementations disagree on table and password format; legacy `users` records use the older unsalted format; `auth.c` now authenticates through `staff` using salted PBKDF2 and upgrades migrated credentials after successful login.
- The console creates an authenticated session after login, but that identity is not propagated through every direct DAL call; read APIs without a user identity cannot enforce roles themselves.
- Prescription update/delete APIs and SQLite append-only DML guards are now implemented. Direct database file/schema owners can remove those guards; read-access auditing and comprehensive privilege separation remain gaps.
- Backup is validated using SQLite integrity checks, and restoration was verified by opening a separate test copy. Admin backup/restore menu options call the existing authorization-checked APIs.
- No automatic idle logout, uptime log/week-long availability evidence, CSV report export, or actor-bound health-check DAL wrapper.
- Doctor APIs remain split between manager and DAL entry points, but both now use the canonical schema; the manager binds SQL parameters and does not depend on the removed legacy columns.
- Input validation remains incomplete across all fields. Authentication and doctor-manager SQL are now parameterized. `crash.c` still calls `_exit` without including its declaration's standard header and targets a shared `hms.db` filename.
- Patient, appointment, prescription, staff, doctor, and billing writes now check transaction/audit outcomes. Authentication state updates, read-access logging, and some noncritical operations remain outside comprehensive audit coverage.
- `db_get_patient_history` remains newest-first as required by AC-013. Prescription history (`HMS-F-041`) now sorts oldest-to-newest per AC-041.

## Test coverage in the checkout

There are eleven test programs: the original six plus `test_auth.c`, `test_core.c`, `test_reliability.c`, `test_app_service.c`, and `test_audit.c`. New assertions cover schema shape and existing-record preservation, Admin-only account creation, salted hashes and legacy-account migration, role spoof rejection, chronological history, backup validation/restore opening, and audit-failure rollback. `test_patient.c` bulk-seeds 10,000 fixtures in a single test transaction, then measures two patient lookups. `test_db.c` remains a print-based smoke program; `test_db2.c` now also asserts persisted-role enforcement. Sprint 5 added isolated Linux WAL interruption/recovery evidence. Sprint 6 tests Admin bootstrap, authentication, account creation, patient registration, appointment authorization, and prescription recording; its executable smoke walk covers fresh setup, login, menu display, sign-out, and quit. Sprint 7 tests append-only DML guards, audit metadata, transactional audit failures, and prescription update/delete. Privileged audit-guard removal, full input fuzzing, Linux/Windows parity, read-access auditing, and optional TCP remain untested.

- docs/TEST_EXECUTION_REPORT.md: actual CI and sprint test results.

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
5. Backup step/finalization and integrity are now checked; a test opens the copied database and compares schema/data. A user-facing Admin restore workflow now exists; platform and broader recovery coverage remain incomplete.
6. Patient/appointment/prescription create-update-delete writes are audited transactionally; prescription update/delete APIs and append-only DML triggers are tested. A database file/schema owner can drop those triggers, and read-access auditing is absent.
7. Patient, appointment, prescription, staff, doctor, and billing write paths now check audit and transaction results; Sprint 5 verifies WAL recovery in a child-process test. Authentication state updates and some other operations still need review.
8. Some older smoke tests still do not check `db_init_schema` results; the new core/auth tests do.
9. Platform support is unproven: the checked-in CI only targets Ubuntu, while shell recipes and POSIX sanitizer options in the Makefile are not directly portable to native Windows make.
10. Resolved in Sprint 4: the test seeds 10,000 patients and 10,000 appointments, then measures 50 repeats for patient ID/name/phone, appointment ID, and doctor/date lookup using monotonic time. Target-hardware performance still needs validation.

## Sprint 1 update

The current code differs from the initial audit snapshot: the schema/manager mismatch is resolved without dropping tables or records; auth, selected DAL role checks, F-041 ordering, transactional audit writes, backup validation, role-based console workflows, and audit DML guards were implemented and tested. Remaining requirement gaps are tracked in `docs/REQUIREMENT_TRACEABILITY.md`.

## Audit and sprint documents

- `docs/PROJECT_AUDIT.md` — this repository and implementation audit.
- `docs/REQUIREMENT_TRACEABILITY.md` — SRS/STP/request mapping and conflicts.
- docs/SPRINT_PLAN.md: seven-sprint plan and current commit status.
- docs/TEST_EXECUTION_REPORT.md: actual CI and sprint test results.
