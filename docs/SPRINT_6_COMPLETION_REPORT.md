# Sprint 6 Completion Report

**Sprint:** 6 — Modular architecture and console workflows
**Branch:** `feat/sprint-1-core`
**Status:** Implemented and verified; ready for its dedicated commit and push.
**Requirement authority:** Original SRS and Software Test Plan; HMS-NF-006, HMS-NF-003 and related functional requirements.

## Changes

- Added a runnable `hms` application with configurable database and schema paths.
- Added a role-based console for Admin, Doctor, and Receptionist workflows covering patient registration/search, appointment booking and schedule viewing, visit/prescription recording and history, doctor listing/creation, staff account creation, backup/restore, and daily report/health check.
- Added a service module that binds authenticated user identity to patient, appointment, clinical, and staff workflow entry points. Existing modules and `db.c` remain the persistence and business-operation layer.
- Added one-time first-Admin creation using a random salt and PBKDF2-HMAC-SHA256. Interactive terminal password entry is masked. A separate bootstrap attempt is rejected after the first staff account exists.
- Updated the Makefile so `make compile` builds the app as well as test executables; `make app` builds the application.

## Verification

- `make ci` exited **0** on Ubuntu 24.04. All ten test executables passed in both the standard and ASan/UBSan runs. The new `test_app_service` covers first-Admin creation, repeat-bootstrap rejection, authentication, Admin-created accounts, patient registration, role-limited appointment booking, and prescription recording. No sanitizer errors or compiler warnings were reported.
- Fresh-database CLI smoke walkthrough passed: first Admin bootstrap, Admin login, role menu display, sign-out, and quit.
- `git diff --check` passed.

## Remaining acceptance gaps

- Menu training/usability acceptance has not been measured with nontechnical staff.
- Several existing APIs are not yet exposed in menus, including patient updates/archive, appointment cancellation/rescheduling, billing, and some report variants.
- Console code still calls a few domain APIs directly; the dependency boundary is improved but not fully inverted.
- Doctor accounts are not mapped to individual doctor records, so doctor-specific ownership cannot be enforced end-to-end.
- Windows-native build/runtime and idle-session timeout remain unverified/unimplemented. The optional LAN client/server is not part of this console delivery.

## Files changed

- `src/Makefile`, `src/main.c`, `src/console.c`, `src/console.h`, `src/app_service.c`, `src/app_service.h`
- `src/auth.c`, `src/auth.h`, `src/test_app_service.c`
- `README.md`, `docs/PROJECT_AUDIT.md`, `docs/REQUIREMENT_TRACEABILITY.md`, `docs/SPRINT_PLAN.md`, `docs/TEST_EXECUTION_REPORT.md`, `docs/SPRINT_6_COMPLETION_REPORT.md`
