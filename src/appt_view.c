#include <string.h>
#include "appt_view.h"
#include "db.h"

static int is_role(const char *role, const char *a, const char *b, const char *c) {
    return role && (strcmp(role, a) == 0 || (b && strcmp(role, b) == 0) || (c && strcmp(role, c) == 0));
}

int appt_schedule_day(const char *role, int doctor_id, const char *date) {
    if (!is_role(role, "Admin", "Receptionist", "Doctor")) return -3;
    if (!date) return -1;
    return db_get_schedule(doctor_id, date);
}

int appt_schedule_week(const char *role, int doctor_id, const char *start_date) {
    if (!is_role(role, "Admin", "Receptionist", "Doctor")) return -3;
    return db_get_schedule_week(doctor_id, start_date);
}

int appt_print_slip(const char *role, int appt_id) {
    if (!is_role(role, "Admin", "Receptionist", NULL)) return -3;
    return db_print_appt_slip(appt_id);
}
