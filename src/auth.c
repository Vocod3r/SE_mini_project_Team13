#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "auth.h"

#define PASSWORD_SALT_BYTES 16
#define PASSWORD_HASH_BYTES 32
#define PASSWORD_ITERATIONS 210000

static const char HEX[] = "0123456789abcdef";

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void to_hex(const unsigned char *src, size_t n, char *dst) {
    for (size_t i = 0; i < n; ++i) {
        dst[i * 2] = HEX[src[i] >> 4];
        dst[i * 2 + 1] = HEX[src[i] & 15];
    }
    dst[n * 2] = '\0';
}

static int from_hex(const char *src, unsigned char *dst, size_t n) {
    if (!src || strlen(src) != n * 2) return 0;
    for (size_t i = 0; i < n; ++i) {
        int hi = hex_nibble(src[i * 2]);
        int lo = hex_nibble(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return 1;
}

static int derive_hash(const char *password, const unsigned char *salt,
                       unsigned char *hash) {
    return PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt,
        PASSWORD_SALT_BYTES, PASSWORD_ITERATIONS, EVP_sha256(),
        PASSWORD_HASH_BYTES, hash) == 1;
}

static int valid_role(const char *role) {
    return role && (!strcmp(role, "Admin") || !strcmp(role, "Doctor") ||
                    !strcmp(role, "Receptionist"));
}

static int is_admin_actor(sqlite3 *db, int actor_id) {
    sqlite3_stmt *st = NULL;
    int valid = 0;
    if (sqlite3_prepare_v2(db,
        "SELECT 1 FROM staff WHERE staff_id=? AND role='Admin';", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, actor_id);
        valid = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return valid;
}

int create_initial_admin(sqlite3 *db, const char *username, const char *password) {
    unsigned char salt_raw[PASSWORD_SALT_BYTES], hash_raw[PASSWORD_HASH_BYTES];
    char salt[sizeof salt_raw * 2 + 1], hash[sizeof hash_raw * 2 + 1];
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (!db || !username || !*username || strlen(username) > 60 ||
        !password || !*password || strlen(password) > 1024) return 0;
    if (RAND_bytes(salt_raw, sizeof salt_raw) != 1 ||
        !derive_hash(password, salt_raw, hash_raw)) return 0;
    to_hex(salt_raw, sizeof salt_raw, salt);
    to_hex(hash_raw, sizeof hash_raw, hash);
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) return 0;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM staff;", -1, &st, NULL) != SQLITE_OK ||
        sqlite3_step(st) != SQLITE_ROW || sqlite3_column_int(st, 0) != 0) goto done;
    sqlite3_finalize(st); st = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO staff(username,password_hash,salt,role) VALUES(?,?,?,'Admin');",
        -1, &st, NULL) != SQLITE_OK) goto done;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, salt, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) goto done;
    sqlite3_finalize(st); st = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO audit_log(user_id,action,table_name,record_id) VALUES(0,'BOOTSTRAP','staff',?);",
        -1, &st, NULL) != SQLITE_OK) goto done;
    sqlite3_bind_int(st, 1, (int)sqlite3_last_insert_rowid(db));
    if (sqlite3_step(st) != SQLITE_DONE) goto done;
    sqlite3_finalize(st); st = NULL;
    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK) ok = 1;
done:
    sqlite3_finalize(st);
    if (!ok) sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    OPENSSL_cleanse(hash_raw, sizeof hash_raw);
    return ok;
}

static int legacy_hash_matches(const char *password, const char *stored) {
    unsigned long hash = 5381;
    int c;
    char legacy[32];
    while ((c = (unsigned char)*password++)) hash = ((hash << 5) + hash) + (unsigned)c;
    snprintf(legacy, sizeof legacy, "%lx", hash);
    return stored && strcmp(legacy, stored) == 0;
}

/* F-031/SR-001: create accounts in the canonical staff table. The actor role
 * is checked here because this function is itself a direct data-access path. */
