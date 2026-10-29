#ifndef DB_H
#define DB_H
#include "sqlite3.h"

/* Return values: id or 0 on success, -1 error, -2 conflict, -3 not permitted */

typedef struct {
    int  staff_id;
    char username[64];
    char password_hash[128];
    char salt[64];
    char role[16];
    int  failed_attempts;
    long long locked_until;
} Staff;

int  db_open(const char *path);
void db_close(void);
int  db_init_schema(const char *schema_path);
int  db_log_audit(int user_id, const char *action, const char *table, int rec_id);

/* Staff and login (F-001, F-003, F-031, SR-001) */
int  db_create_staff(int user_id, const char *role, const char *username,
                     const char *password_hash, const char *salt, const char *new_role);
int  db_get_staff(const char *username, Staff *out);
int  db_is_locked(const char *username);              /* 1 locked, 0 not, -1 no such user */
int  db_record_failed_login(const char *username);    /* 1 = now locked, 0 = counted, -2 = already locked */
int  db_reset_failed_login(const char *username);

/* Doctors (F-030) */
int  db_add_doctor(int user_id, const char *role, const char *name,
                   const char *specialization, double fee);
int  db_update_doctor(int user_id, const char *role, int doctor_id, const char *name,
                      const char *spec, double fee, const char *start, const char *end);
int  db_remove_doctor(int user_id, const char *role, int doctor_id);
int  db_list_doctors(void);

/* Patients (F-010 to F-014) */
int  db_add_patient(int user_id, const char *role, const char *name, int age,
                    const char *gender, const char *phone, const char *addr, const char *bg);
int  db_update_patient(int user_id, const char *role, int patient_id, const char *name,
                       int age, const char *phone, const char *addr);
int  db_search_patient(const char *query);
int  db_archive_patient(int user_id, const char *role, int patient_id);

/* Appointments (F-020 to F-023, F-032) */
int  db_book_appointment(int user_id, int patient_id, int doctor_id,
                         const char *date, const char *slot);
int  db_reschedule_appointment(int user_id, int appt_id, const char *new_date, const char *new_slot);
int  db_cancel_appointment(int user_id, int appt_id);
int  db_get_schedule(int doctor_id, const char *date);
int  db_get_schedule_week(int doctor_id, const char *start_date);  /* F-023: 7 days, -1 bad date */
int  db_print_appt_slip(int appt_id);                              /* F-024: 0 ok, -1 not found, -2 not active */
int  db_queue_count(int doctor_id, const char *date);

/* Consultation (F-013, F-040, F-041) */
int  db_add_visit_and_rx(int user_id, const char *role, int appt_id, const char *diagnosis,
                         const char *medicines, const char *dosage, const char *notes);
int  db_get_patient_history(int patient_id);

/* Billing (F-050, F-051) */
int  db_create_bill(int user_id, const char *role, int visit_id);
int  db_set_payment_status(int user_id, const char *role, int bill_id, const char *status);

/* Reports and admin (F-060 to F-062) */
int  db_daily_report(const char *date);
int  db_backup(const char *dest_path);
int  db_health_check(void);
int  db_get_prescriptions(int patient_id);
#endif