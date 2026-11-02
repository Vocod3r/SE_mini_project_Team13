# Sprint 5 Completion Report

**Sprint:** 5 — Transactional reliability (HMS-NF-004)
**Status:** Implemented and verified; ready for the dedicated Sprint 5 commit and push.
**Branch:** `feat/sprint-1-core`
**Requirement authority:** Original SRS and Software Test Plan; HMS-NF-004, AC-NF-004, TC-Rel-01.

## Changes

- Hardened `db_open`: rejects empty paths and an already-open global connection, closes and clears a handle after open/configuration failure, and checks foreign-key, WAL, and `synchronous=FULL` configuration results.
- Added an isolated process-interruption test. A child opens a disposable WAL database, commits a patient, starts a second transaction and inserts another patient, then exits via `_exit` without SQLite cleanup or commit.
- After the child exits, the parent reopens the database and verifies the original committed patient and the child's committed patient remain, the in-flight patient is absent, and `PRAGMA integrity_check` returns `ok`.

## Verification

The full Ubuntu 24.04 Docker `make ci` pipeline exited **0**. All nine test executables passed in both the standard and ASan/UBSan runs, including `test_reliability`. No sanitizer findings or compiler warnings were reported. Test databases and sidecars are removed by the fixture.

## Platform limits

The process-interruption harness uses POSIX `fork` and `_exit`. Its Windows build branch prints `SKIP` and exits successfully so the portable test suite can still run, but that is not counted as a Windows reliability pass. A native Windows interruption harness remains open. This evidence does not prove behavior under actual power loss or cover every mutator.

## Files changed

- `src/db.c`
- `src/test_reliability.c`
- `docs/PROJECT_AUDIT.md`, `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_5_COMPLETION_REPORT.md`
