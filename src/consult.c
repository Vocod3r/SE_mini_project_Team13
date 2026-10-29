#include <string.h>
#include "consult.h"
#include "db.h"

static int allowed(const char *role) {
    return role && (strcmp(role, "Doctor") == 0 || strcmp(role, "Admin") == 0);
}

int consult_history(const char *role, int patient_id) {
    if (!allowed(role)) return -3;
    return db_get_patient_history(patient_id);
}

int consult_prescriptions(const char *role, int patient_id) {
    if (!allowed(role)) return -3;
    return db_get_prescriptions(patient_id);
}