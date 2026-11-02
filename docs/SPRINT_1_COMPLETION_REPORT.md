# Sprint 1 Completion Report

**Sprint:** 1 — Core data and access foundation
**Status:** Implementation and verification complete in the workspace; changes are verified and ready for the authorized dedicated Sprint 1 commit.
**Branch:** `feat/sprint-1-core`
**Baseline:** `fa3b1b0`
**Requirement authority:** Original SRS and Software Test Plan. HMS-F-040 is prescription recording; HMS-F-041 is prescription history; AC-041 is chronological order; TC-Rx-02 tests retrieval.

## What changed

- Removed the shadow `doctors` declaration and aligned the existing doctor manager with the canonical doctor schema. Doctor-manager statements now bind user values instead of interpolating SQL. Existing canonical doctor rows are preserved.
- Consolidated authentication/account creation on `staff`. Existing `users` rows are retained and copied to `staff` during schema initialization; a legacy account is upgraded to a salted PBKDF2-HMAC-SHA256 hash after a successful login. Account creation requires an Admin actor verified against the database.
- Bound protected data-access role checks to persisted staff roles for doctor, patient, appointment, consultation, and billing mutators. Appointment booking, cancellation, and rescheduling now require an authorized stored role.
- Made patient create/update/archive, appointment create/update/delete, and visit/prescription writes fail atomically when their audit insert or transaction commit fails. Audit action names now use create/update/delete for required patient/appointment/prescription events.
- Added structured F-041 prescription retrieval in chronological order, preserving the existing visit-history ordering behavior for AC-013.
- Made SQLite online backup completion dependent on successful copy finalization and `integrity_check`.
- Changed tests to use isolated data and real persisted staff identities. Added auth and core regression tests, an exact 10,000-patient performance fixture, and safe cleanup. The Makefile clean target no longer deletes arbitrary database files.
- Added OpenSSL build dependencies to Makefile and GitHub Actions; updated README, audit, traceability, sprint, and test execution documentation.

## Requirements verified

- **HMS-F-041 / AC-041 / TC-Rx-02:** chronological retrieval and empty history.
- **HMS-F-061 / AC-061 / TC-Rep-02:** SQLite backup copied, opened separately, data/schema checked, and integrity verified; invalid destination fails.
- **HMS-NF-001 / AC-NF-001 / TC-Perf-01:** exactly 10,000 patient rows; phone and name lookups measured at 12.6 ms combined in the latest normal Linux run and 13.1 ms combined under sanitizers.
- **HMS-SR-001/002/003/004:** salted hash generation, persisted-role checks for protected mutation APIs, audit coverage/rollback tests, and parameterized authentication/doctor-manager SQL.
- Existing appointment, consultation, clinical, and compliance tests also pass.

Requirement-by-requirement status and remaining acceptance gaps are in [REQUIREMENT_TRACEABILITY.md](REQUIREMENT_TRACEABILITY.md). Passing the suite does not mean every SRS requirement is complete.

## Changed files

Modified tracked files:

- `.github/workflows/ci.yml`
- `src/Makefile`
- `src/auth.c`, `src/auth.h`
- `src/db.c`, `src/db.h`
- `src/doctor_manager.c`
- `src/schema.sql`
- `src/test_appt_view.c`, `src/test_clinical.c`, `src/test_db.c`, `src/test_db2.c`, `src/test_patient.c`

New files:

- `README.md`
- `docs/PROJECT_AUDIT.md`, `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_1_COMPLETION_REPORT.md`
- `src/test_auth.c`, `src/test_core.c`

## Test results

Exact final command and per-test results are in [TEST_EXECUTION_REPORT.md](TEST_EXECUTION_REPORT.md). The final Ubuntu 24.04 `make ci` run exited **0**: all eight test executables passed in the standard run and ASan/UBSan run. `git diff --check` also passed. The test DB files created during this work were cleaned; the working tree contains source/documentation changes only.

## Remaining issues

- The console UI, role-specific menus, and LAN client/server are not implemented.
- Some low-level functions without an actor ID cannot enforce authorization directly; `doctor_manager` exposes direct SQLite operations without an Admin identity parameter.
- Legacy `users` rows are preserved and remain unsalted in that legacy table; their copied `staff` credential is upgraded after the first successful login. A staged migration/deprecation is still needed for the legacy table itself.
- Prescription update/delete APIs and audit-log tamper prevention are absent. Some non-required audit paths, including doctor and billing writes, still do not make data changes atomic with audit failures.
- HMS-NF-001 timing covers patient search only; appointment lookup and repeated load are not measured. Crash/power-loss validation, Windows sanitizer runs, and full fuzz testing remain outstanding.
- Native Windows/MSYS2 normal tests passed before the final test-only assertion/cleanup edits; the final exact test state passed Linux `make ci`.

## Git status

The Sprint 1 commit and push are authorized by the user. Sprint 2 work begins only after Sprint 1 is committed and pushed successfully.
