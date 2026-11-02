# Sprint 7 Completion Report

**Sprint:** 7 — Audit logging and protection
**Branch:** `feat/sprint-1-core`
**Status:** Implemented, verified, and committed as a dedicated Sprint 7 changeset.
**Requirement authority:** Original SRS and Software Test Plan; HMS-SR-003, AC-SR-003, TC-Sec-03.

## Changes

- Added Doctor/Admin prescription update and delete APIs. Each change checks the persisted role and writes a timestamped audit event in the same transaction as the data change.
- Hardened doctor, staff DAL, and billing create/update/delete paths so audit insertion or transaction completion failure rolls back the data change.
- Added schema initialization guards that reject `UPDATE` and `DELETE` statements on `audit_log`, including for databases that already contain audit rows.
- Added an isolated audit test covering legacy-row preservation, event actor/action/record/timestamp fields, prescription role checks and create/update/delete events, data rollback after failed audit writes, and DML tamper rejection.

## Verification

- Ubuntu 24.04 Docker `make ci` exited **0**. All eleven test executables passed in standard and ASan/UBSan runs. No sanitizer findings or compiler warnings were reported.
- `git diff --check` passed.

## Protection boundary and remaining gaps

SQLite triggers reject routine audit-row changes while the guards remain installed. A person with direct write access to the database file or schema can drop the triggers; the local embedded database cannot protect itself from its file owner. This is not a tamper-proof external audit archive. Read-access events, comprehensive audit of authentication state changes, and prescription edit/delete console actions remain open.

## Files changed

- `src/schema.sql`, `src/db.c`, `src/db.h`, `src/test_audit.c`
- `docs/PROJECT_AUDIT.md`, `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_7_COMPLETION_REPORT.md`
