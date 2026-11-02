#ifndef SYSTEM_COMPLIANCE_H
#define SYSTEM_COMPLIANCE_H

#include "db.h"

// HMS-NF-002: Verify system availability, uptime log, and database health
int check_system_availability(sqlite3 *db);

// HMS-NF-003: Validate menu usability and role-based navigation prompt clarity
int verify_usability_prompt(const char *role, int menu_option);

#endif