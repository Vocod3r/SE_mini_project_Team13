#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"
#include "db.h"

#ifdef _WIN32
int main(void) {
    puts("SKIP: process-interruption recovery harness is POSIX-only; no Windows result claimed");
    return 0;
}
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(label, condition) do { \
    int ok_ = !!(condition); \
    printf("%s: %s\n", ok_ ? "PASS" : "FAIL", label); \
    if (!ok_) failures++; \
} while (0)

static int scalar_int(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    int value = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) value = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return value;
}

static int scalar_text_is(sqlite3 *db, const char *sql, const char *expected) {
    sqlite3_stmt *st = NULL;
    int matched = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(st, 0);
        matched = value && strcmp((const char *)value, expected) == 0;
    }
    sqlite3_finalize(st);
    return matched;
}

static int integrity_ok(sqlite3 *db) {
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *result = sqlite3_column_text(st, 0);
        ok = result && strcmp((const char *)result, "ok") == 0 &&
             sqlite3_step(st) == SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return ok;
}

static void remove_database(const char *path) {
    char sidecar[256];
    remove(path);
    snprintf(sidecar, sizeof sidecar, "%s-wal", path); remove(sidecar);
    snprintf(sidecar, sizeof sidecar, "%s-shm", path); remove(sidecar);
}

static int insert_patient(sqlite3 *db, const char *name, const char *phone) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO patients(name,age,gender,phone,address,blood_group) "
        "VALUES(?1,30,'M',?2,'Test','O+');", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, phone, -1, SQLITE_TRANSIENT);
    int ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

int main(void) {
    const char *path = "t_reliability.db";
    sqlite3 *db = NULL;
    int pipe_fd[2] = {-1, -1};
    remove_database(path);
    CHECK("reject empty database path", db_open("") == -1);
    CHECK("open isolated database with production WAL/FULL configuration", db_open(path) == 0);
    CHECK("refuse to replace an open database handle", db_open(":memory:") == -1);
    db_close();
    CHECK("create recovery fixture table", sqlite3_open(path, &db) == SQLITE_OK &&
          scalar_text_is(db, "PRAGMA journal_mode;", "wal") &&
          sqlite3_exec(db, "CREATE TABLE patients(patient_id INTEGER PRIMARY KEY, name TEXT, age INTEGER, "
                           "gender TEXT, phone TEXT, address TEXT, blood_group TEXT);", NULL, NULL, NULL) == SQLITE_OK &&
          insert_patient(db, "committed-before-child", "9000000001") &&
          sqlite3_close(db) == SQLITE_OK);
    db = NULL;

    CHECK("create child readiness pipe", pipe(pipe_fd) == 0);
    pid_t child = fork();
    CHECK("fork interruption worker", child >= 0);
    if (child == 0) {
        close(pipe_fd[0]);
        sqlite3 *child_db = NULL;
        int ready = sqlite3_open(path, &child_db) == SQLITE_OK &&
                    sqlite3_exec(child_db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;",
                                 NULL, NULL, NULL) == SQLITE_OK &&
                    sqlite3_exec(child_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) == SQLITE_OK &&
                    insert_patient(child_db, "committed-before-crash", "9000000002") &&
                    sqlite3_exec(child_db, "COMMIT; BEGIN IMMEDIATE;", NULL, NULL, NULL) == SQLITE_OK &&
                    insert_patient(child_db, "interrupted-write", "9000000003");
        char signal = ready ? 'R' : 'E';
        ssize_t written = write(pipe_fd[1], &signal, 1);
        (void)written;
        _exit(ready ? 0 : 2); /* deliberately bypass SQLite close/rollback handlers */
    }
    if (child < 0) {
        if (pipe_fd[0] >= 0) close(pipe_fd[0]);
        if (pipe_fd[1] >= 0) close(pipe_fd[1]);
        remove_database(path);
        return 1;
    }
    close(pipe_fd[1]);
    char signal = 0;
    ssize_t bytes_read = read(pipe_fd[0], &signal, 1);
    close(pipe_fd[0]);
    int child_status = 0;
    pid_t waited = waitpid(child, &child_status, 0);
    CHECK("child exited with an open interrupted transaction",
          bytes_read == 1 && signal == 'R' && waited == child &&
          WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);

    CHECK("reopen database after interrupted write", sqlite3_open(path, &db) == SQLITE_OK);
    CHECK("WAL recovery preserves the pre-existing committed row", db &&
          scalar_int(db, "SELECT count(*) FROM patients WHERE name='committed-before-child'") == 1);
    CHECK("WAL recovery preserves the child's committed row", db &&
          scalar_int(db, "SELECT count(*) FROM patients WHERE name='committed-before-crash'") == 1);
    CHECK("WAL recovery rolls back incomplete row", db &&
          scalar_int(db, "SELECT count(*) FROM patients WHERE name='interrupted-write'") == 0);
    CHECK("database integrity passes after recovery", db && integrity_ok(db));
    if (db) sqlite3_close(db);
    remove_database(path);
    printf(failures ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
#endif
