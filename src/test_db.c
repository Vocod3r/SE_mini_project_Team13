#include <stdio.h>
#include "db.h"

int main(void) {
    db_open("hms.db");
    db_init_schema("schema.sql");

    int doc = db_add_doctor(1, "Admin", "Dr. Mehta", "General", 500.0);
    printf("doctor id = %d (expect > 0)\n", doc);

    int p = db_add_patient(1, "Receptionist", "Asha Rao", 30, "F",
                           "9876543210", "Chennai", "O+");
    printf("patient id = %d (expect > 0)\n", p);
    printf("wrong role = %d (expect -3)\n",
           db_add_patient(1, "Doctor", "X", 20, "M", "1", "a", "A+"));

    printf("search results:\n");
    db_search_patient("Asha");
    printf("wildcard %% search (expect nothing):\n");
    db_search_patient("%");
    printf("wildcard _ search (expect nothing):\n");
    db_search_patient("_");

    int a1 = db_book_appointment(1, p, doc, "2026-11-02", "10:00");
    int a2 = db_book_appointment(1, p, doc, "2026-11-02", "10:00");
    printf("first booking = %d (expect > 0), double booking = %d (expect -2)\n", a1, a2);

    db_cancel_appointment(1, a1);
    printf("rebook after cancel = %d (expect > 0)\n",
           db_book_appointment(1, p, doc, "2026-11-02", "10:00"));

    printf("schedule:\n");
    db_get_schedule(doc, "2026-11-02");

    printf("archive = %d (expect 0)\n", db_archive_patient(1, "Admin", p));
    printf("search after archive (expect nothing):\n");
    db_search_patient("Asha");

    printf("backup = %d (expect 0)\n", db_backup("backup\\hms_backup.db"));
    printf("health = %d (expect 0)\n", db_health_check());

    db_close();
    return 0;
}