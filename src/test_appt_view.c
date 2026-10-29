#include <stdio.h>
#include "db.h"
#include "appt_view.h"

static int fails = 0;
#define CHECK(name, cond) do { printf("%s: %s\n", (cond) ? "PASS" : "FAIL", name); \
                               if (!(cond)) fails++; } while (0)

int main(void) {
    remove("t_view.db"); remove("t_view.db-wal"); remove("t_view.db-shm");
    db_open("t_view.db");
    db_init_schema("schema.sql");

    int d1 = db_add_doctor(1, "Admin", "Dr. Mehta", "General", 500.0);
    int d2 = db_add_doctor(1, "Admin", "Dr. Iyer", "ENT", 400.0);
    int p1 = db_add_patient(1, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");
    int p2 = db_add_patient(1, "Receptionist", "Ravi Kumar", 45, "M", "9123456780", "Pune", "A+");
    int a1 = db_book_appointment(1, p1, d1, "2026-11-03", "10:00");
    int a2 = db_book_appointment(1, p2, d1, "2026-11-03", "09:30");
    int a3 = db_book_appointment(1, p1, d1, "2026-11-05", "11:00");
    int a4 = db_book_appointment(1, p2, d1, "2026-11-10", "09:00"); /* day 8: outside the week */
    int a5 = db_book_appointment(1, p2, d2, "2026-11-03", "09:00"); /* other doctor */
    CHECK("setup ok", d1 > 0 && d2 > 0 && p1 > 0 && p2 > 0 && a1 > 0 && a2 > 0 && a3 > 0 && a4 > 0 && a5 > 0);

    printf("--- HMS-F-023 daily schedule ---\n");
    CHECK("day: 2 rows for Dr. Mehta", appt_schedule_day("Receptionist", d1, "2026-11-03") == 2);
    CHECK("day: empty day = 0", appt_schedule_day("Doctor", d1, "2026-11-04") == 0);
    CHECK("day: other doctor only sees own", appt_schedule_day("Admin", d2, "2026-11-03") == 1);
    CHECK("day: null date rejected", appt_schedule_day("Admin", d1, NULL) == -1);

    printf("--- HMS-F-023 weekly schedule ---\n");
    CHECK("week: 3 rows (day 8 excluded)", appt_schedule_week("Receptionist", d1, "2026-11-03") == 3);
    CHECK("week: starting 11-04 covers 11-04..11-10 (2 rows)", appt_schedule_week("Admin", d1, "2026-11-04") == 2);
    CHECK("week: 7th day (start+6) included, day 8 not", appt_schedule_week("Admin", d1, "2026-11-04") == 2 &&
          appt_schedule_week("Admin", d1, "2026-11-11") == 0);
    CHECK("week: other doctor sees own rows only", appt_schedule_week("Admin", d2, "2026-11-03") == 1);
    CHECK("week: bad date rejected", appt_schedule_week("Admin", d1, "not-a-date") == -1 &&
          appt_schedule_week("Admin", d1, "2026-13-45") == -1);
    CHECK("week: null date rejected", appt_schedule_week("Admin", d1, NULL) == -1);

    printf("--- role checks ---\n");
    CHECK("schedule: unknown role refused", appt_schedule_day("Patient", d1, "2026-11-03") == -3);
    CHECK("schedule: NULL role refused", appt_schedule_week(NULL, d1, "2026-11-03") == -3);

    printf("--- HMS-F-024 confirmation slip ---\n");
    CHECK("slip: receptionist ok", appt_print_slip("Receptionist", a1) == 0);
    CHECK("slip: admin ok", appt_print_slip("Admin", a2) == 0);
    CHECK("slip: doctor refused", appt_print_slip("Doctor", a1) == -3);
    CHECK("slip: unknown appointment", appt_print_slip("Admin", 9999) == -1);
    db_cancel_appointment(1, a3);
    CHECK("slip: cancelled appointment not issued", appt_print_slip("Admin", a3) == -2);
    db_reschedule_appointment(1, a1, "2026-11-04", "10:00");
    CHECK("slip: rescheduled shows new slot", appt_print_slip("Admin", a1) == 0);

    db_close();
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
