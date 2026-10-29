#include <stdio.h>
#include <time.h>
#include "db.h"
#include "patient.h"

static int fails = 0;
#define CHECK(name, cond) do { printf("%s: %s\n", (cond) ? "PASS" : "FAIL", name); \
                               if (!(cond)) fails++; } while (0)

int main(void) {
    remove("t_patient.db"); remove("t_patient.db-wal"); remove("t_patient.db-shm");
    db_open("t_patient.db");
    db_init_schema("schema.sql");

    int doc = db_add_doctor(1, "Admin", "Dr. Mehta", "General", 500.0);
    int p = db_add_patient(1, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");

    printf("--- HMS-F-011 patient_search ---\n");
    CHECK("search by name",  patient_search("Asha") == 1);
    CHECK("search by phone", patient_search("9876543210") == 1);
    CHECK("search by id",   patient_search("1") == 1);
    CHECK("no match",       patient_search("Nobody") == 0);
    CHECK("% is literal, not wildcard", patient_search("%") == 0);
    CHECK("_ is literal, not wildcard", patient_search("_") == 0);

    for (int i = 0; i < 10000; i++) {
        char nm[32], ph[16];
        snprintf(nm, sizeof nm, "Bulk%d", i);
        snprintf(ph, sizeof ph, "8%09d", i);
        db_add_patient(1, "Receptionist", nm, 40, "M", ph, "X", "A+");
    }
    clock_t t0 = clock();
    int a = patient_search("8000005000");
    int b = patient_search("Bulk7777");
    double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    printf("2 searches over 10,000+ records: %.1f ms\n", ms);
    CHECK("searches under 2000 ms", a == 1 && b >= 1 && ms < 2000.0);

    printf("--- HMS-F-014 patient_archive ---\n");
    CHECK("non-admin refused", patient_archive(1, "Receptionist", p) == -3);
    CHECK("admin archives",    patient_archive(1, "Admin", p) == 0);
    CHECK("hidden from search", patient_search("Asha") == 0);
    CHECK("hidden from booking",
          db_book_appointment(1, p, doc, "2026-11-05", "10:00") == -1);

    db_close();
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}