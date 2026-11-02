#include <stdio.h>
#include "db.h"

static int failures;
#define CHECK(label, condition) do { \
    int ok_ = !!(condition); \
    printf("%s: %s\n", ok_ ? "PASS" : "FAIL", label); \
    if (!ok_) failures++; \
} while (0)

int main(void) {
    remove("t_db2.db"); remove("t_db2.db-wal"); remove("t_db2.db-shm");
    db_open("t_db2.db");
    db_init_schema("schema.sql");

    /* ----- staff and lockout (F-003, F-031) ----- */
    int admin_id = db_create_staff(0, "Admin", "bootstrap-admin", "HASHADMIN", "SALTADMIN", "Admin");
    printf("bootstrap admin = %d (expect > 0)\n", admin_id);
    printf("create staff = %d (expect > 0)\n",
           db_create_staff(admin_id, "Admin", "reception1", "HASH123", "SALT123", "Receptionist"));
    Staff receptionist_staff;
    int receptionist_id = db_get_staff("reception1", &receptionist_staff) == 0
        ? receptionist_staff.staff_id : -1;
    int doctor_user_id = db_create_staff(admin_id, "Admin", "doctor1", "HASHDOCTOR", "SALTDOCTOR", "Doctor");
    printf("duplicate staff = %d (expect -2)\n",
           db_create_staff(admin_id, "Admin", "reception1", "H", "S", "Receptionist"));
    printf("staff by non-admin = %d (expect -3)\n",
           db_create_staff(admin_id, "Doctor", "hacker", "H", "S", "Admin"));
    CHECK("staff role cannot be spoofed by a non-admin user",
          receptionist_id > 0 && db_create_staff(receptionist_id, "Admin", "hacker2", "H", "S", "Admin") == -3);
    Staff s;
    int g = db_get_staff("reception1", &s);
    printf("get staff = %d (expect 0), role = %s\n", g, s.role);

    int f1 = db_record_failed_login("reception1");
    int f2 = db_record_failed_login("reception1");
    int f3 = db_record_failed_login("reception1");
    printf("fail 1 = %d, fail 2 = %d, fail 3 = %d (expect 0, 0, 1)\n", f1, f2, f3);

    int f4 = db_record_failed_login("reception1");
    int lk = db_is_locked("reception1");
    printf("4th attempt = %d (expect -2), locked = %d (expect 1)\n", f4, lk);

    db_reset_failed_login("reception1");
    printf("locked after reset = %d (expect 0)\n", db_is_locked("reception1"));

    int doc = db_add_doctor(admin_id, "Admin", "Dr. Mehta", "General", 500.0);
    int p   = db_add_patient(receptionist_id, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");
    int p2  = db_add_patient(receptionist_id, "Receptionist", "Ravi Kumar", 45, "M", "9123456789", "Mysuru", "B+");
    printf("setup: doctor=%d patient=%d patient2=%d (all > 0)\n", doc, p, p2);

    /* ----- patients ----- */
    printf("update patient = %d (expect 0)\n",
           db_update_patient(receptionist_id, "Receptionist", p, "Asha R", 31, "9999999999", "Bengaluru"));
    printf("search new phone:\n");
    db_search_patient("9999999999");

    /* ----- appointments ----- */
    int a  = db_book_appointment(receptionist_id, p,  doc, "2026-11-03", "11:00");
    int b  = db_book_appointment(receptionist_id, p2, doc, "2026-11-03", "12:00");
    printf("appointments: %d, %d (both > 0)\n", a, b);
    printf("reschedule to free slot = %d (expect 0)\n",
           db_reschedule_appointment(receptionist_id, a, "2026-11-03", "11:30"));
    printf("reschedule into taken slot = %d (expect -2)\n",
           db_reschedule_appointment(receptionist_id, a, "2026-11-03", "12:00"));
    printf("queue count = %d (expect 2)\n", db_queue_count(doc, "2026-11-03"));

    /* ----- consultation ----- */
    printf("rx by receptionist = %d (expect -3)\n",
           db_add_visit_and_rx(receptionist_id, "Receptionist", a, "Fever", "Paracetamol", "500mg", "x"));
    int v = db_add_visit_and_rx(doctor_user_id, "Doctor", a, "Fever", "Paracetamol", "500mg twice daily", "3 days");
    printf("visit id = %d (expect > 0)\n", v);
    printf("rx on already-completed appointment = %d (expect -1)\n",
           db_add_visit_and_rx(doctor_user_id, "Doctor", a, "Again", "X", "Y", "Z"));
    printf("history:\n");
    db_get_patient_history(p);

    /* ----- billing ----- */
    int bill = db_create_bill(receptionist_id, "Receptionist", v);
    printf("bill id = %d (expect > 0)\n", bill);
    printf("duplicate bill = %d (expect -1)\n", db_create_bill(receptionist_id, "Receptionist", v));
    printf("set Paid = %d (expect 0), set bad status = %d (expect -1)\n",
           db_set_payment_status(receptionist_id, "Receptionist", bill, "Paid"),
           db_set_payment_status(receptionist_id, "Receptionist", bill, "Maybe"));

    /* ----- reports and doctors ----- */
    db_daily_report("2026-11-03");
    printf("doctors:\n");
    db_list_doctors();
    printf("update doctor = %d (expect 0)\n",
           db_update_doctor(admin_id, "Admin", doc, "Dr. Mehta", "Cardiology", 800.0, "09:00", "16:00"));
    printf("remove doctor with appointments = %d (expect -1)\n", db_remove_doctor(admin_id, "Admin", doc));

    db_close();
    remove("t_db2.db"); remove("t_db2.db-wal"); remove("t_db2.db-shm");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
