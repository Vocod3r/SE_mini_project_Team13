#include <stdio.h>
#include "db.h"
#include "consult.h"
#include "patient.h"

static int fails = 0;
#define CHECK(name, cond) do { printf("%s: %s\n", (cond) ? "PASS" : "FAIL", name); \
                               if (!(cond)) fails++; } while (0)

int main(void) {
    remove("t_clin.db"); remove("t_clin.db-wal"); remove("t_clin.db-shm");
    db_open("t_clin.db");
    db_init_schema("schema.sql");

    int doc = db_add_doctor(1, "Admin", "Dr. Mehta", "General", 500.0);
    int p = db_add_patient(1, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");

    int a1 = db_book_appointment(1, p, doc, "2026-11-03", "10:00");
    int a2 = db_book_appointment(1, p, doc, "2026-11-04", "10:00");
    int v1 = db_add_visit_and_rx(1, "Doctor", a1, "Fever", "Paracetamol", "500mg", "3 days");
    int v2 = db_add_visit_and_rx(1, "Doctor", a2, "Cough", "Syrup", "5ml", "5 days");

    printf("--- HMS-F-013 consult_history ---\n");
    CHECK("history has 2 visits", consult_history("Doctor", p) == 2);
    CHECK("admin allowed",        consult_history("Admin", p) == 2);
    CHECK("receptionist refused", consult_history("Receptionist", p) == -3);

    printf("--- HMS-F-041 consult_prescriptions ---\n");
    CHECK("2 prescriptions", consult_prescriptions("Doctor", p) == 2);
    CHECK("receptionist refused", consult_prescriptions("Receptionist", p) == -3);

    printf("--- e2e: register -> book -> consult -> bill -> pay ---\n");
    int bill = db_create_bill(1, "Receptionist", v2);
    CHECK("visit ids valid", v1 > 0 && v2 > 0);
    CHECK("bill created", bill > 0);
    CHECK("bill marked paid", db_set_payment_status(1, "Receptionist", bill, "Paid") == 0);
    CHECK("archive keeps history",
          patient_archive(1, "Admin", p) == 0 && consult_history("Admin", p) == 2);

    db_close();
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}