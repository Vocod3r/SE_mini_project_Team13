#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_service.h"
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

int main(void) {
    sqlite3 *auth_db = NULL;
    User admin = {0}, receptionist = {0}, doctor_user = {0};
    remove_db("t_app_service.db");
    CHECK("open isolated app-service database", db_open("t_app_service.db") == 0);
    CHECK("initialize app-service schema", db_init_schema("schema.sql") == 0);
    CHECK("open authentication connection", sqlite3_open("t_app_service.db", &auth_db) == SQLITE_OK);
    CHECK("bootstrap first Admin account", create_initial_admin(auth_db, "root", "StrongPass!42") == 1);
    CHECK("reject second bootstrap account", create_initial_admin(auth_db, "second", "StrongPass!42") == 0);
    CHECK("authenticate through service boundary",
          service_authenticate(auth_db, "root", "StrongPass!42", &admin) == 1 &&
          strcmp(admin.role, "Admin") == 0);
    CHECK("Admin service creates receptionist",
          service_create_staff(auth_db, &admin, "frontdesk", "DeskPass!42", "Receptionist") == 1 &&
          service_authenticate(auth_db, "frontdesk", "DeskPass!42", &receptionist) == 1);
    CHECK("Admin service creates doctor account",
          service_create_staff(auth_db, &admin, "clinician", "DocPass!42", "Doctor") == 1 &&
          service_authenticate(auth_db, "clinician", "DocPass!42", &doctor_user) == 1);
    CHECK("patient registration uses authenticated actor",
          service_add_patient(&receptionist, "Test Patient", 40, "F", "9000000001", "Test", "A+") > 0);
    int doctor_id = db_add_doctor(admin.user_id, admin.role, "Dr Test", "General", 100.0);
    CHECK("Admin registers doctor", doctor_id > 0);
    CHECK("Receptionist booking routes through service",
          service_book_appointment(&receptionist, 1, doctor_id, "2026-11-05", "09:00") > 0);
    CHECK("Doctor cannot use receptionist booking workflow",
          service_book_appointment(&doctor_user, 1, doctor_id, "2026-11-05", "10:00") == -3);
    CHECK("Doctor records visit and prescription",
          service_record_prescription(&doctor_user, 1, "Test diagnosis", "Medicine", "daily", "") == 1);
    if (auth_db) sqlite3_close(auth_db);
    db_close(); remove_db("t_app_service.db");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
