#ifdef _WIN32
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

static sqlite3 *g_db = NULL;

int db_open(const char *path) {
    if (sqlite3_open(path, &g_db) != SQLITE_OK) return -1;
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", 0, 0, 0);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    sqlite3_busy_timeout(g_db, 3000);
    return 0;
}

void db_close(void) {
    if (g_db) { sqlite3_close(g_db); g_db = NULL; }
}

int db_init_schema(const char *schema_path) {
    FILE *f = fopen(schema_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *buf = malloc(n + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); free(buf); return -1; }
    buf[n] = 0;
    fclose(f);
    char *err = NULL;
    int rc = sqlite3_exec(g_db, buf, 0, 0, &err);
    free(buf);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "schema error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* SR-003: every create/update/delete calls this */
int db_log_audit(int user_id, const char *action, const char *table, int rec_id) {
    sqlite3_stmt *st;
    const char *sql =
        "INSERT INTO audit_log(user_id,action,table_name,record_id) VALUES(?,?,?,?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, user_id);
    sqlite3_bind_text(st, 2, action, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, table, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 4, rec_id);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

static int role_is(const char *role, const char *a, const char *b) {
    return role && (strcmp(role, a) == 0 || (b && strcmp(role, b) == 0));
}

/* F-030 */
int db_add_doctor(int user_id, const char *role, const char *name,
                  const char *spec, double fee) {
    if (!role_is(role, "Admin", NULL)) return -3;
    if (!name || !*name || strlen(name) > 100 || fee < 0) return -1;
    sqlite3_stmt *st;
    const char *sql =
        "INSERT INTO doctors(name,specialization,consultation_fee) VALUES(?,?,?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, spec, -1, SQLITE_STATIC);
    sqlite3_bind_double(st, 3, fee);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return -1;
    int id = (int)sqlite3_last_insert_rowid(g_db);
    db_log_audit(user_id, "CREATE", "doctors", id);
    return id;
}

/* F-010 + SR-002 + SR-003 + SR-004 */
int db_add_patient(int user_id, const char *role, const char *name, int age,
                   const char *gender, const char *phone, const char *addr, const char *bg) {
    if (!role_is(role, "Receptionist", "Admin")) return -3;
    if (!name || !*name || strlen(name) > 100 || !phone || strlen(phone) > 15) return -1;

    sqlite3_exec(g_db, "BEGIN;", 0, 0, 0);
    sqlite3_stmt *st;
    const char *sql =
        "INSERT INTO patients(name,age,gender,phone,address,blood_group) VALUES(?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", 0, 0, 0);
        return -1;
    }
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, age);
    sqlite3_bind_text(st, 3, gender, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 5, addr, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 6, bg, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(g_db, "ROLLBACK;", 0, 0, 0);
        return -1;
    }
    int id = (int)sqlite3_last_insert_rowid(g_db);
    db_log_audit(user_id, "CREATE", "patients", id);
    sqlite3_exec(g_db, "COMMIT;", 0, 0, 0);
    return id;
}

/* F-011: by ID, phone or name; archived hidden (F-014).
   % _ and \ typed by the user are escaped so they match literally. */
int db_search_patient(const char *q) {
    if (!q || !*q || strlen(q) > 100) return -1;

    char esc[256];
    size_t j = 0;
    for (size_t i = 0; q[i] && j < sizeof esc - 2; i++) {
        if (q[i] == '%' || q[i] == '_' || q[i] == '\\') esc[j++] = '\\';
        esc[j++] = q[i];
    }
    esc[j] = 0;

    sqlite3_stmt *st;
    const char *sql =
        "SELECT patient_id,name,age,phone FROM patients "
        "WHERE is_archived=0 AND "
        "(CAST(patient_id AS TEXT)=?1 OR phone=?1 OR name LIKE '%'||?2||'%' ESCAPE '\\');";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, q, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, esc, -1, SQLITE_STATIC);
    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%d | %s | %d | %s\n",
               sqlite3_column_int(st, 0),
               sqlite3_column_text(st, 1),
               sqlite3_column_int(st, 2),
               sqlite3_column_text(st, 3));
        count++;
    }
    sqlite3_finalize(st);
    return count;
}

/* F-014: soft delete */
int db_archive_patient(int user_id, const char *role, int pid) {
    if (!role_is(role, "Admin", NULL)) return -3;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db,
        "UPDATE patients SET is_archived=1 WHERE patient_id=?;", -1, &st, 0) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, pid);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "ARCHIVE", "patients", pid);
    return 0;
}

