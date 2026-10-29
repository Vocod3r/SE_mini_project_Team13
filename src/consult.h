#ifndef CONSULT_H
#define CONSULT_H
/* HMS-F-013: Doctor/Admin only. Returns visit count, -1 error, -3 not permitted */
int consult_history(const char *role, int patient_id);
/* HMS-F-041: Doctor/Admin only. Returns prescription count, -1 error, -3 not permitted */
int consult_prescriptions(const char *role, int patient_id);
#endif