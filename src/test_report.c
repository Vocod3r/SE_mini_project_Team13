#include <stdio.h>
#include <string.h>
#include "app_service.h"
#include "appt_view.h"
#include "db.h"
#include "report.h"

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

int main(void) {
    sqlite3 *auth_db = NULL;
    User admin = {0}, rec = {0}, doc = {0};
    remove_db("t_report.db");
    CHECK("open database", db_open("t_report.db") == 0 && db_init_schema("schema.sql") == 0);
    CHECK("open auth connection", sqlite3_open("t_report.db", &auth_db) == SQLITE_OK);
    CHECK("create staff", create_initial_admin(auth_db, "root", "StrongPass!42") == 1 &&
          service_authenticate(auth_db, "root", "StrongPass!42", &admin) == 1 &&
          service_create_staff(auth_db, &admin, "desk", "DeskPass!42", "Receptionist") == 1 &&
          service_authenticate(auth_db, "desk", "DeskPass!42", &rec) == 1 &&
          service_create_staff(auth_db, &admin, "dr", "DocPass!4242", "Doctor") == 1 &&
          service_authenticate(auth_db, "dr", "DocPass!4242", &doc) == 1);

    int d1 = db_add_doctor(admin.user_id, admin.role, "Dr. Mehta", "General", 500.0);
    int d2 = db_add_doctor(admin.user_id, admin.role, "Dr. Iyer", "ENT", 400.0);
    int p1 = service_add_patient(&rec, "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");
    int p2 = service_add_patient(&rec, "Ravi Kumar", 45, "M", "9123456780", "Pune", "A+");
    int a1 = service_book_appointment(&rec, p1, d1, "2026-11-03", "10:00");
    int a2 = service_book_appointment(&rec, p2, d1, "2026-11-03", "09:30");
    int a3 = service_book_appointment(&rec, p1, d1, "2026-11-03", "11:00");
    int a4 = service_book_appointment(&rec, p2, d1, "2026-11-03", "11:30");
    int a5 = service_book_appointment(&rec, p2, d2, "2026-11-03", "09:00");
    int a6 = service_book_appointment(&rec, p1, d1, "2026-11-04", "09:00");
    CHECK("setup data", d1 > 0 && d2 > 0 && p1 > 0 && p2 > 0 && a1 > 0 && a2 > 0 && a3 > 0 &&
                        a4 > 0 && a5 > 0 && a6 > 0);

    printf("--- HMS-F-032 doctor queue ---\n");
    CHECK("queue: 4 waiting for Dr. Mehta", appt_queue(doc.user_id, doc.role, d1, "2026-11-03") == 4);
    CHECK("queue: receptionist and admin may view", appt_queue(rec.user_id, rec.role, d1, "2026-11-03") == 4 &&
          appt_queue(admin.user_id, admin.role, d1, "2026-11-03") == 4);
    CHECK("queue: other doctor sees only own", appt_queue(admin.user_id, admin.role, d2, "2026-11-03") == 1);
    CHECK("queue: empty day = 0", appt_queue(doc.user_id, doc.role, d1, "2026-12-25") == 0);
    db_cancel_appointment(rec.user_id, a3);
    CHECK("queue: cancelled patient leaves", appt_queue(doc.user_id, doc.role, d1, "2026-11-03") == 3);
    CHECK("queue: visit recorded -> patient leaves",
          service_record_prescription(&doc, a2, "Fever", "Paracetamol", "500mg", "") > 0 &&
          appt_queue(doc.user_id, doc.role, d1, "2026-11-03") == 2);
    CHECK("queue: rescheduled patient still waiting",
          db_reschedule_appointment(rec.user_id, a1, "2026-11-03", "10:30") == 0 &&
          appt_queue(doc.user_id, doc.role, d1, "2026-11-03") == 2);
    CHECK("queue: bad date refused", appt_queue(doc.user_id, doc.role, d1, "2026-13-45") == -1 &&
          appt_queue(doc.user_id, doc.role, d1, "tomorrow") == -1 && appt_queue(doc.user_id, doc.role, d1, NULL) == -1);
    CHECK("queue: forged role refused (checked against staff table)",
          appt_queue(rec.user_id, "Admin", d1, "2026-11-03") == -3 &&
          appt_queue(0, "Doctor", d1, "2026-11-03") == -3 && appt_queue(doc.user_id, NULL, d1, "2026-11-03") == -3);

    printf("--- HMS-F-060 daily report ---\n");
    CHECK("report: every status counted (5 on 11-03)", report_daily(admin.user_id, admin.role, "2026-11-03") == 5);
    CHECK("report: matches the raw DB listing", report_daily(admin.user_id, admin.role, "2026-11-03") == db_daily_report("2026-11-03"));
    CHECK("report: other day", report_daily(admin.user_id, admin.role, "2026-11-04") == 1);
    CHECK("report: empty day = 0", report_daily(admin.user_id, admin.role, "2026-12-25") == 0);
    CHECK("report: bad date refused", report_daily(admin.user_id, admin.role, "2026-02-31") == -1 &&
          report_daily(admin.user_id, admin.role, "") == -1 && report_daily(admin.user_id, admin.role, NULL) == -1);
    CHECK("report: receptionist and doctor refused", report_daily(rec.user_id, rec.role, "2026-11-03") == -3 &&
          report_daily(doc.user_id, doc.role, "2026-11-03") == -3);
    CHECK("report: forged Admin role refused", report_daily(rec.user_id, "Admin", "2026-11-03") == -3 &&
          report_daily(0, "Admin", "2026-11-03") == -3 && report_daily(admin.user_id, NULL, "2026-11-03") == -3);

    if (auth_db) sqlite3_close(auth_db);
    db_close(); remove_db("t_report.db");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
