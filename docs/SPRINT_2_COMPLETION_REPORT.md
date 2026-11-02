# Sprint 2 Completion Report

**Sprint:** 2 — Prescription history (HMS-F-041)
**Status:** Implemented and verified; ready for the dedicated Sprint 2 commit and push.
**Branch:** `feat/sprint-1-core`
**Requirement authority:** Original SRS and Software Test Plan. HMS-F-040 is prescription recording; HMS-F-041 is history retrieval; AC-041 requires chronological listing; TC-Rx-02 verifies retrieval.

## Changes

- Reused the existing chronological query and strengthened the retrieval path to require a staff ID and claimed role, then verify that role against the persisted `staff` row. Doctor and Admin are allowed; Receptionist and forged role claims are rejected.
- Added count-then-fetch semantics so callers can allocate for the full history without an arbitrary production limit. Too-small buffers and invalid patient IDs fail clearly; a valid patient with no history returns zero.
- Updated the consultation history display to show visit date, medicines, dosage, and notes for every prescription in chronological order.
- Expanded acceptance tests for all available fields, order, empty history, invalid IDs, role spoofing, buffer handling, and retrieval of 66 historical prescriptions.

## Verification

The complete `make ci` pipeline ran in Ubuntu 24.04 via Docker and exited **0**. All eight test executables passed in both the standard and ASan/UBSan runs. Focused evidence is in `out/test_core` and `out/test_clinical`; the complete per-test inventory is in `TEST_EXECUTION_REPORT.md`.

## Limits

The standalone login and patient-selection menu is not yet integrated; that application workflow is planned for Sprint 6. Existing schema fields do not separately store prescribing clinician, frequency, or duration. Retrieval and role validation are implemented at the DAL boundary.

## Files changed

- `src/db.c`, `src/db.h`
- `src/consult.c`, `src/consult.h`
- `src/test_core.c`, `src/test_clinical.c`
- `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/SPRINT_2_COMPLETION_REPORT.md`
