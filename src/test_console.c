/* Drives the real console (console_run) with scripted keystrokes and checks what it prints.
 * Covers the menu, input validation (SR-004), idle logout (SR-005) and the screens for
 * F-023/F-024/F-032/F-060 end to end. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "console.h"
#include "db.h"

static int failures;
#define CHECK(label, condition) do { \
    int ok_ = !!(condition); printf("%s: %s\n", ok_ ? "PASS" : "FAIL", label); \
    if (!ok_) failures++; \
} while (0)

static void remove_db(const char *path) {
    char sidecar[256]; remove(path);
    snprintf(sidecar, sizeof sidecar, "%s-wal", path); remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-shm", path); remove(sidecar);
}

static char screen[65536];

/* Runs the console on a fresh `keys` script; the screen text is left in `screen`. */
static int run_console(sqlite3 *db, const char *keys) {
    FILE *k = fopen("t_console_in.txt", "w");
    fputs(keys, k);
    fclose(k);
    if (!freopen("t_console_in.txt", "r", stdin)) return -99;
    fflush(stdout);
    int saved = dup(1);
    int fd = open("t_console_out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    dup2(fd, 1);
    close(fd);
    int rc = console_run(db);
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    FILE *o = fopen("t_console_out.txt", "r");
    size_t n = o ? fread(screen, 1, sizeof screen - 1, o) : 0;
    screen[n] = '\0';
    if (o) fclose(o);
    return rc;
}

static time_t g_now, g_step;
static time_t stepping_clock(void) { g_now += g_step; return g_now; }

int main(void) {
    sqlite3 *auth_db = NULL;
    remove_db("t_console.db");
    CHECK("open database", db_open("t_console.db") == 0 && db_init_schema("schema.sql") == 0 &&
          sqlite3_open("t_console.db", &auth_db) == SQLITE_OK);

    printf("--- admin sets up, receptionist works, admin reports ---\n");
    int rc = run_console(auth_db,
        "root\nStrongPass!42\n"                       /* first-run admin */
        "root\nStrongPass!42\n"                       /* sign in */
        "8\nDr. Mehta\nGeneral\n500\n"                /* add doctor */
        "9\ndesk\nDeskPass!42\nReceptionist\n"        /* staff */
        "9\ndrmehta\nDocPass!4242\nDoctor\n"
        "0\n"
        "desk\nDeskPass!42\n"                         /* receptionist session */
        "1\nAsha Rao\n30\nF\n9876543210\nChennai\nO+\n"      /* patient 1 */
        "1\nRavi Kumar\n45\nM\n9123456780\nPune\nA+\n"       /* patient 2 */
        "3\n1\n1\n2026-11-03\n10:00\n"                /* booked */
        "3\n2\n1\n2026-11-03\n09:30\n"
        "3\n1\n1\n2026-11-03\n10:00\n"                /* same slot again */
        "4\n1\nd\n2026-11-03\n"                       /* day schedule */
        "4\n1\nw\n2026-11-01\n"                       /* week schedule */
        "13\n1\n2026-11-03\n"                         /* queue */
        "14\n1\n"                                     /* slip */
        "14\n999\n"                                   /* unknown slip */
        "0\n"
        "root\nStrongPass!42\n"                       /* admin again for the report */
        "12\n2026-11-03\n"
        "0\nq\n");
    CHECK("console exits cleanly", rc == 0);
    CHECK("menu lists new options", strstr(screen, "13 Doctor patient queue") && strstr(screen, "14 Print appointment slip") &&
          strstr(screen, "(day or week)"));
    CHECK("doctor and staff created", strstr(screen, "Doctor ID: 1") && strstr(screen, "Staff account created."));
    CHECK("bookings confirmed with IDs", strstr(screen, "Appointment booked. Appointment ID: 1") &&
          strstr(screen, "Appointment booked. Appointment ID: 2"));
    CHECK("double booking explained in plain words", strstr(screen, "already booked at that time"));
    CHECK("daily schedule shows both patients", strstr(screen, "09:30 | Ravi Kumar | Booked") && strstr(screen, "10:00 | Asha Rao | Booked"));
    CHECK("weekly schedule shows dated rows", strstr(screen, "2026-11-03 | 09:30 | Ravi Kumar | Booked"));
    CHECK("queue lists patients in order", strstr(screen, "Patient queue for 2026-11-03") &&
          strstr(screen, " 1. 09:30  Ravi Kumar  (appt #2)") && strstr(screen, " 2. 10:00  Asha Rao  (appt #1)") &&
          strstr(screen, "Patients waiting: 2"));
    CHECK("slip printed", strstr(screen, "APPOINTMENT CONFIRMATION") && strstr(screen, "Patient        : Asha Rao (ID 1)"));
    CHECK("unknown slip explained", strstr(screen, "No appointment with that ID."));
    CHECK("daily report with summary", strstr(screen, "Daily appointment report for 2026-11-03") &&
          strstr(screen, "Total: 2 | Booked: 2 | Rescheduled: 0 | Completed: 0 | Cancelled: 0"));

    printf("--- bad input is refused kindly (SR-004) ---\n");
    rc = run_console(auth_db,
        "desk\nDeskPass!42\n"
        "1\nR2D2\n30\nF\n9876543210\nX\nO+\n"        /* bad name */
        "1\nAsha Rao\n200\nF\n9876543210\nX\nO+\n"   /* bad age */
        "1\nAsha Rao\n30\nF\n12345\nX\nO+\n"         /* bad phone */
        "3\n1\n1\n2026-02-30\n10:00\n"               /* bad date */
        "3\n1\n1\n2026-11-05\n25:99\n"               /* bad time */
        "4\n1\nd\ntomorrow\n2026-13-45\nnope\n"      /* three bad dates */
        "13\n1\nbad\nbad\nbad\n"
        "' OR 1=1; --\n"                             /* junk at the menu */
        "abc\n99\n"
        "0\nq\n");
    CHECK("console survives bad input", rc == 0);
    CHECK("bad name explained", strstr(screen, "Name must contain letters only"));
    CHECK("bad age explained", strstr(screen, "Age must be between 0 and 130"));
    CHECK("bad phone explained", strstr(screen, "Phone must be 10 to 13 digits"));
    CHECK("bad date explained", strstr(screen, "not a real date"));
    CHECK("bad time explained", strstr(screen, "not a valid time"));
    CHECK("date prompt re-asks with an example", strstr(screen, "Please enter a real date as YYYY-MM-DD, for example 2026-11-03."));
    CHECK("junk menu choices handled", strstr(screen, "Enter a valid menu number.") || strstr(screen, "cannot use that option"));

    printf("--- role limits ---\n");
    rc = run_console(auth_db,
        "desk\nDeskPass!42\n"
        "12\n"                                       /* receptionist: no report */
        "14\n1\n"                                     /* receptionist slip ok */
        "0\n"
        "drmehta\nDocPass!4242\n"
        "14\n"                                        /* doctor: no slip */
        "13\n1\n2026-11-03\n"                         /* doctor queue ok */
        "0\nq\n");
    CHECK("role session ends cleanly", rc == 0);
    CHECK("receptionist cannot open the report", strstr(screen, "This role cannot use that option."));
    CHECK("receptionist prints a slip", strstr(screen, "APPOINTMENT CONFIRMATION"));
    CHECK("doctor sees the queue", strstr(screen, "Patients waiting: 2"));

    printf("--- idle logout (SR-005) ---\n");
    g_now = 1700000000; g_step = 601;                 /* every clock read jumps past 10 minutes */
    console_set_clock(stepping_clock);
    rc = run_console(auth_db,
        "root\nStrongPass!42\n"
        "1\nShould Never Be Added\n30\nF\n9876543210\nX\nO+\n"
        "q\n");
    CHECK("idle user is logged out with a clear message", rc == 0 &&
          strstr(screen, "logged out after 10 minutes of inactivity"));
    CHECK("command typed after the timeout is thrown away", !strstr(screen, "Patient result"));
    g_now = 1700000000; g_step = 100;                 /* active user: clock moves slowly */
    rc = run_console(auth_db, "root\nStrongPass!42\n0\nq\n");
    CHECK("active user is not logged out", rc == 0 && !strstr(screen, "logged out after 10 minutes"));
    console_set_clock(NULL);

    if (auth_db) sqlite3_close(auth_db);
    db_close(); remove_db("t_console.db"); remove("t_console_in.txt"); remove("t_console_out.txt");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
