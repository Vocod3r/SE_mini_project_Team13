#include "patient.h"
#include "db.h"

int patient_search(const char *query) {
    return db_search_patient(query);
}

int patient_archive(int user_id, const char *role, int patient_id) {
    return db_archive_patient(user_id, role, patient_id);
}