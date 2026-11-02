#include <stdio.h>
#include <string.h>
#include "doctor_manager.h"

/* The manager API accepts the old "HH:MM-HH:MM" field, while the canonical
 * schema stores its endpoints separately. Reject malformed input rather than
 * interpolating it into SQL or silently storing an unusable schedule. */
static int parse_hours(const char *hours, char *start, size_t start_cap,
                       char *end, size_t end_cap) {
    char sh[6] = {0}, sm[6] = {0}, eh[6] = {0}, em[6] = {0};
    int consumed = 0;
    if (!hours || sscanf(hours, "%5[0-9]:%5[0-9]-%5[0-9]:%5[0-9]%n",
                         sh, sm, eh, em, &consumed) != 4 ||
        hours[consumed] != '\0' || strlen(sh) != 2 || strlen(sm) != 2 ||
        strlen(eh) != 2 || strlen(em) != 2 || start_cap < 6 || end_cap < 6) return 0;
    snprintf(start, start_cap, "%s:%s", sh, sm);
    snprintf(end, end_cap, "%s:%s", eh, em);
    return 1;
}

int add_doctor(sqlite3 *db, const char *name, const char *specialization,
               const char *working_hours) {
    char start[12] = {0}, end[12] = {0};
    sqlite3_stmt *st = NULL;
    int result = 0;
    if (!db || !name || !*name || !specialization || !working_hours ||
        !parse_hours(working_hours, start, sizeof start, end, sizeof end)) return 0;
    /* parse_hours stores both endpoints in compact buffers below. */
    if (sqlite3_prepare_v2(db,
        "INSERT INTO doctors(name,specialization,work_start,work_end) VALUES(?,?,?,?);",
        -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, specialization, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, start, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, end, -1, SQLITE_TRANSIENT);
        result = sqlite3_step(st) == SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return result;
}

int update_doctor(sqlite3 *db, int doctor_id, const char *specialization,
                  const char *working_hours) {
    char start[12] = {0}, end[12] = {0};
    sqlite3_stmt *st = NULL;
    int result = 0;
    if (!db || doctor_id <= 0 || !specialization || !working_hours ||
        !parse_hours(working_hours, start, sizeof start, end, sizeof end)) return 0;
    if (sqlite3_prepare_v2(db,
        "UPDATE doctors SET specialization=?,work_start=?,work_end=? WHERE doctor_id=?;",
        -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, specialization, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, start, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, end, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 4, doctor_id);
        result = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) > 0;
    }
    sqlite3_finalize(st);
    return result;
}

int remove_doctor(sqlite3 *db, int doctor_id) {
    sqlite3_stmt *st = NULL;
    int result = 0;
    if (!db || doctor_id <= 0) return 0;
    if (sqlite3_prepare_v2(db, "DELETE FROM doctors WHERE doctor_id=?;", -1,
                           &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, doctor_id);
        result = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) > 0;
    }
    sqlite3_finalize(st);
    return result;
}

int get_doctor_queue_count(sqlite3 *db, int doctor_id, const char *date_str) {
    sqlite3_stmt *st = NULL;
    int count = -1;
    if (!db || doctor_id <= 0 || !date_str || sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM appointments WHERE doctor_id=? AND appt_date=? "
        "AND status IN ('Booked','Rescheduled');", -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, doctor_id);
    sqlite3_bind_text(st, 2, date_str, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return count;
}