/* F-020 / F-021: the partial UNIQUE index rejects double-booking */
int db_book_appointment(int user_id, int pid, int did, const char *date, const char *slot) {
    sqlite3_stmt *st;
    const char *sql =
        "INSERT INTO appointments(patient_id,doctor_id,appt_date,time_slot) "
        "SELECT ?1,?2,?3,?4 WHERE EXISTS "
        "(SELECT 1 FROM patients WHERE patient_id=?1 AND is_archived=0);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, pid);
    sqlite3_bind_int(st, 2, did);
    sqlite3_bind_text(st, 3, date, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, slot, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc == SQLITE_CONSTRAINT) return -2;
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;  /* archived or unknown patient */
    int id = (int)sqlite3_last_insert_rowid(g_db);
    db_log_audit(user_id, "CREATE", "appointments", id);
    return id;
}

/* F-022: cancelling frees the slot (index ignores 'Cancelled') */
int db_cancel_appointment(int user_id, int appt_id) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db,
        "UPDATE appointments SET status='Cancelled' WHERE appt_id=?;", -1, &st, 0) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, appt_id);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "CANCEL", "appointments", appt_id);
    return 0;
}

/* F-023 */
int db_get_schedule(int did, const char *date) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT a.time_slot,p.name,a.status FROM appointments a "
        "JOIN patients p ON p.patient_id=a.patient_id "
        "WHERE a.doctor_id=? AND a.appt_date=? ORDER BY a.time_slot;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, did);
    sqlite3_bind_text(st, 2, date, -1, SQLITE_STATIC);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s | %s | %s\n",
               sqlite3_column_text(st, 0),
               sqlite3_column_text(st, 1),
               sqlite3_column_text(st, 2));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}

/* F-023 (weekly view): 7 days starting at start_date. Returns row count, -1 error / bad date. */
int db_get_schedule_week(int did, const char *start_date) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT a.appt_date,a.time_slot,p.name,a.status FROM appointments a "
        "JOIN patients p ON p.patient_id=a.patient_id "
        "WHERE a.doctor_id=?1 "
        "AND a.appt_date BETWEEN date(?2) AND date(?2,'+6 days') "
        "ORDER BY a.appt_date,a.time_slot;";
    if (!start_date) return -1;
    /* SQLite's date() yields NULL for anything that is not a real YYYY-MM-DD date */
    if (sqlite3_prepare_v2(g_db, "SELECT date(?1)=?1;", -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, start_date, -1, SQLITE_STATIC);
    int valid = (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st, 0) == 1);
    sqlite3_finalize(st);
    if (!valid) return -1;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, did);
    sqlite3_bind_text(st, 2, start_date, -1, SQLITE_STATIC);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s | %s | %s | %s\n",
               sqlite3_column_text(st, 0), sqlite3_column_text(st, 1),
               sqlite3_column_text(st, 2), sqlite3_column_text(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}

/* F-024: prints a confirmation slip. Returns 0 ok, -1 not found / error,
 * -2 appointment is not active (Cancelled or Completed). */
int db_print_appt_slip(int appt_id) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT a.appt_id,p.patient_id,p.name,d.name,d.specialization,d.consultation_fee,"
        "a.appt_date,a.time_slot,a.status "
        "FROM appointments a JOIN patients p ON p.patient_id=a.patient_id "
        "JOIN doctors d ON d.doctor_id=a.doctor_id WHERE a.appt_id=?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, appt_id);
    int rc = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *status = (const char *)sqlite3_column_text(st, 8);
        if (strcmp(status, "Booked") != 0 && strcmp(status, "Rescheduled") != 0) {
            rc = -2;
        } else {
            printf("==============================\n");
            printf("   APPOINTMENT CONFIRMATION\n");
            printf("==============================\n");
            printf("Appointment ID : %d\n", sqlite3_column_int(st, 0));
            printf("Patient        : %s (ID %d)\n", sqlite3_column_text(st, 2), sqlite3_column_int(st, 1));
            printf("Doctor         : %s (%s)\n", sqlite3_column_text(st, 3),
                   sqlite3_column_text(st, 4) ? (const char *)sqlite3_column_text(st, 4) : "-");
            printf("Date           : %s\n", sqlite3_column_text(st, 6));
            printf("Time           : %s\n", sqlite3_column_text(st, 7));
            printf("Fee            : %.2f\n", sqlite3_column_double(st, 5));
            printf("Status         : %s\n", status);
            printf("==============================\n");
            rc = 0;
        }
    }
    sqlite3_finalize(st);
    return rc;
}