int create_staff_account(sqlite3 *db, int actor_id, const char *actor_role,
                         const char *username, const char *password,
                         const char *role) {
    unsigned char salt_raw[PASSWORD_SALT_BYTES], hash_raw[PASSWORD_HASH_BYTES];
    char salt[sizeof salt_raw * 2 + 1], hash[sizeof hash_raw * 2 + 1];
    sqlite3_stmt *st = NULL;
    int result = 0;
    if (!db || actor_id <= 0 || !actor_role || strcmp(actor_role, "Admin") != 0 ||
        !is_admin_actor(db, actor_id) ||
        !username || !*username || strlen(username) > 60 || !password ||
        !*password || strlen(password) > 1024 || !valid_role(role)) return 0;
    if (RAND_bytes(salt_raw, sizeof salt_raw) != 1 ||
        !derive_hash(password, salt_raw, hash_raw)) return 0;
    to_hex(salt_raw, sizeof salt_raw, salt);
    to_hex(hash_raw, sizeof hash_raw, hash);
    if (sqlite3_exec(db, "SAVEPOINT create_staff_account;", NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO staff(username,password_hash,salt,role) VALUES(?,?,?,?);",
        -1, &st, NULL) != SQLITE_OK) goto done;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, salt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, role, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) goto done;
    sqlite3_finalize(st);
    st = NULL;

    if (sqlite3_prepare_v2(db,
        "INSERT INTO audit_log(user_id,action,table_name,record_id) VALUES(?,'CREATE','staff',?);",
        -1, &st, NULL) != SQLITE_OK) goto done;
    sqlite3_bind_int(st, 1, actor_id);
    sqlite3_bind_int(st, 2, (int)sqlite3_last_insert_rowid(db));
    if (sqlite3_step(st) != SQLITE_DONE) goto done;
    sqlite3_finalize(st);
    st = NULL;
    if (sqlite3_exec(db, "RELEASE create_staff_account;", NULL, NULL, NULL) == SQLITE_OK)
        result = 1;
    return result;

done:
    sqlite3_finalize(st);
    sqlite3_exec(db, "ROLLBACK TO create_staff_account;", NULL, NULL, NULL);
    sqlite3_exec(db, "RELEASE create_staff_account;", NULL, NULL, NULL);
    return 0;
}

typedef struct {
    int id;
    int failed_attempts;
    long long locked_until;
    char username[64];
    char password_hash[128];
    char salt[64];
    char role[16];
} AuthRecord;

static int load_staff(sqlite3 *db, const char *username, AuthRecord *record) {
    sqlite3_stmt *st = NULL;
    int found = 0;
    if (sqlite3_prepare_v2(db,
        "SELECT staff_id,username,password_hash,salt,role,failed_attempts,locked_until "
        "FROM staff WHERE username=?;", -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        record->id = sqlite3_column_int(st, 0);
        snprintf(record->username, sizeof record->username, "%s", sqlite3_column_text(st, 1));
        snprintf(record->password_hash, sizeof record->password_hash, "%s", sqlite3_column_text(st, 2));
        snprintf(record->salt, sizeof record->salt, "%s", sqlite3_column_text(st, 3));
        snprintf(record->role, sizeof record->role, "%s", sqlite3_column_text(st, 4));
        record->failed_attempts = sqlite3_column_int(st, 5);
        record->locked_until = sqlite3_column_int64(st, 6);
        found = 1;
    }
    sqlite3_finalize(st);
    return found;
}

static int save_failure(sqlite3 *db, const AuthRecord *record) {
    sqlite3_stmt *st = NULL;
    int locked = 0;
    if (sqlite3_exec(db, "SAVEPOINT failed_login;", NULL, NULL, NULL) != SQLITE_OK) return 0;
    if (sqlite3_prepare_v2(db,
        "UPDATE staff SET failed_attempts=failed_attempts+1 WHERE staff_id=?;",
        -1, &st, NULL) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, record->id);
    if (sqlite3_step(st) != SQLITE_DONE) goto fail;
    sqlite3_finalize(st); st = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE staff SET locked_until=CAST(strftime('%s','now') AS INTEGER)+300, "
        "failed_attempts=0 WHERE staff_id=? AND failed_attempts>=3;",
        -1, &st, NULL) != SQLITE_OK) goto fail;
    sqlite3_bind_int(st, 1, record->id);
    if (sqlite3_step(st) != SQLITE_DONE) goto fail;
    locked = sqlite3_changes(db) > 0;
    sqlite3_finalize(st); st = NULL;
    if (locked) {
        if (sqlite3_prepare_v2(db,
            "INSERT INTO audit_log(user_id,action,table_name,record_id) VALUES(?,'LOCKOUT','staff',?);",
            -1, &st, NULL) != SQLITE_OK) goto fail;
        sqlite3_bind_int(st, 1, record->id);
        sqlite3_bind_int(st, 2, record->id);
        if (sqlite3_step(st) != SQLITE_DONE) goto fail;
        sqlite3_finalize(st); st = NULL;
    }
    if (sqlite3_exec(db, "RELEASE failed_login;", NULL, NULL, NULL) == SQLITE_OK) return 1;
