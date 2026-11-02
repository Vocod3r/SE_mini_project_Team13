#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"
#include "doctor_manager.h"

static int failures;
#define CHECK(label, condition) do { \
    int ok_ = !!(condition); \
    printf("%s: %s\n", ok_ ? "PASS" : "FAIL", label); \
    if (!ok_) failures++; \
} while (0)

static int scalar_int(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    int value = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) value = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return value;
}

static int scalar_text_is(sqlite3 *db, const char *sql, const char *expected) {
    sqlite3_stmt *st = NULL;
    int matched = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(st, 0);
        matched = value && strcmp((const char *)value, expected) == 0;
    }
    sqlite3_finalize(st);
    return matched;
}

static int integrity_ok(sqlite3 *db) {
    return scalar_text_is(db, "PRAGMA integrity_check;", "ok");
}

static void remove_test_db(const char *path) {
    char aux[256];
    remove(path);
    snprintf(aux, sizeof aux, "%s-wal", path); remove(aux);
    snprintf(aux, sizeof aux, "%s-shm", path); remove(aux);
}

int main(void) {
    sqlite3 *direct = NULL, *backup = NULL;
    remove_test_db("t_core.db");
    remove_test_db("t_core_backup.db");
    CHECK("open isolated database", db_open("t_core.db") == 0);
    CHECK("initialize canonical schema", db_init_schema("schema.sql") == 0);
    int admin_id = db_create_staff(0, "Admin", "admin", "testhash", "testsalt", "Admin");
    int receptionist_id = db_create_staff(admin_id, "Admin", "reception", "testhash", "testsalt", "Receptionist");
    int doctor_user_id = db_create_staff(admin_id, "Admin", "doctor", "testhash", "testsalt", "Doctor");
    CHECK("open inspection connection", sqlite3_open("t_core.db", &direct) == SQLITE_OK);
    CHECK("one canonical doctors table", scalar_int(direct,
          "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='doctors'") == 1);
    CHECK("canonical doctors columns exist",
          scalar_int(direct, "SELECT count(*) FROM pragma_table_info('doctors') "
                             "WHERE name IN ('work_start','work_end','consultation_fee')") == 3);
    CHECK("incompatible working_hours schema was not created",
          scalar_int(direct, "SELECT count(*) FROM pragma_table_info('doctors') "
                             "WHERE name IN ('working_hours','is_active')") == 0);

    CHECK("doctor manager inserts against canonical schema",
          add_doctor(direct, "Dr. O'Neil", "General", "09:00-17:00") == 1);
    int did = scalar_int(direct, "SELECT max(doctor_id) FROM doctors");
    CHECK("doctor manager safely updates hours", update_doctor(direct, did, "Family", "08:30-16:30") == 1);
    CHECK("doctor manager rejects malformed hours",
          update_doctor(direct, did, "Family", "09:00'); DROP TABLE doctors;--") == 0);
    CHECK("doctor table remains intact", scalar_int(direct, "SELECT count(*) FROM doctors") == 1);

    int pid = db_add_patient(receptionist_id, "Receptionist", "Asha Rao", 30, "F",
                             "9876543210", "Chennai", "O+");
    CHECK("caller cannot claim a different stored role",
          db_update_patient(receptionist_id, "Admin", pid, "Spoofed", 30, "1", "X") == -3 &&
          db_book_appointment(doctor_user_id, pid, did, "2026-11-09", "10:00") == -3);
    CHECK("patient create records audit", pid > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='patients' AND action='CREATE'") == 1);
    CHECK("patient update records audit", db_update_patient(receptionist_id, "Receptionist", pid,
          "Asha R", 31, "9999999999", "Bengaluru") == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='patients' AND action='UPDATE'") == 1);

    CHECK("create audit-failure trigger", sqlite3_exec(direct,
          "CREATE TRIGGER reject_patient_audit BEFORE INSERT ON audit_log "
          "WHEN NEW.table_name='patients' AND NEW.action='UPDATE' "
          "BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK("patient write fails if audit fails", db_update_patient(receptionist_id, "Receptionist", pid,
          "Should Roll Back", 32, "1111111111", "Nowhere") == -1);
    CHECK("failed patient update rolled back", scalar_int(direct,
          "SELECT count(*) FROM patients WHERE patient_id=1 AND name='Asha R'") == 1);
    sqlite3_exec(direct, "DROP TRIGGER reject_patient_audit;", NULL, NULL, NULL);

    int appt = db_book_appointment(receptionist_id, pid, did, "2026-11-10", "10:00");
    CHECK("appointment create records audit", appt > 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='appointments' AND action='CREATE'") == 1);
    CHECK("appointment audit failure rolls back booking", sqlite3_exec(direct,
          "CREATE TRIGGER reject_appt_audit BEFORE INSERT ON audit_log "
          "WHEN NEW.table_name='appointments' AND NEW.action='CREATE' "
          "BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;", NULL, NULL, NULL) == SQLITE_OK &&
          db_book_appointment(receptionist_id, pid, did, "2026-11-10", "11:00") == -1 &&
          scalar_int(direct, "SELECT count(*) FROM appointments") == 1);
    sqlite3_exec(direct, "DROP TRIGGER reject_appt_audit;", NULL, NULL, NULL);
    CHECK("appointment reschedule records update", db_reschedule_appointment(receptionist_id, appt,
          "2026-11-11", "10:30") == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='appointments' AND action='UPDATE'") == 1);
    CHECK("reschedule audit failure rolls back slot change", sqlite3_exec(direct,
          "CREATE TRIGGER reject_appt_update BEFORE INSERT ON audit_log "
          "WHEN NEW.table_name='appointments' AND NEW.action='UPDATE' "
          "BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;", NULL, NULL, NULL) == SQLITE_OK &&
          db_reschedule_appointment(receptionist_id, appt, "2026-11-12", "10:30") == -1 &&
          scalar_text_is(direct, "SELECT appt_date FROM appointments WHERE appt_id=1", "2026-11-11"));
    sqlite3_exec(direct, "DROP TRIGGER reject_appt_update;", NULL, NULL, NULL);
    CHECK("appointment cancellation records delete", db_cancel_appointment(receptionist_id, appt) == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='appointments' AND action='DELETE'") == 1);

    int fail_appt = db_book_appointment(receptionist_id, pid, did, "2026-11-12", "08:00");
    CHECK("prescription audit failure rolls back consultation", fail_appt > 0 && sqlite3_exec(direct,
          "CREATE TRIGGER reject_rx_audit BEFORE INSERT ON audit_log "
          "WHEN NEW.table_name='prescriptions' "
          "BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;", NULL, NULL, NULL) == SQLITE_OK &&
          db_add_visit_and_rx(doctor_user_id, "Doctor", fail_appt, "Test", "Medicine", "1/day", "2 days") == -1 &&
          scalar_int(direct, "SELECT count(*) FROM visits WHERE appt_id=2") == 0 &&
          scalar_text_is(direct, "SELECT status FROM appointments WHERE appt_id=2", "Booked"));
    sqlite3_exec(direct, "DROP TRIGGER reject_rx_audit;", NULL, NULL, NULL);

    int p2 = db_add_patient(receptionist_id, "Receptionist", "No History", 29, "F",
                            "9000000000", "Pune", "A+");
    int appt1 = db_book_appointment(receptionist_id, pid, did, "2026-11-12", "09:00");
    int appt2 = db_book_appointment(receptionist_id, pid, did, "2026-11-13", "09:00");
    sqlite3_stmt *fixture = NULL;
    int fixture_ok = appt1 > 0 && appt2 > 0 && sqlite3_prepare_v2(direct,
        "INSERT INTO visits(appt_id,patient_id,diagnosis,visit_date) VALUES(?,?,?,?);",
        -1, &fixture, NULL) == SQLITE_OK;
    if (fixture_ok) {
        sqlite3_bind_int(fixture, 1, appt1); sqlite3_bind_int(fixture, 2, pid);
        sqlite3_bind_text(fixture, 3, "later", -1, SQLITE_STATIC);
        sqlite3_bind_text(fixture, 4, "2026-11-20", -1, SQLITE_STATIC);
        fixture_ok = sqlite3_step(fixture) == SQLITE_DONE;
    }
    sqlite3_finalize(fixture); fixture = NULL;
    int later_visit = (int)sqlite3_last_insert_rowid(direct);
    if (fixture_ok && sqlite3_prepare_v2(direct,
        "INSERT INTO visits(appt_id,patient_id,diagnosis,visit_date) VALUES(?,?,?,?);",
        -1, &fixture, NULL) == SQLITE_OK) {
        sqlite3_bind_int(fixture, 1, appt2); sqlite3_bind_int(fixture, 2, pid);
        sqlite3_bind_text(fixture, 3, "earlier", -1, SQLITE_STATIC);
        sqlite3_bind_text(fixture, 4, "2026-11-01", -1, SQLITE_STATIC);
        fixture_ok = sqlite3_step(fixture) == SQLITE_DONE;
    } else fixture_ok = 0;
    sqlite3_finalize(fixture); fixture = NULL;
    int earlier_visit = (int)sqlite3_last_insert_rowid(direct);
    if (fixture_ok && sqlite3_prepare_v2(direct,
        "INSERT INTO prescriptions(visit_id,patient_id,medicines,dosage,notes) VALUES(?,?,?,'1/day','3 days');",
        -1, &fixture, NULL) == SQLITE_OK) {
        sqlite3_bind_int(fixture, 1, later_visit); sqlite3_bind_int(fixture, 2, pid);
        sqlite3_bind_text(fixture, 3, "Later medicine", -1, SQLITE_STATIC);
        fixture_ok = sqlite3_step(fixture) == SQLITE_DONE;
    } else fixture_ok = 0;
    sqlite3_finalize(fixture); fixture = NULL;
    if (fixture_ok && sqlite3_prepare_v2(direct,
        "INSERT INTO prescriptions(visit_id,patient_id,medicines,dosage,notes) VALUES(?,?,?,'1/day','3 days');",
        -1, &fixture, NULL) == SQLITE_OK) {
        sqlite3_bind_int(fixture, 1, earlier_visit); sqlite3_bind_int(fixture, 2, pid);
        sqlite3_bind_text(fixture, 3, "Earlier medicine", -1, SQLITE_STATIC);
        fixture_ok = sqlite3_step(fixture) == SQLITE_DONE;
    } else fixture_ok = 0;
    sqlite3_finalize(fixture);
    CHECK("insert dated history fixtures", fixture_ok);
    PrescriptionRecord records[2];
    CHECK("HMS-F-041 count query returns the complete history",
          db_get_prescription_records(doctor_user_id, "Doctor", pid, NULL, 0) == 2);
    CHECK("HMS-F-041 requires enough capacity for the complete history",
          db_get_prescription_records(doctor_user_id, "Doctor", pid, records, 1) == -1);
    CHECK("HMS-F-041 returns all details chronologically",
          db_get_prescription_records(doctor_user_id, "Doctor", pid, records, 2) == 2 &&
          strcmp(records[0].visit_date, "2026-11-01") == 0 &&
          strcmp(records[0].medicines, "Earlier medicine") == 0 &&
          strcmp(records[0].dosage, "1/day") == 0 &&
          strcmp(records[0].notes, "3 days") == 0 &&
          strcmp(records[1].visit_date, "2026-11-20") == 0 &&
          strcmp(records[1].medicines, "Later medicine") == 0);
    CHECK("HMS-F-041 rejects forged Doctor role",
          db_get_prescription_records(receptionist_id, "Doctor", pid, records, 2) == -3);
    CHECK("HMS-F-041 handles empty history",
          db_get_prescription_records(admin_id, "Admin", p2, records, 2) == 0);
    CHECK("HMS-F-041 rejects invalid patient IDs",
          db_get_prescription_records(doctor_user_id, "Doctor", 0, records, 2) == -3);
    int bulk_history_ok = 1;
    for (int i = 0; i < 64 && bulk_history_ok; ++i) {
        sqlite3_stmt *bulk = NULL;
        bulk_history_ok = sqlite3_prepare_v2(direct,
            "INSERT INTO visits(appt_id,patient_id,diagnosis,visit_date) "
            "VALUES(?,?,?,'2027-01-01');", -1, &bulk, NULL) == SQLITE_OK;
        if (bulk_history_ok) {
            sqlite3_bind_int(bulk, 1, appt1);
            sqlite3_bind_int(bulk, 2, pid);
            sqlite3_bind_text(bulk, 3, "long history", -1, SQLITE_STATIC);
            bulk_history_ok = sqlite3_step(bulk) == SQLITE_DONE;
        }
        sqlite3_finalize(bulk);
        if (!bulk_history_ok) break;
        int visit = (int)sqlite3_last_insert_rowid(direct);
        bulk_history_ok = sqlite3_prepare_v2(direct,
            "INSERT INTO prescriptions(visit_id,patient_id,medicines,dosage,notes) "
            "VALUES(?,?,'Medicine','1/day','1 day');", -1, &bulk, NULL) == SQLITE_OK;
        if (bulk_history_ok) {
            sqlite3_bind_int(bulk, 1, visit);
            sqlite3_bind_int(bulk, 2, pid);
            bulk_history_ok = sqlite3_step(bulk) == SQLITE_DONE;
        }
        sqlite3_finalize(bulk);
    }
    PrescriptionRecord long_history[66];
    CHECK("HMS-F-041 loads histories beyond a small fixed result size",
          bulk_history_ok &&
          db_get_prescription_records(doctor_user_id, "Doctor", pid, NULL, 0) == 66 &&
          db_get_prescription_records(doctor_user_id, "Doctor", pid, long_history, 66) == 66);

    CHECK("backup completes and validates", db_backup("t_core_backup.db") == 0);
    CHECK("open backup for restore validation", sqlite3_open("t_core_backup.db", &backup) == SQLITE_OK);
    CHECK("backup preserves patients and schema",
          scalar_int(backup, "SELECT count(*) FROM patients") == 2 &&
          scalar_int(backup, "SELECT count(*) FROM pragma_table_info('doctors')") ==
          scalar_int(direct, "SELECT count(*) FROM pragma_table_info('doctors')"));
    CHECK("backup passes integrity check", integrity_ok(backup));
    CHECK("backup to missing directory fails", db_backup("missing_backup_folder/t_core.db") == -1);

    char audit_sql[128];
    snprintf(audit_sql, sizeof audit_sql,
             "SELECT count(*) FROM audit_log WHERE ts IS NOT NULL AND user_id=%d", receptionist_id);
    CHECK("audit rows contain timestamp and user", scalar_int(direct, audit_sql) >= 7);
    CHECK("patient archive records delete", db_archive_patient(admin_id, "Admin", p2) == 0 &&
          scalar_int(direct, "SELECT count(*) FROM audit_log WHERE table_name='patients' AND action='DELETE'") == 1);
    CHECK("independent doctor can be removed using canonical schema",
          add_doctor(direct, "Dr. Removable", "General", "09:00-17:00") == 1 &&
          remove_doctor(direct, scalar_int(direct, "SELECT max(doctor_id) FROM doctors")) == 1);
    sqlite3_close(backup);
    sqlite3_close(direct);
    db_close();
    remove_test_db("t_core.db");
    remove_test_db("t_core_backup.db");
    sqlite3 *old_db = NULL;
    remove_test_db("t_schema_legacy.db");
    CHECK("prepare existing-database preservation fixture",
          sqlite3_open("t_schema_legacy.db", &old_db) == SQLITE_OK && sqlite3_exec(old_db,
          "CREATE TABLE doctors(doctor_id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "staff_id INTEGER REFERENCES staff(staff_id), name TEXT NOT NULL, specialization TEXT, "
          "work_start TEXT DEFAULT '09:00', work_end TEXT DEFAULT '17:00', "
          "consultation_fee REAL NOT NULL DEFAULT 0);"
          "INSERT INTO doctors(name,specialization,consultation_fee) VALUES('Existing Doctor','General',250);",
          NULL, NULL, NULL) == SQLITE_OK);
    if (old_db) sqlite3_close(old_db);
    CHECK("open existing database and apply current schema",
          db_open("t_schema_legacy.db") == 0 && db_init_schema("schema.sql") == 0);
    old_db = NULL;
    CHECK("existing doctor record remains intact", sqlite3_open("t_schema_legacy.db", &old_db) == SQLITE_OK &&
          scalar_int(old_db, "SELECT count(*) FROM doctors WHERE name='Existing Doctor' AND consultation_fee=250") == 1);
    if (old_db) sqlite3_close(old_db);
    db_close();
    remove_test_db("t_schema_legacy.db");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
