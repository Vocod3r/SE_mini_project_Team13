# Hospital Management System — Team 13

This repository contains the C/SQLite implementation and project documents for the HMS mini-project. The current checkout is an in-progress data-access prototype; the repository audit and requirement status are in [`docs/PROJECT_AUDIT.md`](docs/PROJECT_AUDIT.md).

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

The Sprint 1 Linux CI run completed successfully, including the sanitizer pass. See [the test execution report](docs/TEST_EXECUTION_REPORT.md) for the exact command, results, measured lookups, and remaining test limits.
