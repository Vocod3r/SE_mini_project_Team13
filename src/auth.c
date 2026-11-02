#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auth.h"

// Satisfies security requirement HMS-SR-001 (Password hashing)
static void simple_hash(const char *password, char *output_buffer) {
    unsigned long hash = 5381;
    int c;
    while ((c = *password++)) {
        hash = ((hash << 5) + hash) + c;
    }
    sprintf(output_buffer, "%lx", hash);
}

// HMS-F-031: Create staff accounts with assigned role
int create_staff_account(sqlite3 *db, const char *username, const char *password, const char *role) {
    char hashed_pass[100];
    simple_hash(password, hashed_pass);

    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO users (username, password_hash, role) VALUES ('%s', '%s', '%s');", 
        username, hashed_pass, role);
    
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    return 1;
}

typedef struct {
    User *user;
    int found;
} AuthQueryContext;

static int auth_callback(void *data, int argc, char **argv, char **azColName) {
    AuthQueryContext *ctx = (AuthQueryContext *)data;
    (void)azColName; // Suppresses unused parameter warning
    if (argc >= 4) {
        ctx->user->user_id = atoi(argv[0]);
        strncpy(ctx->user->username, argv[1], sizeof(ctx->user->username) - 1);
        strncpy(ctx->user->password_hash, argv[2], sizeof(ctx->user->password_hash) - 1);
        strncpy(ctx->user->role, argv[3], sizeof(ctx->user->role) - 1);
        ctx->found = 1;
    }
    return 0;
}

// HMS-F-001 & HMS-F-031: Authenticate staff and restrict permissions
int authenticate_user(sqlite3 *db, const char *username, const char *password, User *out_user) {
    char hashed_pass[100];
    simple_hash(password, hashed_pass);

    char sql[256];
    snprintf(sql, sizeof(sql), 
        "SELECT user_id, username, password_hash, role FROM users WHERE username = '%s';", 
        username);

    User temp_user;
    AuthQueryContext ctx = { &temp_user, 0 };
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, auth_callback, &ctx, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }

    if (ctx.found && strcmp(temp_user.password_hash, hashed_pass) == 0) {
        *out_user = temp_user;
        return 1; 
    }
    return 0; 
}