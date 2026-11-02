# Sprint Plan and Status

## Planning basis

The repository contains no authoritative earlier sprint allocations or sprint completion records. The user authorized Sprint 1 completion and then provided the following explicit seven-sprint sequence. The original SRS and Software Test Plan remain authoritative. Features already implemented and tested in Sprint 1 are reused in their relevant requirement rows; no duplicate implementation is planned.

## Status

- Sprint 1: committed and pushed as `1cedd6b7655ef6ed44b33829ee808b2675706036` on `feat/sprint-1-core`.
- Sprint 2: implementation and verification complete; dedicated commit and push are next.
- Sprints 3–7: not started. Each receives a separate implementation or completion-evidence commit and push after its full suite passes.
- The prescription mapping is HMS-F-040 recording, HMS-F-041 retrieval, AC-041 chronological order, and TC-Rx-02 retrieval test.

## Sprint 1 — Foundation

**Scope:** Canonical database schema and doctor manager correction; persisted staff authentication with PBKDF2 and legacy migration; selected persisted-role checks; checked transactions for patient, appointment, consultation/prescription, and account writes; chronological prescription DAL retrieval; backup integrity checks; isolated tests and Linux sanitizer CI.

**Evidence:** `SPRINT_1_COMPLETION_REPORT.md` and `TEST_EXECUTION_REPORT.md`. The latest CI pass covered eight executables in normal and ASan/UBSan modes. It measured phone/name patient search against exactly 10,000 rows; appointment lookup performance remains for Sprint 4.

## Sprint 2 — Prescription history (implemented; awaiting dedicated commit/push)

**Requirements:** HMS-F-041, AC-041, TC-Rx-02; also role-safe patient history access under HMS-F-013 and HMS-SR-002.

**Existing work to reuse:** Sprint 1 added structured chronological prescription retrieval and tests for reversed dates and empty history. Do not duplicate that DAL functionality.

**Delivered:** Identity-based Doctor/Admin history retrieval checks the persisted staff role at the DAL, counts before allocating so all rows can be returned, and prints visit date, medicines, dosage, and notes chronologically. Tests cover invalid IDs, no history, forged role, field contents, capacity errors, and 66 prescriptions. The patient-selection/login menu remains in Sprint 6. See `SPRINT_2_COMPLETION_REPORT.md`.

## Sprint 3 — Patient database backup

**Requirements:** HMS-F-061, AC-061, TC-Rep-02.

**Existing work to reuse:** Sprint 1 added SQLite online backup completion checks, integrity validation, and isolated backup-open/data/schema tests.

**Remaining work:** Add an Admin-controlled workflow, safe destination handling and clear failure reporting, plus a restore operation into a separate database path. Test restoration and incomplete/invalid backups without touching an operational database.

## Sprint 4 — Performance

**Requirements:** HMS-NF-001, AC-NF-001, TC-Perf-01.

**Existing work to reuse:** Sprint 1 seeded and asserted exactly 10,000 patients and measured phone/name searches under two seconds.

**Remaining work:** Add a timed appointment lookup query against a fixture with 10,000 patients and appointments; measure the specified ID/name/phone and appointment lookups repeatedly, report actual individual and aggregate timings, and add indexes only when query plans/evidence support them.

## Sprint 5 — Transactional reliability

**Requirements:** HMS-NF-004, AC-NF-004, TC-Rel-01.

**Existing work to reuse:** Checked transactions and audit-failure rollback tests for key writes.

**Remaining work:** Use a child process and disposable databases to interrupt writes at controlled points, reopen and integrity-check the DB, and demonstrate committed records survive while incomplete writes are absent. Never use the operational/project database for destructive tests.

## Sprint 6 — Modular architecture and application workflows

**Requirements:** HMS-NF-006, AC-NF-006, TC-Maint-01; remaining console/UI acceptance criteria across the functional requirements.

**Remaining work:** Add the specified menu-driven console application, role-specific workflows, and explicit UI/business/data-access boundaries. Refactor incrementally, preserve existing DAL behavior, and test integrated workflows. LAN client/server remains optional per the SRS and is not assumed in scope.

## Sprint 7 — Audit logging and protection

**Requirements:** HMS-SR-003, AC-SR-003, TC-Sec-03; associated patient/appointment/prescription requirements.

**Existing work to reuse:** Sprint 1 writes timestamped audit entries for required patient, appointment, and prescription operations and tests rollback when required audit inserts fail.

**Remaining work:** Add prescription update/delete operations if required by the use cases; verify every required create/update/delete produces record type, ID, action, timestamp, and authenticated user ID; define and test a practical protection against unauthorized audit modification. Include other SRS audit scope where required.

## Other SRS requirements and release gate

Work in each sprint must also close any remaining functional, non-functional, security-objective, or security-requirement gaps identified by `REQUIREMENT_TRACEABILITY.md`; the seven named sprint topics do not narrow the SRS scope. Do not claim requirements complete without acceptance evidence. Final release requires the SRS Section 6 gates: High-priority functional tests, all five security requirements, NF-001, NF-004, and Windows/Linux build evidence. The test plan also calls for integration, regression, fuzz/input-boundary, and role workflow validation.

## Git workflow

Before each sprint push: run the complete applicable suite and sanitizer pipeline where supported, update actual test evidence, inspect the staged diff, commit only that sprint, verify the commit hash, push without force, and verify the remote branch. Sprint 1 must push successfully before Sprint 2 begins. The user has authorized separate pushes without per-sprint confirmation.
