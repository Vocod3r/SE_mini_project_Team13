#ifndef AUTH_H
#define AUTH_H

#include "db.h"

typedef struct {
    int user_id;
    char username[50];
    char password_hash[100];
    char role[20]; // "Admin", "Doctor", "Receptionist"
} User;

int create_staff_account(sqlite3 *db, const char *username, const char *password, const char *role);
int authenticate_user(sqlite3 *db, const char *username, const char *password, User *out_user);

#endif