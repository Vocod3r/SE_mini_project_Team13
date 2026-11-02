#include <stdio.h>
#include <sqlite3.h>
#include "console.h"
#include "db.h"

int main(int argc, char **argv) {
    const char *database = argc > 1 ? argv[1] : "hms.db";
    const char *schema = argc > 2 ? argv[2] : "schema.sql";
    sqlite3 *auth_db = NULL;
    if (db_open(database) != 0) {
        fprintf(stderr, "Cannot open database: %s\n", database);
        return 1;
    }
    if (db_init_schema(schema) != 0) {
        fprintf(stderr, "Cannot initialize schema: %s\n", schema);
        db_close(); return 1;
    }
    if (sqlite3_open(database, &auth_db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open authentication database.\n");
        if (auth_db) sqlite3_close(auth_db);
        db_close(); return 1;
    }
    int result = console_run(auth_db);
    sqlite3_close(auth_db);
    db_close();
    return result;
}
