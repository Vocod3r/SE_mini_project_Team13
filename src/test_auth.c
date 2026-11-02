#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auth.h"
#include "db.h"

static int failures;
#define CHECK(label, condition) do { \
    int ok_ = !!(condition); \
    printf("%s: %s\n", ok_ ? "PASS" : "FAIL", label); \
    if (!ok_) failures++; \
} while (0)

static void remove_test_db(const char *path) {
    char aux[256];
    remove(path);
    snprintf(aux, sizeof aux, "%s-wal", path); remove(aux);
    snprintf(aux, sizeof aux, "%s-shm", path); remove(aux);
}

static void legacy_hash(const char *password, char out[32]) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*password++)) hash = ((hash << 5) + hash) + (unsigned)c;
    snprintf(out, 32, "%lx", hash);
}

static int read_staff(sqlite3 *db, const char *username, char *hash, size_t hash_cap,
                      char *salt, size_t salt_cap) {
    sqlite3_stmt *st = NULL;
    int found = 0;
    if (sqlite3_prepare_v2(db,
        "SELECT password_hash,salt FROM staff WHERE username=?;", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            snprintf(hash, hash_cap, "%s", sqlite3_column_text(st, 0));
            snprintf(salt, salt_cap, "%s", sqlite3_column_text(st, 1));
            found = 1;
        }
    }
    sqlite3_finalize(st);
    return found;
}

static int staff_count(sqlite3 *db, const char *username) {
    sqlite3_stmt *st = NULL;
    int count = -1;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM staff WHERE username=?;", -1,
                           &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return count;
}

int main(void) {
    sqlite3 *db = NULL;
    User admin = {0}, receptionist = {0};
    char old_hash[32], stored_hash[128], salt[64];
    remove_test_db("t_auth.db");
    CHECK("open isolated auth database", db_open("t_auth.db") == 0);
    CHECK("initialize auth schema", db_init_schema("schema.sql") == 0);
    CHECK("open auth test handle", sqlite3_open("t_auth.db", &db) == SQLITE_OK);

    legacy_hash("LegacyPass!42", old_hash);
    sqlite3_stmt *st = NULL;
    CHECK("insert legacy account fixture", sqlite3_prepare_v2(db,
        "INSERT INTO users(username,password_hash,role) VALUES('legacy-admin',?,'Admin');",
        -1, &st, NULL) == SQLITE_OK);
    sqlite3_bind_text(st, 1, old_hash, -1, SQLITE_TRANSIENT);
    CHECK("save legacy account fixture", sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    CHECK("schema initialization migrates legacy account", db_init_schema("schema.sql") == 0 &&
          staff_count(db, "legacy-admin") == 1);

    CHECK("legacy login succeeds and upgrades stored hash",
          authenticate_user(db, "legacy-admin", "LegacyPass!42", &admin) == 1 &&
          strcmp(admin.role, "Admin") == 0 &&
          read_staff(db, "legacy-admin", stored_hash, sizeof stored_hash, salt, sizeof salt) &&
          strlen(stored_hash) == 64 && strlen(salt) == 32 && strcmp(stored_hash, old_hash) != 0);
    CHECK("legacy users row remains preserved",
          sqlite3_prepare_v2(db, "SELECT count(*) FROM users WHERE username='legacy-admin';",
                             -1, &st, NULL) == SQLITE_OK &&
          sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st, 0) == 1);
    sqlite3_finalize(st); st = NULL;

    CHECK("Admin can create a staff account",
          create_staff_account(db, admin.user_id, "Admin", "frontdesk", "DeskPass!42",
                               "Receptionist") == 1);
    CHECK("new account authenticates with assigned role",
          authenticate_user(db, "frontdesk", "DeskPass!42", &receptionist) == 1 &&
          strcmp(receptionist.role, "Receptionist") == 0);
    CHECK("wrong password is rejected", authenticate_user(db, "frontdesk", "wrong", &receptionist) == 0);
    CHECK("SQL injection string is treated as a username",
          authenticate_user(db, "' OR 1=1 --", "DeskPass!42", &receptionist) == 0);
    CHECK("non-admin actor is rejected",
          create_staff_account(db, admin.user_id, "Doctor", "blocked", "AnyPass!42", "Doctor") == 0 &&
          staff_count(db, "blocked") == 0);
    CHECK("random salt and PBKDF2 hash are stored",
          read_staff(db, "frontdesk", stored_hash, sizeof stored_hash, salt, sizeof salt) &&
          strlen(stored_hash) == 64 && strlen(salt) == 32 &&
          strcmp(stored_hash, "DeskPass!42") != 0 && strcmp(salt, "DeskPass!42") != 0);

    CHECK("audit failure prevents account creation", sqlite3_exec(db,
          "CREATE TRIGGER reject_staff_audit BEFORE INSERT ON audit_log "
          "WHEN NEW.table_name='staff' AND NEW.action='CREATE' "
          "BEGIN SELECT RAISE(ABORT,'audit unavailable'); END;", NULL, NULL, NULL) == SQLITE_OK &&
          create_staff_account(db, admin.user_id, "Admin", "audit-blocked", "AnyPass!42", "Doctor") == 0 &&
          staff_count(db, "audit-blocked") == 0);
    sqlite3_exec(db, "DROP TRIGGER reject_staff_audit;", NULL, NULL, NULL);

    CHECK("third invalid login locks account", authenticate_user(db, "frontdesk", "bad", &receptionist) == 0 &&
          authenticate_user(db, "frontdesk", "bad", &receptionist) == 0 &&
          authenticate_user(db, "frontdesk", "bad", &receptionist) == 0);
    CHECK("locked account rejects correct password",
          authenticate_user(db, "frontdesk", "DeskPass!42", &receptionist) == 0);
    CHECK("lockout audit event is recorded",
          sqlite3_prepare_v2(db, "SELECT count(*) FROM audit_log WHERE action='LOCKOUT' AND table_name='staff';",
                             -1, &st, NULL) == SQLITE_OK &&
          sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st, 0) == 1);
    sqlite3_finalize(st);

    sqlite3_close(db);
    db_close();
    remove_test_db("t_auth.db");
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