/* F-061 */
int db_backup(const char *dest) {
    sqlite3 *dst;
    if (sqlite3_open(dest, &dst) != SQLITE_OK) return -1;
    sqlite3_backup *b = sqlite3_backup_init(dst, "main", g_db, "main");
    if (!b) { sqlite3_close(dst); return -1; }
    sqlite3_backup_step(b, -1);
    sqlite3_backup_finish(b);
    int ok = (sqlite3_errcode(dst) == SQLITE_OK);
    sqlite3_close(dst);
    return ok ? 0 : -1;
}

/* F-062 (connectivity; disk space can be added later) */
int db_health_check(void) {
    sqlite3_stmt *st;
    if (!g_db || sqlite3_prepare_v2(g_db, "SELECT 1;", -1, &st, 0) != SQLITE_OK) return -1;
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_ROW) return -1;

    double free_mb = 0;
#ifdef _WIN32
    ULARGE_INTEGER freeBytes;
    if (!GetDiskFreeSpaceExA(".", &freeBytes, NULL, NULL)) return -1;
    free_mb = freeBytes.QuadPart / (1024.0 * 1024.0);
#else
    struct statvfs sv;
    if (statvfs(".", &sv) != 0) return -1;
    free_mb = (double)sv.f_bavail * sv.f_frsize / (1024.0 * 1024.0);
#endif
    printf("DB: OK | free disk: %.0f MB\n", free_mb);
    return free_mb >= 50 ? 0 : -1;
}

/* ---------- helpers ---------- */
static int exec_text(const char *sql, const char *text) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, text, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_changes(g_db);
}

static int staff_id_of(const char *username) {
    sqlite3_stmt *st;
    int id = -1;
    if (sqlite3_prepare_v2(g_db, "SELECT staff_id FROM staff WHERE username=?;", -1, &st, 0) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return id;
}

/* ---------- staff and login ---------- */

/* F-031: password_hash and salt come from the Developer's Auth module (SR-001) */
int db_create_staff(int user_id, const char *role, const char *username,
                    const char *hash, const char *salt, const char *new_role) {
    if (!role_is(role, "Admin", NULL)) return -3;
    if (!username || !*username || strlen(username) > 60 || !hash || !salt) return -1;
    if (!new_role || !(strcmp(new_role, "Admin") == 0 || strcmp(new_role, "Doctor") == 0 ||
                       strcmp(new_role, "Receptionist") == 0)) return -1;
    sqlite3_stmt *st;
    const char *sql = "INSERT INTO staff(username,password_hash,salt,role) VALUES(?,?,?,?);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, salt, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, new_role, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc == SQLITE_CONSTRAINT) return -2;          /* username already exists */
    if (rc != SQLITE_DONE) return -1;
    int id = (int)sqlite3_last_insert_rowid(g_db);
    db_log_audit(user_id, "CREATE", "staff", id);
    return id;
}

int db_get_staff(const char *username, Staff *out) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT staff_id,username,password_hash,salt,role,failed_attempts,locked_until "
        "FROM staff WHERE username=?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);
    int found = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out->staff_id = sqlite3_column_int(st, 0);
        snprintf(out->username, sizeof out->username, "%s", (const char *)sqlite3_column_text(st, 1));
        snprintf(out->password_hash, sizeof out->password_hash, "%s", (const char *)sqlite3_column_text(st, 2));
        snprintf(out->salt, sizeof out->salt, "%s", (const char *)sqlite3_column_text(st, 3));
        snprintf(out->role, sizeof out->role, "%s", (const char *)sqlite3_column_text(st, 4));
        out->failed_attempts = sqlite3_column_int(st, 5);
        out->locked_until = sqlite3_column_int64(st, 6);
        found = 0;
    }
    sqlite3_finalize(st);
    return found;
}

int db_is_locked(const char *username) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT locked_until > CAST(strftime('%s','now') AS INTEGER) FROM staff WHERE username=?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_STATIC);
    int r = -1;
    if (sqlite3_step(st) == SQLITE_ROW) r = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return r;
}

/* F-003: lock for 5 minutes (300 s) after 3 consecutive failures, and write an audit event */
int db_record_failed_login(const char *username) {
    int sid = staff_id_of(username);
    if (sid < 0) return -1;
    if (db_is_locked(username) == 1) return -2;
    exec_text("UPDATE staff SET failed_attempts=failed_attempts+1 WHERE username=?;", username);
    int locked = exec_text(
        "UPDATE staff SET locked_until=CAST(strftime('%s','now') AS INTEGER)+300, failed_attempts=0 "
        "WHERE username=? AND failed_attempts>=3;", username);
    if (locked > 0) {
        db_log_audit(sid, "LOCKOUT", "staff", sid);
        return 1;
    }
    return 0;
}

