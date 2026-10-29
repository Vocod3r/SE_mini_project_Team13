#ifndef PATIENT_H
#define PATIENT_H
/* HMS-F-011: returns number of matches, -1 on error */
int patient_search(const char *query);
/* HMS-F-014: Admin only. 0 ok, -1 error, -3 not permitted */
int patient_archive(int user_id, const char *role, int patient_id);
#endif