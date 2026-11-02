#include <string.h>
#include "app_service.h"
#include "db.h"

int service_authenticate(sqlite3 *db, const char *username, const char *password, User *user) {
    return authenticate_user(db, username, password, user);
}

int service_create_staff(sqlite3 *db, const User *actor, const char *username,
                         const char *password, const char *role) {
    if (!actor) return 0;
    return create_staff_account(db, actor->user_id, actor->role, username, password, role);
}

int service_add_patient(const User *actor, const char *name, int age, const char *gender,
                        const char *phone, const char *address, const char *blood_group) {
    if (!actor) return -1;
    return db_add_patient(actor->user_id, actor->role, name, age, gender,
                          phone, address, blood_group);
}

int service_book_appointment(const User *actor, int patient_id, int doctor_id,
                             const char *date, const char *slot) {
    if (!actor || (strcmp(actor->role, "Admin") && strcmp(actor->role, "Receptionist")))
        return -3;
    return db_book_appointment(actor->user_id, patient_id, doctor_id, date, slot);
}

int service_record_prescription(const User *actor, int appointment_id, const char *diagnosis,
                                 const char *medicines, const char *dosage, const char *notes) {
    if (!actor) return -1;
    return db_add_visit_and_rx(actor->user_id, actor->role, appointment_id,
                               diagnosis, medicines, dosage, notes);
}
