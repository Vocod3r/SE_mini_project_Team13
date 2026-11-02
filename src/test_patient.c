#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "db.h"
#include "patient.h"

static int fails = 0;
#define CHECK(name, cond) do { int ok_ = !!(cond); \
                               printf("%s: %s\n", ok_ ? "PASS" : "FAIL", name); \
                               if (!ok_) fails++; } while (0)

static double monotonic_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)) return -1.0;
    return 1000.0 * (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1.0;
    return 1000.0 * (double)ts.tv_sec + (double)ts.tv_nsec / 1000000.0;
#endif
}

int main(void) {
    remove("t_patient.db"); remove("t_patient.db-wal"); remove("t_patient.db-shm");
    db_open("t_patient.db");
    db_init_schema("schema.sql");

    int admin_id = db_create_staff(0, "Admin", "admin", "testhash", "testsalt", "Admin");
    int receptionist_id = db_create_staff(admin_id, "Admin", "reception", "testhash", "testsalt", "Receptionist");
    int doc = db_add_doctor(admin_id, "Admin", "Dr. Mehta", "General", 500.0);
    int p = db_add_patient(receptionist_id, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");

    printf("--- HMS-F-011 patient_search ---\n");
    CHECK("search by name",  patient_search("Asha") == 1);
    CHECK("search by phone", patient_search("9876543210") == 1);
    CHECK("search by id",   patient_search("1") == 1);
    CHECK("no match",       patient_search("Nobody") == 0);
    CHECK("% is literal, not wildcard", patient_search("%") == 0);
    CHECK("_ is literal, not wildcard", patient_search("_") == 0);

    /* Seed performance fixtures in one isolated transaction. The timed
     * measurement is for lookups, not 10,000 individual audited writes. */
    sqlite3 *seed_db = NULL;
    sqlite3_stmt *seed = NULL;
    int seed_ok = sqlite3_open("t_patient.db", &seed_db) == SQLITE_OK &&
                  sqlite3_exec(seed_db, "BEGIN;", NULL, NULL, NULL) == SQLITE_OK &&
                  sqlite3_prepare_v2(seed_db,
                      "INSERT INTO patients(name,age,gender,phone,address,blood_group) "
                      "VALUES(?1,40,'M',?2,'X','A+');", -1, &seed, NULL) == SQLITE_OK;
    for (int i = 0; seed_ok && i < 9999; i++) {
        char nm[32], ph[16];
        snprintf(nm, sizeof nm, "Bulk%d", i);
        snprintf(ph, sizeof ph, "8%09d", i);
        sqlite3_bind_text(seed, 1, nm, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(seed, 2, ph, -1, SQLITE_TRANSIENT);
        seed_ok = sqlite3_step(seed) == SQLITE_DONE;
        sqlite3_reset(seed);
        sqlite3_clear_bindings(seed);
    }
    sqlite3_finalize(seed);
    if (seed_ok) seed_ok = sqlite3_exec(seed_db, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK;
    else sqlite3_exec(seed_db, "ROLLBACK;", NULL, NULL, NULL);
    sqlite3_stmt *count_stmt = NULL;
    int patient_count = -1;
    if (seed_ok && sqlite3_prepare_v2(seed_db, "SELECT count(*) FROM patients;", -1,
                                      &count_stmt, NULL) == SQLITE_OK &&
        sqlite3_step(count_stmt) == SQLITE_ROW)
        patient_count = sqlite3_column_int(count_stmt, 0);
    sqlite3_finalize(count_stmt);
    CHECK("seed exactly 10,000 patient records", seed_ok && patient_count == 10000);

    int doctor_ids[10] = {doc};
    for (int i = 1; i < 10; ++i) {
        char name[32];
        snprintf(name, sizeof name, "Dr. Perf%d", i);
        doctor_ids[i] = db_add_doctor(admin_id, "Admin", name, "General", 100.0);
    }
    sqlite3_stmt *appt_seed = NULL;
    int appt_seed_ok = seed_db != NULL && patient_count == 10000;
    for (int i = 0; i < 10; ++i) appt_seed_ok = appt_seed_ok && doctor_ids[i] > 0;
    appt_seed_ok = appt_seed_ok && sqlite3_exec(seed_db, "BEGIN;", NULL, NULL, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(seed_db,
            "INSERT INTO appointments(patient_id,doctor_id,appt_date,time_slot) "
            "VALUES(?1,?2,date('2026-11-01','+'||?3||' days'),'09:00');",
            -1, &appt_seed, NULL) == SQLITE_OK;
    for (int i = 0; appt_seed_ok && i < 10000; ++i) {
        sqlite3_bind_int(appt_seed, 1, i + 1);
        sqlite3_bind_int(appt_seed, 2, doctor_ids[i % 10]);
        sqlite3_bind_int(appt_seed, 3, i / 10);
        appt_seed_ok = sqlite3_step(appt_seed) == SQLITE_DONE;
        sqlite3_reset(appt_seed);
        sqlite3_clear_bindings(appt_seed);
    }
    sqlite3_finalize(appt_seed);
    if (appt_seed_ok) appt_seed_ok = sqlite3_exec(seed_db, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK;
    else sqlite3_exec(seed_db, "ROLLBACK;", NULL, NULL, NULL);
    CHECK("seed exactly 10,000 appointment records", appt_seed_ok &&
          sqlite3_prepare_v2(seed_db, "SELECT count(*) FROM appointments;", -1,
                             &count_stmt, NULL) == SQLITE_OK &&
          sqlite3_step(count_stmt) == SQLITE_ROW && sqlite3_column_int(count_stmt, 0) == 10000);
    sqlite3_finalize(count_stmt);
    CHECK("performance fixture has no open statements",
          seed_db && sqlite3_next_stmt(seed_db, NULL) == NULL);
    int seed_close_rc = seed_db ? sqlite3_close(seed_db) : SQLITE_MISUSE;
    seed_db = NULL;
    CHECK("close performance fixture database", seed_close_rc == SQLITE_OK);

    PatientRecord patient_record;
    int id_ok = 1, name_ok = 1, phone_ok = 1, appt_ok = 1, day_ok = 1;
    double t0 = monotonic_ms();
    for (int i = 0; i < 50; ++i)
        id_ok = id_ok && db_find_patients(receptionist_id, "Receptionist", "10000",
                                          &patient_record, 1) == 1 &&
                patient_record.patient_id == 10000;
    double id_ms = monotonic_ms() - t0;
    t0 = monotonic_ms();
    for (int i = 0; i < 50; ++i)
        name_ok = name_ok && db_find_patients(receptionist_id, "Receptionist", "Bulk7777",
                                              &patient_record, 1) == 1 &&
                  strcmp(patient_record.name, "Bulk7777") == 0;
    double name_ms = monotonic_ms() - t0;
    t0 = monotonic_ms();
    for (int i = 0; i < 50; ++i)
        phone_ok = phone_ok && db_find_patients(receptionist_id, "Receptionist", "8000005000",
                                                &patient_record, 1) == 1 &&
                   strcmp(patient_record.phone, "8000005000") == 0;
    double phone_ms = monotonic_ms() - t0;
    AppointmentRecord appointment_record;
    int appointment_id = 10000;
    t0 = monotonic_ms();
    for (int i = 0; i < 50; ++i)
        appt_ok = appt_ok && db_get_appointment_by_id(receptionist_id, "Receptionist",
                                                      appointment_id, &appointment_record) == 0 &&
                  appointment_record.appt_id == appointment_id &&
                  appointment_record.patient_id == appointment_id;
    double appointment_ms = monotonic_ms() - t0;
    AppointmentRecord day_records[1];
    t0 = monotonic_ms();
    for (int i = 0; i < 50; ++i)
        day_ok = day_ok && db_get_appointments_for_day(receptionist_id, "Receptionist",
                                                      appointment_record.doctor_id,
                                                      appointment_record.appt_date,
                                                      day_records, 1) == 1 &&
                 day_records[0].appt_id == appointment_id;
    double appointment_day_ms = monotonic_ms() - t0;
    printf("50 patient ID lookups: %.2f ms; 50 name lookups: %.2f ms; "
           "50 phone lookups: %.2f ms; 50 appointment ID lookups: %.2f ms; "
           "50 doctor/date appointment lookups: %.2f ms\n",
           id_ms, name_ms, phone_ms, appointment_ms, appointment_day_ms);
    CHECK("50 patient ID lookups under 2 seconds", id_ok && id_ms >= 0.0 && id_ms < 2000.0);
    CHECK("50 patient name lookups under 2 seconds", name_ok && name_ms >= 0.0 && name_ms < 2000.0);
    CHECK("50 patient phone lookups under 2 seconds", phone_ok && phone_ms >= 0.0 && phone_ms < 2000.0);
    CHECK("50 appointment lookups under 2 seconds", appt_ok && appointment_ms >= 0.0 && appointment_ms < 2000.0);
    CHECK("50 doctor/date appointment lookups under 2 seconds",
          day_ok && appointment_day_ms >= 0.0 && appointment_day_ms < 2000.0);

    printf("--- HMS-F-014 patient_archive ---\n");
    CHECK("non-admin refused", patient_archive(1, "Receptionist", p) == -3);
    CHECK("admin archives",    patient_archive(1, "Admin", p) == 0);
    CHECK("hidden from search", patient_search("Asha") == 0);
    CHECK("hidden from booking",
          db_book_appointment(receptionist_id, p, doc, "2026-11-05", "10:00") == -1);

    db_close();
    remove("t_patient.db"); remove("t_patient.db-wal"); remove("t_patient.db-shm");
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
