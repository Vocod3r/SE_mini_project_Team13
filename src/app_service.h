#ifndef APP_SERVICE_H
#define APP_SERVICE_H

#include "auth.h"

int service_authenticate(sqlite3 *db, const char *username, const char *password, User *user);
int service_create_staff(sqlite3 *db, const User *actor, const char *username,
                         const char *password, const char *role);
int service_add_patient(const User *actor, const char *name, int age, const char *gender,
                        const char *phone, const char *address, const char *blood_group);
int service_book_appointment(const User *actor, int patient_id, int doctor_id,
                             const char *date, const char *slot);
int service_record_prescription(const User *actor, int appointment_id, const char *diagnosis,
                                 const char *medicines, const char *dosage, const char *notes);

#endif
