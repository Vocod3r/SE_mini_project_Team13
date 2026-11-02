#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doctor_manager.h"

// HMS-F-030: Add a new doctor profile
int add_doctor(sqlite3 *db, const char *name, const char *specialization, const char *working_hours) {
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO doctors (name, specialization, working_hours, is_active) VALUES ('%s', '%s', '%s', 1);", 
        name, specialization, working_hours);
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    return 1;
}

// HMS-F-030: Update an existing doctor's specialization and working hours
int update_doctor(sqlite3 *db, int doctor_id, const char *specialization, const char *working_hours) {
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "UPDATE doctors SET specialization = '%s', working_hours = '%s' WHERE doctor_id = %d;", 
        specialization, working_hours, doctor_id);
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    return 1;
}

// HMS-F-030: Soft-remove a doctor profile
int remove_doctor(sqlite3 *db, int doctor_id) {
    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE doctors SET is_active = 0 WHERE doctor_id = %d;", doctor_id);
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    return 1;
}

// Callback for counting query results
static int callback_count(void *data, int argc, char **argv, char **azColName) {
    int *count = (int *)data;
    if (argc > 0 && argv[0]) {
        *count = atoi(argv[0]);
    }
    (void)azColName; // Suppresses unused parameter warning
    return 0;
}

// HMS-F-032: Display doctor's current patient queue count for the day
int get_doctor_queue_count(sqlite3 *db, int doctor_id, const char *date_str) {
    char sql[256];
    int count = 0;
    snprintf(sql, sizeof(sql), 
        "SELECT COUNT(*) FROM appointments WHERE doctor_id = %d AND appointment_date = '%s' AND status = 'Booked';", 
        doctor_id, date_str);
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, callback_count, &count, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    return count;
}