#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int scalar_int(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL; int value = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        value = sqlite3_column_int(st, 0);
    sqlite3_finalize(st); return value;
}

static int scalar_text(sqlite3 *db, const char *sql, const char *expected) {
    sqlite3_stmt *st = NULL; int same = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(st, 0);
        same = v && strcmp((const char *)v, expected) == 0;
    }
    sqlite3_finalize(st); return same;
}

static int scalar_double_is(sqlite3 *db, const char *sql, double expected) {
    sqlite3_stmt *st = NULL; int same = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        same = sqlite3_column_double(st, 0) == expected;
    sqlite3_finalize(st); return same;
}

int main(void) {
    const char *path = "t_audit.db";
    sqlite3 *direct = NULL, *legacy = NULL;
    remove_db(path);
    CHECK("create legacy audit-log fixture", sqlite3_open(path, &legacy) == SQLITE_OK &&
          sqlite3_exec(legacy, "CREATE TABLE audit_log(log_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "user_id INTEGER NOT NULL,action TEXT NOT NULL,table_name TEXT NOT NULL,"
                               "record_id INTEGER,ts TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
                               "INSERT INTO audit_log(user_id,action,table_name,record_id) "
                               "VALUES(0,'LEGACY','schema',1);", NULL, NULL, NULL) == SQLITE_OK);
    if (legacy) sqlite3_close(legacy);
    CHECK("open isolated audit database", db_open(path) == 0);
    CHECK("initialize schema and append-only audit guards", db_init_schema("schema.sql") == 0);
    CHECK("open audit verification connection", sqlite3_open(path, &direct) == SQLITE_OK);
    CHECK("schema upgrade preserves existing audit rows", scalar_text(direct,
          "SELECT action FROM audit_log WHERE log_id=1", "LEGACY"));

    int admin = db_create_staff(0, "Admin", "audit-admin", "h", "s", "Admin");
    int reception = db_create_staff(admin, "Admin", "audit-reception", "h", "s", "Receptionist");
    int clinician = db_create_staff(admin, "Admin", "audit-doctor", "h", "s", "Doctor");
    CHECK("bootstrap and create staff accounts are audited", admin > 0 && reception > 0 && clinician > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='staff' AND action='CREATE'") == 3);
    CHECK("staff creation rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_staff_create BEFORE INSERT ON audit_log WHEN NEW.table_name='staff' "
          "AND NEW.action='CREATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_create_staff(admin, "Admin", "rejected-staff", "h", "s", "Doctor") == -1 &&
          scalar_int(direct, "SELECT count(*) FROM staff WHERE username='rejected-staff'") == 0);
    sqlite3_exec(direct, "DROP TRIGGER reject_staff_create;", NULL, NULL, NULL);

    int doc = db_add_doctor(admin, "Admin", "Dr Audit", "General", 100.0);
    CHECK("doctor creation is audited", doc > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='doctors' AND action='CREATE'") == 1);
    CHECK("doctor update is audited", db_update_doctor(admin, "Admin", doc, "Dr Audit", "ENT", 120.0,
          "09:00", "17:00") == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='doctors' AND action='UPDATE'") == 1);
    int disposable_doc = db_add_doctor(admin, "Admin", "Dr Remove", "General", 90.0);
    CHECK("doctor deletion is audited", disposable_doc > 0 && db_remove_doctor(admin, "Admin", disposable_doc) == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='doctors' AND action='DELETE'") == 1);
    int guarded_doc = db_add_doctor(admin, "Admin", "Dr Guarded", "General", 80.0);
    CHECK("doctor deletion rolls back when its audit insert fails", guarded_doc > 0 && sqlite3_exec(direct,
          "CREATE TRIGGER reject_doctor_delete BEFORE INSERT ON audit_log WHEN NEW.table_name='doctors' "
          "AND NEW.action='DELETE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_remove_doctor(admin, "Admin", guarded_doc) == -1 &&
          scalar_int(direct, "SELECT count(*) FROM doctors WHERE doctor_id=3") == 1);
    sqlite3_exec(direct, "DROP TRIGGER reject_doctor_delete;", NULL, NULL, NULL);

    CHECK("doctor insert rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_doctor_create BEFORE INSERT ON audit_log WHEN NEW.table_name='doctors' "
          "AND NEW.action='CREATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_add_doctor(admin, "Admin", "Dr Rejected", "Test", 1.0) == -1 &&
          scalar_int(direct, "SELECT count(*) FROM doctors WHERE name='Dr Rejected'") == 0);
    sqlite3_exec(direct, "DROP TRIGGER reject_doctor_create;", NULL, NULL, NULL);
    CHECK("doctor update rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_doctor_update BEFORE INSERT ON audit_log WHEN NEW.table_name='doctors' "
          "AND NEW.action='UPDATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_update_doctor(admin, "Admin", doc, "Dr Changed", "ENT", 999.0,
          "09:00", "17:00") == -1 && scalar_text(direct,
          "SELECT name FROM doctors WHERE doctor_id=1", "Dr Audit") &&
          scalar_double_is(direct, "SELECT consultation_fee FROM doctors WHERE doctor_id=1", 120.0));
    sqlite3_exec(direct, "DROP TRIGGER reject_doctor_update;", NULL, NULL, NULL);

    int patient = db_add_patient(reception, "Receptionist", "Audit Patient", 35, "F",
                                 "9000000001", "Test", "O+");
    int appt = db_book_appointment(reception, patient, doc, "2026-11-15", "10:00");
    int visit = db_add_visit_and_rx(clinician, "Doctor", appt, "Diagnosis", "Medicine",
                                    "daily", "original notes");
    CHECK("patient, appointment, and prescription creates have timestamped audit events",
          patient > 0 && appt > 0 && visit > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name IN ('patients','appointments','prescriptions') "
                            "AND ts IS NOT NULL AND user_id>0 AND record_id IS NOT NULL") == 3);
    int rx = scalar_int(direct, "SELECT rx_id FROM prescriptions WHERE visit_id=1");
    CHECK("Receptionist cannot update or delete a prescription",
          db_update_prescription(reception, "Receptionist", rx, "forged", "", "") == -3 &&
          db_delete_prescription(reception, "Receptionist", rx) == -3);
    CHECK("prescription update records UPDATE", rx > 0 && db_update_prescription(clinician, "Doctor", rx,
          "Updated medicine", "twice daily", "updated notes") == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='prescriptions' AND action='UPDATE'") == 1);
    CHECK("prescription update rolls back when audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_rx_update BEFORE INSERT ON audit_log WHEN NEW.table_name='prescriptions' "
          "AND NEW.action='UPDATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_update_prescription(clinician, "Doctor", rx,
          "Rejected change", "once", "") == -1 && scalar_text(direct,
          "SELECT medicines FROM prescriptions WHERE rx_id=1", "Updated medicine"));
    sqlite3_exec(direct, "DROP TRIGGER reject_rx_update;", NULL, NULL, NULL);
    CHECK("prescription deletion rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_rx_delete BEFORE INSERT ON audit_log WHEN NEW.table_name='prescriptions' "
          "AND NEW.action='DELETE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_delete_prescription(clinician, "Doctor", rx) == -1 &&
          scalar_int(direct, "SELECT count(*) FROM prescriptions WHERE rx_id=1") == 1);
    sqlite3_exec(direct, "DROP TRIGGER reject_rx_delete;", NULL, NULL, NULL);
    CHECK("prescription deletion records DELETE", db_delete_prescription(clinician, "Doctor", rx) == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='prescriptions' AND action='DELETE'") == 1 &&
          scalar_int(direct, "SELECT count(*) FROM prescriptions WHERE rx_id=1") == 0);
    char rx_audit_sql[256];
    snprintf(rx_audit_sql, sizeof rx_audit_sql,
             "SELECT count(*) FROM audit_log WHERE table_name='prescriptions' AND record_id=%d "
             "AND user_id=%d AND ts IS NOT NULL AND action IN ('CREATE','UPDATE','DELETE')", rx, clinician);
    CHECK("prescription audit events preserve actor, action, record ID, and timestamp",
          scalar_int(direct, rx_audit_sql) == 3);

    int bill_visit_appt = db_book_appointment(reception, patient, doc, "2026-11-16", "10:00");
    int bill_visit = db_add_visit_and_rx(clinician, "Doctor", bill_visit_appt, "Diagnosis", "Medicine", "daily", "");
    int bill = db_create_bill(reception, "Receptionist", bill_visit);
    CHECK("bill creation is audited", bill > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='bills' AND action='CREATE'") == 1);
    CHECK("payment status update is audited", db_set_payment_status(reception, "Receptionist", bill, "Paid") == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='bills' AND action='UPDATE'") == 1);
    CHECK("payment status rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_bill_update BEFORE INSERT ON audit_log WHEN NEW.table_name='bills' "
          "AND NEW.action='UPDATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_set_payment_status(reception, "Receptionist", bill, "Pending") == -1 &&
          scalar_text(direct, "SELECT payment_status FROM bills WHERE bill_id=1", "Paid"));
    sqlite3_exec(direct, "DROP TRIGGER reject_bill_update;", NULL, NULL, NULL);
    CHECK("bill creation rolls back when its audit insert fails", sqlite3_exec(direct,
          "CREATE TRIGGER reject_bill_create BEFORE INSERT ON audit_log WHEN NEW.table_name='bills' "
          "AND NEW.action='CREATE' BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;",
          NULL, NULL, NULL) == SQLITE_OK && db_create_bill(reception, "Receptionist", bill_visit) == -1 &&
          scalar_int(direct, "SELECT count(*) FROM bills WHERE visit_id=2") == 1);
    sqlite3_exec(direct, "DROP TRIGGER reject_bill_create;", NULL, NULL, NULL);

    CHECK("audit UPDATE is rejected", sqlite3_exec(direct,
          "UPDATE audit_log SET action='TAMPER' WHERE log_id=1;", NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK("audit DELETE is rejected", sqlite3_exec(direct,
          "DELETE FROM audit_log WHERE log_id=1;", NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK("audit entry remains unchanged after tamper attempts", scalar_text(direct,
          "SELECT action FROM audit_log WHERE log_id=1", "LEGACY"));

    if (direct) sqlite3_close(direct);
    db_close(); remove_db(path);
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
