#include <string.h>
#include "appt_view.h"
#include <stdio.h>
#include "db.h"
#include "input.h"

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

#define QUEUE_MAX 256

int appt_queue(int user_id, const char *role, int doctor_id, const char *date) {
    static AppointmentRecord rows[QUEUE_MAX];
    if (!input_valid_date(date)) return -1;
    int n = db_get_appointments_for_day(user_id, role, doctor_id, date, rows, QUEUE_MAX);
    if (n < 0) return n;   /* -3 when the staff record does not match the role */
    printf("Patient queue for %s\n", date);
    int waiting = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(rows[i].status, "Booked") != 0 && strcmp(rows[i].status, "Rescheduled") != 0) continue;
        printf("%2d. %s  %s  (appt #%d)\n", ++waiting, rows[i].time_slot, rows[i].patient_name, rows[i].appt_id);
    }
    printf("Patients waiting: %d\n", waiting);
    return waiting;
}
