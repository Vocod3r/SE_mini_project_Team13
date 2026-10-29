#include <stdlib.h>
#include "sqlite3.h"
int main(void) {
    sqlite3 *d;
    sqlite3_open("hms.db", &d);
    sqlite3_exec(d, "BEGIN; INSERT INTO patients(name,phone) VALUES('CRASHTEST','111');", 0, 0, 0);
    _exit(1);
}