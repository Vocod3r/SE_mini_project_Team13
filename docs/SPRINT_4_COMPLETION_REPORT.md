# Sprint 4 Completion Report

**Sprint:** 4 — Performance optimization (HMS-NF-001)
**Status:** Implemented and verified; ready for the dedicated Sprint 4 commit and push.
**Branch:** `feat/sprint-1-core`
**Requirement authority:** Original SRS and Software Test Plan; HMS-NF-001, AC-NF-001, TC-Perf-01.

## Changes

- Added authenticated structured patient lookup by ID, name, and phone. The query separates exact ID and phone branches from substring name matching. It reuses the existing patient indexes and verifies the persisted staff role with one authorization query.
- Added authenticated structured appointment lookup by appointment ID and doctor/date. The doctor/date path uses the existing `idx_appt_doc_date`; the appointment-ID path uses the primary key.
- Extended the isolated performance fixture to create exactly 10,000 patients and 10,000 appointments. It performs 50 repetitions of each of five lookup patterns and checks every result as well as the two-second aggregate threshold.
- Switched the performance measurements to monotonic wall-clock time. The test checks the SQLite query plan for use of the doctor/date index. No additional indexes were added without evidence they were needed.
- Fixed the performance test's assertion macro to evaluate each condition only once; this avoids duplicate database operations and leaked statements in assertions with side effects.

## Measured results

Each value below is elapsed time for 50 lookups. All five results in both runs are below the 2-second threshold.

| Lookup | Standard run | ASan/UBSan run |
|---|---:|---:|
| Patient ID | 111.81 ms | 149.40 ms |
| Patient name substring | 135.24 ms | 109.10 ms |
| Patient phone | 126.01 ms | 93.11 ms |
| Appointment ID | 86.40 ms | 42.42 ms |
| Doctor/date schedule | 72.03 ms | 41.37 ms |

The standard measurements were captured by a standalone run of the exact performance fixture. The sanitizer measurements came from the final full `make ci` run. Timings are specific to the Docker test host and are not a target-hardware guarantee.

## Verification

The full Ubuntu 24.04 Docker `make ci` pipeline exited **0**. All eight test executables passed in both standard and ASan/UBSan runs. It reported no compiler warnings, sanitizer errors, or test failures. The fixture also confirmed exactly 10,000 patient rows and 10,000 appointment rows.

## Limits

Substring name search scans the patient table; the measured 10,000-row workload remains comfortably within the SRS threshold. The menu-driven application integration is assigned to Sprint 6.

## Files changed

- `src/db.c`, `src/db.h`
- `src/test_core.c`, `src/test_patient.c`
- `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_4_COMPLETION_REPORT.md`
