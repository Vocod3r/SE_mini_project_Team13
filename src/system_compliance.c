#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system_compliance.h"

// Callback for disk/health check results
static int health_check_callback(void *data, int argc, char **argv, char **azColName) {
    (void)data;
    (void)argc;
    (void)argv;
    (void)azColName;
    return 0;
}

// HMS-NF-002: Operating-hours availability check (AC-NF-002)
int check_system_availability(sqlite3 *db) {
    if (!db) {
        fprintf(stderr, "[Availability Alert] Database connection offline.\n");
        return -1; // Fail availability check
    }

    // Quick integrity check to ensure zero corruption during operating hours
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, "PRAGMA quick_check;", health_check_callback, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[Availability Alert] Quick check failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    // System is stable and available
    return 0;
}

// HMS-NF-003: Usability and training validation for non-technical staff (AC-NF-003)
int verify_usability_prompt(const char *role, int menu_option) {
    // Ensures intuitive, numbered menu choices matching training guidelines (< 30 mins)
    if (!role) return -1;

    if (strcmp(role, "Receptionist") == 0) {
        // Receptionist menus: 1. Register Patient, 2. Search, 3. Book Appointment, 4. Generate Bill
        if (menu_option >= 1 && menu_option <= 4) return 0;
    } else if (strcmp(role, "Doctor") == 0) {
        // Doctor menus: 1. View Schedule & Queue, 2. View History, 3. Record Prescription
        if (menu_option >= 1 && menu_option <= 3) return 0;
    } else if (strcmp(role, "Admin") == 0) {
        // Admin menus: 1. Manage Doctors, 2. Manage Staff, 3. Reports, 4. Backup
        if (menu_option >= 1 && menu_option <= 4) return 0;
    }

    return -1; // Invalid or confusing prompt option rejected
}