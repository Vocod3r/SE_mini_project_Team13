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

    int admin_id = db_create_staff(0, "Admin", "admin", "testhash", "testsalt", "Admin");
    int receptionist_id = db_create_staff(admin_id, "Admin", "reception", "testhash", "testsalt", "Receptionist");
    int doctor_user_id = db_create_staff(admin_id, "Admin", "doctor", "testhash", "testsalt", "Doctor");
    int doc = db_add_doctor(admin_id, "Admin", "Dr. Mehta", "General", 500.0);
    int p = db_add_patient(receptionist_id, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");

    int a1 = db_book_appointment(receptionist_id, p, doc, "2026-11-03", "10:00");
    int a2 = db_book_appointment(receptionist_id, p, doc, "2026-11-04", "10:00");
    int v1 = db_add_visit_and_rx(doctor_user_id, "Doctor", a1, "Fever", "Paracetamol", "500mg", "3 days");
    int v2 = db_add_visit_and_rx(doctor_user_id, "Doctor", a2, "Cough", "Syrup", "5ml", "5 days");

    printf("--- HMS-F-013 consult_history ---\n");
    CHECK("history has 2 visits", consult_history("Doctor", p) == 2);
    CHECK("admin allowed",        consult_history("Admin", p) == 2);
    CHECK("receptionist refused", consult_history("Receptionist", p) == -3);

    printf("--- HMS-F-041 consult_prescriptions ---\n");
    CHECK("2 prescriptions", consult_prescriptions(doctor_user_id, "Doctor", p) == 2);
    CHECK("receptionist refused", consult_prescriptions(receptionist_id, "Receptionist", p) == -3);
    CHECK("persisted receptionist role cannot be claimed as Doctor",
          consult_prescriptions(receptionist_id, "Doctor", p) == -3);

    printf("--- e2e: register -> book -> consult -> bill -> pay ---\n");
    int bill = db_create_bill(receptionist_id, "Receptionist", v2);
    CHECK("visit ids valid", v1 > 0 && v2 > 0);
    CHECK("bill created", bill > 0);
    CHECK("bill marked paid", db_set_payment_status(receptionist_id, "Receptionist", bill, "Paid") == 0);
    CHECK("archive keeps history",
          patient_archive(admin_id, "Admin", p) == 0 && consult_history("Admin", p) == 2);

    db_close();
    remove("t_clin.db"); remove("t_clin.db-wal"); remove("t_clin.db-shm");
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