fail:
    sqlite3_finalize(st);
    sqlite3_exec(db, "ROLLBACK TO failed_login;", NULL, NULL, NULL);
    sqlite3_exec(db, "RELEASE failed_login;", NULL, NULL, NULL);
    return 0;
}

int authenticate_user(sqlite3 *db, const char *username, const char *password,
                      User *out_user) {
    AuthRecord record = {0};
    unsigned char salt_raw[PASSWORD_SALT_BYTES], hash_raw[PASSWORD_HASH_BYTES];
    char salt[sizeof salt_raw * 2 + 1], hash[sizeof hash_raw * 2 + 1];
    int loaded, verified = 0;
    sqlite3_stmt *st = NULL;
    if (!db || !username || !*username || !password || !out_user) return 0;
    loaded = load_staff(db, username, &record);
    if (loaded != 1) return 0;
    if (record.locked_until > (long long)time(NULL)) return 0;
    if (record.salt[0] == '\0') {
        /* Old users-table hashes were DJB2. Keep those accounts usable once,
         * then upgrade their stored credentials to PBKDF2 on successful login. */
        verified = legacy_hash_matches(password, record.password_hash);
        if (verified) {
            if (RAND_bytes(salt_raw, sizeof salt_raw) != 1 ||
                !derive_hash(password, salt_raw, hash_raw)) return 0;
            to_hex(salt_raw, sizeof salt_raw, salt);
            to_hex(hash_raw, sizeof hash_raw, hash);
            if (sqlite3_prepare_v2(db,
                "UPDATE staff SET password_hash=?,salt=?,failed_attempts=0,locked_until=0 "
                "WHERE staff_id=? AND salt='';", -1, &st, NULL) != SQLITE_OK) return 0;
            sqlite3_bind_text(st, 1, hash, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, salt, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 3, record.id);
            verified = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) == 1;
            sqlite3_finalize(st); st = NULL;
        }
    } else if (from_hex(record.salt, salt_raw, sizeof salt_raw) &&
               from_hex(record.password_hash, hash_raw, sizeof hash_raw)) {
        unsigned char candidate[PASSWORD_HASH_BYTES];
        verified = derive_hash(password, salt_raw, candidate) &&
                   CRYPTO_memcmp(candidate, hash_raw, sizeof candidate) == 0;
        OPENSSL_cleanse(candidate, sizeof candidate);
    }
    if (!verified) {
        if (!save_failure(db, &record)) return -1;
        return 0;
    }
    if (record.salt[0] != '\0') {
        if (sqlite3_prepare_v2(db,
            "UPDATE staff SET failed_attempts=0,locked_until=0 WHERE staff_id=?;",
            -1, &st, NULL) != SQLITE_OK) return 0;
        sqlite3_bind_int(st, 1, record.id);
        if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return 0; }
        sqlite3_finalize(st);
    }
    out_user->user_id = record.id;
    snprintf(out_user->username, sizeof out_user->username, "%s", record.username);
    snprintf(out_user->password_hash, sizeof out_user->password_hash, "%s", record.password_hash);
    snprintf(out_user->role, sizeof out_user->role, "%s", record.role);
    return 1;
}
