#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "db.h"
#include "system_compliance.h"

int main() {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    assert(rc == SQLITE_OK);

    // Test HMS-NF-002: System availability and quick check
    int avail_status = check_system_availability(db);
    assert(avail_status == 0);
    printf("PASS: HMS-NF-002 availability check ok\n");

    // Test HMS-NF-003: Usability prompt verification for roles
    assert(verify_usability_prompt("Receptionist", 1) == 0);
    assert(verify_usability_prompt("Receptionist", 5) == -1); // Out of bounds menu
    assert(verify_usability_prompt("Doctor", 2) == 0);
    assert(verify_usability_prompt("Admin", 3) == 0);
    assert(verify_usability_prompt("Unknown", 1) == -1);
    printf("PASS: HMS-NF-003 usability and menu prompts ok\n");

    sqlite3_close(db);
    printf("RESULT: ALL COMPLIANCE TESTS PASSED\n");
    return 0;
}