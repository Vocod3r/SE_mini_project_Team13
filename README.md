# Hospital Management System — Team 13

This repository contains a C/SQLite Hospital Management System with a menu-driven console application, role-based login, service-layer entry points, and a central data-access layer. Requirement coverage and remaining SRS gaps are tracked in [`docs/REQUIREMENT_TRACEABILITY.md`](docs/REQUIREMENT_TRACEABILITY.md) and [`docs/PROJECT_AUDIT.md`](docs/PROJECT_AUDIT.md).

## Project documents

- [Software Requirements Specification](SRS_SE_miniproject_Team13%20(1).pdf)
- [Software Test Plan](HMS_Software_Test_Plan%20.pdf)
- [Component diagram](se_component_dia.jpeg)
- [Requirement traceability](docs/REQUIREMENT_TRACEABILITY.md)
- [Proposed sprint plan](docs/SPRINT_PLAN.md)
- [Test execution report](docs/TEST_EXECUTION_REPORT.md)

## Build and test

The project uses C11, GCC, GNU Make, and the SQLite3 development library. From the `src` directory, run:

```sh
make compile
make test
make ci
```

`make ci` runs compilation, the test executables, then a sanitizer build and test run. The checked-in GitHub Actions workflow runs this command on Ubuntu. The Makefile uses Unix shell commands and sanitizer flags; Windows build support has not yet been established.

## Run the console application

From `src`, run `make app` and then `./out/hms [database-path] [schema-path]`. Defaults are `hms.db` and `schema.sql`. On first run, the application creates the initial Admin account with a salted PBKDF2 password hash. Password entry is masked in an interactive terminal. The role-specific menus expose patient registration/search, appointment booking/schedule, clinical visit and prescription history, and Admin staff/doctor/backup/report workflows. Existing API and acceptance gaps remain listed in the traceability matrix.

The console uses `console.c` for terminal interaction, `app_service.c` for authenticated workflow entry points, and the existing modules and `db.c` for business operations and persistence. See [the test execution report](docs/TEST_EXECUTION_REPORT.md) for CI results, the first-run smoke walkthrough, performance measurements, and remaining limits.
