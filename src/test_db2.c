#include <stdio.h>
#include "db.h"

int main(void) {
    db_open("hms.db");
    db_init_schema("schema.sql");

    int doc = db_add_doctor(1, "Admin", "Dr. Mehta", "General", 500.0);
    int p   = db_add_patient(1, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");
    int p2  = db_add_patient(1, "Receptionist", "Ravi Kumar", 45, "M", "9123456789", "Mysuru", "B+");
    printf("setup: doctor=%d patient=%d patient2=%d (all > 0)\n", doc, p, p2);

    /* ----- staff and lockout (F-003, F-031) ----- */
    printf("create staff = %d (expect > 0)\n",
           db_create_staff(1, "Admin", "reception1", "HASH123", "SALT123", "Receptionist"));
    printf("duplicate staff = %d (expect -2)\n",
           db_create_staff(1, "Admin", "reception1", "H", "S", "Receptionist"));
    printf("staff by non-admin = %d (expect -3)\n",
           db_create_staff(1, "Doctor", "hacker", "H", "S", "Admin"));
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

    /* ----- patients ----- */
    printf("update patient = %d (expect 0)\n",
           db_update_patient(1, "Receptionist", p, "Asha R", 31, "9999999999", "Bengaluru"));
    printf("search new phone:\n");
    db_search_patient("9999999999");

    /* ----- appointments ----- */
    int a  = db_book_appointment(1, p,  doc, "2026-11-03", "11:00");
    int b  = db_book_appointment(1, p2, doc, "2026-11-03", "12:00");
    printf("appointments: %d, %d (both > 0)\n", a, b);
    printf("reschedule to free slot = %d (expect 0)\n",
           db_reschedule_appointment(1, a, "2026-11-03", "11:30"));
    printf("reschedule into taken slot = %d (expect -2)\n",
           db_reschedule_appointment(1, a, "2026-11-03", "12:00"));
    printf("queue count = %d (expect 2)\n", db_queue_count(doc, "2026-11-03"));

    /* ----- consultation ----- */
    printf("rx by receptionist = %d (expect -3)\n",
           db_add_visit_and_rx(1, "Receptionist", a, "Fever", "Paracetamol", "500mg", "x"));
    int v = db_add_visit_and_rx(1, "Doctor", a, "Fever", "Paracetamol", "500mg twice daily", "3 days");
    printf("visit id = %d (expect > 0)\n", v);
    printf("rx on already-completed appointment = %d (expect -1)\n",
           db_add_visit_and_rx(1, "Doctor", a, "Again", "X", "Y", "Z"));
    printf("history:\n");
    db_get_patient_history(p);

    /* ----- billing ----- */
    int bill = db_create_bill(1, "Receptionist", v);
    printf("bill id = %d (expect > 0)\n", bill);
    printf("duplicate bill = %d (expect -1)\n", db_create_bill(1, "Receptionist", v));
    printf("set Paid = %d (expect 0), set bad status = %d (expect -1)\n",
           db_set_payment_status(1, "Receptionist", bill, "Paid"),
           db_set_payment_status(1, "Receptionist", bill, "Maybe"));

    /* ----- reports and doctors ----- */
    db_daily_report("2026-11-03");
    printf("doctors:\n");
    db_list_doctors();
    printf("update doctor = %d (expect 0)\n",
           db_update_doctor(1, "Admin", doc, "Dr. Mehta", "Cardiology", 800.0, "09:00", "16:00"));
    printf("remove doctor with appointments = %d (expect -1)\n", db_remove_doctor(1, "Admin", doc));

    db_close();
    return 0;
}