int db_reset_failed_login(const char *username) {
    return exec_text("UPDATE staff SET failed_attempts=0, locked_until=0 WHERE username=?;", username) >= 0 ? 0 : -1;
}

/* ---------- doctors ---------- */
int db_update_doctor(int user_id, const char *role, int did, const char *name,
                     const char *spec, double fee, const char *start, const char *end) {
    if (!role_is(role, "Admin", NULL)) return -3;
    if (!name || !*name || strlen(name) > 100 || fee < 0) return -1;
    sqlite3_stmt *st;
    const char *sql =
        "UPDATE doctors SET name=?,specialization=?,consultation_fee=?,work_start=?,work_end=? "
        "WHERE doctor_id=?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, spec, -1, SQLITE_STATIC);
    sqlite3_bind_double(st, 3, fee);
    sqlite3_bind_text(st, 4, start, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 5, end, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 6, did);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "UPDATE", "doctors", did);
    return 0;
}

/* Fails (returns -1) if the doctor still has appointments, because of the foreign key */
int db_remove_doctor(int user_id, const char *role, int did) {
    if (!role_is(role, "Admin", NULL)) return -3;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM doctors WHERE doctor_id=?;", -1, &st, 0) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, did);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "DELETE", "doctors", did);
    return 0;
}

int db_list_doctors(void) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db,
        "SELECT doctor_id,name,specialization,consultation_fee FROM doctors ORDER BY doctor_id;",
        -1, &st, 0) != SQLITE_OK) return -1;
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%d | %s | %s | %.2f\n", sqlite3_column_int(st, 0),
               sqlite3_column_text(st, 1), sqlite3_column_text(st, 2), sqlite3_column_double(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}

/* ---------- patients ---------- */
int db_update_patient(int user_id, const char *role, int pid, const char *name,
                      int age, const char *phone, const char *addr) {
    if (!role_is(role, "Receptionist", "Admin")) return -3;
    if (!name || !*name || strlen(name) > 100 || !phone || strlen(phone) > 15) return -1;
    sqlite3_stmt *st;
    const char *sql =
        "UPDATE patients SET name=?,age=?,phone=?,address=? WHERE patient_id=? AND is_archived=0;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, age);
    sqlite3_bind_text(st, 3, phone, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, addr, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 5, pid);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "UPDATE", "patients", pid);
    return 0;
}

/* ---------- appointments ---------- */
int db_reschedule_appointment(int user_id, int appt_id, const char *date, const char *slot) {
    sqlite3_stmt *st;
    const char *sql =
        "UPDATE appointments SET appt_date=?,time_slot=?,status='Rescheduled' "
        "WHERE appt_id=? AND status IN ('Booked','Rescheduled');";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, date, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, slot, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 3, appt_id);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc == SQLITE_CONSTRAINT) return -2;           /* new slot taken */
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "RESCHEDULE", "appointments", appt_id);
    return 0;
}

/* F-032 */
int db_queue_count(int did, const char *date) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT COUNT(*) FROM appointments WHERE doctor_id=? AND appt_date=? "
        "AND status IN ('Booked','Rescheduled');";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, did);
    sqlite3_bind_text(st, 2, date, -1, SQLITE_STATIC);
    int n = -1;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return n;
}

/* ---------- consultation ---------- */

/* F-040: visit + prescription + appointment status in ONE transaction. Returns visit_id. */
int db_add_visit_and_rx(int user_id, const char *role, int appt_id, const char *diagnosis,
                        const char *medicines, const char *dosage, const char *notes) {
    if (!role_is(role, "Doctor", NULL)) return -3;
    if (!medicines || !*medicines) return -1;
    sqlite3_stmt *st = NULL;
    int pid = -1, vid = -1, rc;

    sqlite3_exec(g_db, "BEGIN;", 0, 0, 0);

    if (sqlite3_prepare_v2(g_db,
        "SELECT patient_id FROM appointments WHERE appt_id=? AND status IN ('Booked','Rescheduled');",
        -1, &st, 0) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, appt_id);
    if (sqlite3_step(st) == SQLITE_ROW) pid = sqlite3_column_int(st, 0);
    sqlite3_finalize(st); st = NULL;
    if (pid < 0) goto fail;

    if (sqlite3_prepare_v2(g_db, "INSERT INTO visits(appt_id,patient_id,diagnosis) VALUES(?,?,?);",
                           -1, &st, 0) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, appt_id);
    sqlite3_bind_int(st, 2, pid);
    sqlite3_bind_text(st, 3, diagnosis, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st); st = NULL;
    if (rc != SQLITE_DONE) goto fail;
    vid = (int)sqlite3_last_insert_rowid(g_db);

    if (sqlite3_prepare_v2(g_db,
        "INSERT INTO prescriptions(visit_id,patient_id,medicines,dosage,notes) VALUES(?,?,?,?,?);",
        -1, &st, 0) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, vid);
    sqlite3_bind_int(st, 2, pid);
    sqlite3_bind_text(st, 3, medicines, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, dosage, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 5, notes, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st); st = NULL;
    if (rc != SQLITE_DONE) goto fail;
    int rxid = (int)sqlite3_last_insert_rowid(g_db);

    if (sqlite3_prepare_v2(g_db, "UPDATE appointments SET status='Completed' WHERE appt_id=?;",
                           -1, &st, 0) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, appt_id);
    rc = sqlite3_step(st);
    sqlite3_finalize(st); st = NULL;
    if (rc != SQLITE_DONE) goto fail;

    db_log_audit(user_id, "CREATE", "visits", vid);
    db_log_audit(user_id, "CREATE", "prescriptions", rxid);
    sqlite3_exec(g_db, "COMMIT;", 0, 0, 0);
    return vid;

fail:
    if (st) sqlite3_finalize(st);
    sqlite3_exec(g_db, "ROLLBACK;", 0, 0, 0);
    return -1;
}

