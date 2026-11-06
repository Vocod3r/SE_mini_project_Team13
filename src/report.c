#include "report.h"
#include "db.h"

int report_daily(int user_id, const char *role, const char *date) {
    return db_print_daily_report(user_id, role, date);   /* role is checked against the staff table */
}
