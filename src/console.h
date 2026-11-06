#ifndef CONSOLE_H
#define CONSOLE_H
#include <time.h>
#include "sqlite3.h"
int console_run(sqlite3 *db);
/* Test hook: replace the clock used for the 10-minute idle logout (NULL = real time). */
void console_set_clock(time_t (*clock)(void));
#endif
