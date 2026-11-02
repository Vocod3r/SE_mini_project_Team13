#ifndef DOCTOR_MANAGER_H
#define DOCTOR_MANAGER_H

#include "db.h"

typedef struct {
    int doctor_id;
    char name[50];
    char specialization[50];
    char working_hours[30];
    int is_active;
} Doctor;

int add_doctor(sqlite3 *db, const char *name, const char *specialization, const char *working_hours);
int update_doctor(sqlite3 *db, int doctor_id, const char *specialization, const char *working_hours);
int remove_doctor(sqlite3 *db, int doctor_id);
int get_doctor_queue_count(sqlite3 *db, int doctor_id, const char *date_str);

#endif