#ifndef APPT_VIEW_H
#define APPT_VIEW_H
/* HMS-F-023: Admin/Receptionist/Doctor. Prints one doctor's schedule.
 * Returns number of rows, -1 error / bad date, -3 not permitted. */
int appt_schedule_day(const char *role, int doctor_id, const char *date);
int appt_schedule_week(const char *role, int doctor_id, const char *start_date);
/* HMS-F-024: Admin/Receptionist only. 0 ok, -1 not found, -2 not active (cancelled/completed), -3 not permitted */
int appt_print_slip(const char *role, int appt_id);
#endif
