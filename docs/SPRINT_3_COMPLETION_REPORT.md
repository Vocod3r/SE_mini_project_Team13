# Sprint 3 Completion Report

**Sprint:** 3 — Patient database backup (HMS-F-061)
**Status:** Implemented and verified; ready for the dedicated Sprint 3 commit and push.
**Branch:** `feat/sprint-1-core`
**Requirement authority:** Original SRS and Software Test Plan; HMS-F-061, AC-061, TC-Rep-02.

## Changes

- Restricted backup and restore entry points to a persisted Admin identity; a caller cannot claim Admin using a different account's ID.
- Backups use SQLite's online backup API, validate `integrity_check`, and require a newly created destination file. Existing files are not truncated.
- Added restore-to-new-database support. It validates the source before creating the destination, copies into a separate database, validates the resulting database, and removes only a failed output created by that call.
- Extended isolated tests for admin authorization, invalid and missing destinations, preservation of existing files, backup schema/data, restore schema/data/integrity, and rejection of a corrupt source without leaving an output file.

## Verification

The complete Ubuntu 24.04 Docker `make ci` pipeline exited **0**. All eight test executables passed in the standard and ASan/UBSan runs. Backup and restoration scenarios are asserted in `out/test_core`; the existing `out/test_db` backup smoke workflow also passed.

## Limits

The Admin-facing menu and path prompts are part of Sprint 6's console application. The API creates separate backup/restore files but does not implement a destructive in-place restore.

## Files changed

- `src/db.c`, `src/db.h`
- `src/test_core.c`, `src/test_db.c`
- `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_3_COMPLETION_REPORT.md`
