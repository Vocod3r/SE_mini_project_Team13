#ifndef AUTH_H
#define AUTH_H

#include "db.h"

typedef struct {
    int user_id;
    char username[64];
    char password_hash[128];
    char role[20]; // "Admin", "Doctor", "Receptionist"
} User;

/* Admin-only account creation; actor_id is checked against the staff table.
 * Returns 1 on success and 0 on rejection/error. */
int create_staff_account(sqlite3 *db, int actor_id, const char *actor_role,
                         const char *username, const char *password, const char *role);
int authenticate_user(sqlite3 *db, const char *username, const char *password, User *out_user);

#endif
