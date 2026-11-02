# Test Execution Report

## Sprint 1 final run

**Date:** 2026-11-02
**Branch:** `feat/sprint-1-core`
**Final CI command:**

```powershell
docker run --rm -v D:/SE_MINI_PROJECT:/workspace -w /workspace/src ubuntu:24.04 bash -lc "apt-get update -qq && apt-get install -y -qq build-essential libsqlite3-dev libssl-dev && make ci"
```

**Result:** Exit code **0**. `make ci` compiled and ran all eight test executables, then cleaned/rebuilt them with AddressSanitizer and UndefinedBehaviorSanitizer and ran all eight again. No compiler warnings, test failures, AddressSanitizer findings, or UndefinedBehaviorSanitizer findings were reported.

The final performance fixture asserted exactly 10,000 patient rows. In the standard test pass, its two patient searches completed in **12.6 ms combined**. In the sanitizer pass, they completed in **13.1 ms combined**. Both measured results are below the 2-second acceptance threshold. The timing covers a patient phone lookup and a patient name lookup; appointment lookup latency was not measured.

## Test executables

| Executable | Verified scenarios | Result |
|---|---|---|
| `out/test_appt_view` | Day/week schedule, date validation, appointment slip, role gates | Passed |
| `out/test_auth` | Legacy account migration/upgrade, PBKDF2 salt/hash, account roles, injection-shaped username, lockout/audit, audit failure rollback | Passed |
| `out/test_clinical` | Visit and prescription creation, history access, billing, payment, archive history preservation | Passed |
| `out/test_compliance` | SQLite quick check and role menu option validation | Passed |
| `out/test_core` | Canonical schema, existing doctor-record preservation, doctor SQL binding, persisted-role checks, transactional audit rollback, F-041 chronology/empty history, backup integrity/data/schema checks | Passed |
| `out/test_db` | Print-based in-memory database smoke workflow, backup and health check | Exit code 0; assertions are primarily printed expectations |
| `out/test_db2` | Print-based account/patient/appointment/consultation/billing/doctor workflow, plus an asserted stored-role spoof rejection | Passed |
| `out/test_patient` | Patient registration/search/archive and exactly 10,000 patient rows with timed phone/name lookups | Passed |

`make ci` ran the normal and sanitizer passes. Its `make clean` step removed only `src/out`; generated DB files were not used as persistent fixtures. The updated tests clean their own temporary database files.

## Requirement-specific coverage

- **TC-Rx-02 / HMS-F-041 / AC-041:** two prescriptions with deliberately reversed visit dates are returned oldest-to-newest; no-history behavior is also asserted.
- **TC-Rep-02 / HMS-F-061 / AC-061:** test creates a backup, opens the backup as a separate SQLite database, compares patient/schema data, runs integrity validation, and checks failure for a destination under a nonexistent directory.
- **TC-Perf-01 / HMS-NF-001 / AC-NF-001:** exactly 10,000 patient records are confirmed before timing two patient searches; each combined run is below 2 seconds.
- **TC-Sec-03 / HMS-SR-003 / AC-SR-003:** Sprint 7 verifies timestamp, actor, action, and record IDs for clinical writes, create/update/delete audit outcomes, failure rollback, and append-only DML guards in test_audit and test_core.
- **TC-Sec-01/02/04:** authentication checks salted PBKDF2 storage, legacy migration, authorization against the persisted staff role, and SQL-safe account lookup. Full fuzz testing is not performed.

## Platform and execution limits

- The full sanitizer pipeline was executed in Ubuntu 24.04 via Docker. Windows/MSYS2 ran the normal suite earlier, before the final exact-10,000-row assertion and cleanup-only test edits; the final exact test state was verified by Linux `make ci`.
- Windows-native sanitizer execution was not available because the local MinGW toolchain lacks ASan/UBSan libraries.
- Performance evidence covers patient lookup only, not appointment lookup or repeated 50-iteration load.
- Crash/power-loss recovery, cross-platform Windows/Linux parity, full fuzzing and comprehensive read-access auditing remain unverified; Sprint 7 verifies DML tamper guards, while direct schema/file-owner tampering remains possible.
- `test_db.c` remains a print-based smoke test; its exit code alone does not validate each printed expectation.

## Sprint 2 final verification

On 2026-11-02, after the prescription-history authorization and long-history tests were added, the same Ubuntu 24.04 Docker make ci pipeline exited **0**. All eight test executables passed in both the standard and ASan/UBSan passes. Added TC-Rx-02 checks cover persisted Doctor/Admin identity, forged-role rejection, invalid IDs, empty history, visit-date order, all available prescription detail fields, insufficient result capacity, and 66 prescription rows. The patient lookup timing from that later run is not reported here because Sprint 2 made no performance changes and the run was for regression validation.

## Sprint 3 final verification

On 2026-11-02, after Admin-only backup/restore, exclusive destination creation, and restoration tests were added, the Ubuntu 24.04 Docker make ci pipeline exited **0**. All eight test executables passed in both standard and ASan/UBSan runs. TC-Rep-02 checks backup/restore integrity and content preservation, persisted Admin authorization, invalid/missing sources and paths, and refusal to overwrite existing destinations. A corrupt source is rejected before restore output is created.

## Sprint 4 final verification

The final Sprint 4 full Ubuntu 24.04 Docker make ci run exited **0**. All eight executables passed in both the standard and ASan/UBSan runs; no compiler warnings or sanitizer findings were reported. The performance fixture asserted exactly 10,000 patients and appointments and ran 50 repetitions for each of five lookup types. Standard and sanitizer timings are recorded in SPRINT_4_COMPLETION_REPORT.md; every 50-lookup aggregate was below 2 seconds. git diff --check passed.

## Sprint 5 final verification

The Ubuntu 24.04 Docker make ci run exited **0** with nine test executables. All nine passed in both standard and ASan/UBSan runs. test_reliability verifies WAL/FULL setup, child-process exit during an open transaction, survival of committed rows, rollback of the interrupted row, and post-recovery integrity. Its POSIX harness ran on Ubuntu; the Windows branch is an explicit skip and is not counted as a pass. No sanitizer findings or compiler warnings were reported.


## Sprint 6 final verification

The Ubuntu 24.04 Docker `make ci` run exited **0** with ten test executables. All ten passed in both standard and ASan/UBSan runs, including the new app-service/bootstrap test; no sanitizer findings or compiler warnings were reported. A fresh-database console smoke walkthrough completed first-Admin bootstrap, Admin login, role menu display, sign-out, and quit. `git diff --check` passed. The smoke test used piped input and therefore verifies workflow behavior, not interactive password-echo suppression; masked entry is implemented for terminal sessions. See `SPRINT_6_COMPLETION_REPORT.md` for remaining acceptance gaps.


## Sprint 7 final verification

The Ubuntu 24.04 Docker `make ci` run exited **0** with eleven test executables. All eleven passed in both standard and ASan/UBSan runs, including `test_audit`; there were no sanitizer findings or compiler warnings. The audit tests verify preservation of a pre-existing audit row during schema initialization, timestamp/user/action records, rollback of data changes when audit inserts fail, Doctor/Admin authorization for prescription changes, and rejection of audit-row UPDATE/DELETE statements. These SQLite triggers are an application-level DML guard and do not prevent a database file/schema owner from dropping them. `git diff --check` passed before staging.
