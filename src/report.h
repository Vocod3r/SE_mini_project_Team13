#ifndef REPORT_H
#define REPORT_H
/* HMS-F-060: Admin only. Prints all appointments on a date (every status) and a status summary.
 * Returns total appointments, -1 error / bad date, -3 not permitted. */
int report_daily(int user_id, const char *role, const char *date);
#endif