/* F-013 / F-041: newest first */
int db_get_patient_history(int pid) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT v.visit_date,v.diagnosis,r.medicines,r.dosage FROM visits v "
        "LEFT JOIN prescriptions r ON r.visit_id=v.visit_id "
        "WHERE v.patient_id=? ORDER BY v.visit_date DESC, v.visit_id DESC;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, pid);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s | %s | %s | %s\n", sqlite3_column_text(st, 0), sqlite3_column_text(st, 1),
               sqlite3_column_text(st, 2), sqlite3_column_text(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}

/* ---------- billing ---------- */

/* F-050: amount is taken from the doctor's fee; only one bill per visit */
int db_create_bill(int user_id, const char *role, int visit_id) {
    if (!role_is(role, "Receptionist", "Admin")) return -3;
    sqlite3_stmt *st;
    const char *sql =
        "INSERT INTO bills(visit_id,amount) "
        "SELECT v.visit_id, d.consultation_fee FROM visits v "
        "JOIN appointments a ON a.appt_id=v.appt_id "
        "JOIN doctors d ON d.doctor_id=a.doctor_id "
        "WHERE v.visit_id=? AND NOT EXISTS (SELECT 1 FROM bills b WHERE b.visit_id=v.visit_id);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, visit_id);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    int id = (int)sqlite3_last_insert_rowid(g_db);
    db_log_audit(user_id, "CREATE", "bills", id);
    return id;
}

/* F-051 */
int db_set_payment_status(int user_id, const char *role, int bill_id, const char *status) {
    if (!role_is(role, "Receptionist", "Admin")) return -3;
    if (!status || !(strcmp(status, "Paid") == 0 || strcmp(status, "Pending") == 0)) return -1;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(g_db, "UPDATE bills SET payment_status=? WHERE bill_id=?;",
                           -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, bill_id);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(g_db) == 0) return -1;
    db_log_audit(user_id, "UPDATE", "bills", bill_id);
    return 0;
}

/* ---------- reports ---------- */

/* F-060 */
int db_daily_report(const char *date) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT a.appt_id,a.time_slot,p.name,d.name,a.status FROM appointments a "
        "JOIN patients p ON p.patient_id=a.patient_id "
        "JOIN doctors d ON d.doctor_id=a.doctor_id "
        "WHERE a.appt_date=? ORDER BY a.time_slot;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, date, -1, SQLITE_STATIC);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("#%d | %s | %s | %s | %s\n", sqlite3_column_int(st, 0), sqlite3_column_text(st, 1),
               sqlite3_column_text(st, 2), sqlite3_column_text(st, 3), sqlite3_column_text(st, 4));
        n++;
    }
    sqlite3_finalize(st);
    printf("Total appointments on %s: %d\n", date, n);
    return n;

}
/* F-041: prescriptions, newest first */
int db_get_prescriptions(int pid) {
    sqlite3_stmt *st;
    const char *sql =
        "SELECT v.visit_date,r.medicines,r.dosage,r.notes FROM prescriptions r "
        "JOIN visits v ON v.visit_id=r.visit_id "
        "WHERE r.patient_id=? ORDER BY v.visit_date DESC, r.rx_id DESC;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, 0) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, pid);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s | %s | %s | %s\n", sqlite3_column_text(st, 0), sqlite3_column_text(st, 1),
               sqlite3_column_text(st, 2), sqlite3_column_text(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}