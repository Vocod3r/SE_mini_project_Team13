#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include "console.h"
#include "app_service.h"
#include "appt_view.h"
#include "consult.h"
#include "db.h"

static int input(const char *label, char *out, size_t cap) {
    printf("%s", label);
    if (!fgets(out, (int)cap, stdin)) return 0;
    size_t n = strlen(out);
    if (n && out[n - 1] == '\n') out[n - 1] = '\0';
    else { int c; while ((c = getchar()) != '\n' && c != EOF) {} }
    return 1;
}

static int password_input(const char *label, char *out, size_t cap) {
    printf("%s", label); fflush(stdout);
#ifdef _WIN32
    if (_isatty(_fileno(stdin))) {
        size_t n = 0; int c;
        while ((c = _getch()) != '\r' && c != '\n') {
            if ((c == '\b' || c == 127) && n) { --n; fputs("\b \b", stdout); }
            else if (c >= 32 && n + 1 < cap) { out[n++] = (char)c; putchar('*'); }
        }
        out[n] = '\0'; putchar('\n'); return 1;
    }
#else
    if (isatty(STDIN_FILENO)) {
        struct termios old_state, hidden_state;
        if (tcgetattr(STDIN_FILENO, &old_state) != 0) return 0;
        hidden_state = old_state; hidden_state.c_lflag &= (tcflag_t)~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden_state) != 0) return 0;
        char *read_result = fgets(out, (int)cap, stdin);
        int restore_result = tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_state);
        putchar('\n');
        if (!read_result || restore_result != 0) return 0;
        size_t n = strlen(out);
        if (n && out[n - 1] == '\n') out[n - 1] = '\0';
        else { int c; while ((c = getchar()) != '\n' && c != EOF) {} }
        return 1;
    }
#endif
    return input("", out, cap);
}

static int number(const char *label, int *value) {
    char text[64], *end = NULL;
    long parsed;
    if (!input(label, text, sizeof text) || !*text) return 0;
    parsed = strtol(text, &end, 10);
    if (*end || parsed < 0 || parsed > 2147483647L) return 0;
    *value = (int)parsed;
    return 1;
}

static int staff_count(sqlite3 *db) {
    sqlite3_stmt *st = NULL;
    int count = -1;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM staff;", -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return count;
}

static void add_patient(const User *u) {
    char name[128], gender[16], phone[32], address[256], bg[16]; int age;
    if (!input("Name: ", name, sizeof name) || !number("Age: ", &age) ||
        !input("Gender: ", gender, sizeof gender) || !input("Phone: ", phone, sizeof phone) ||
        !input("Address: ", address, sizeof address) || !input("Blood group: ", bg, sizeof bg)) {
        puts("Invalid or incomplete input."); return;
    }
    printf("Patient result: %d\n", service_add_patient(u, name, age, gender, phone, address, bg));
}

static void search_patient(const User *u) {
    char query[128]; PatientRecord rows[128];
    if (!input("Patient name, phone, or ID: ", query, sizeof query)) return;
    int n = db_find_patients(u->user_id, u->role, query, rows, 128);
    if (n < 0) { printf("Search failed (%d).\n", n); return; }
    for (int i = 0; i < n; ++i)
        printf("%d | %s | age %d | %s\n", rows[i].patient_id, rows[i].name,
               rows[i].age, rows[i].phone);
    if (!n) puts("No matching patients.");
}

static void book(const User *u) {
    int pid, did; char date[16], slot[64];
    if (!number("Patient ID: ", &pid) || !number("Doctor ID: ", &did) ||
        !input("Date (YYYY-MM-DD): ", date, sizeof date) ||
        !input("Time slot: ", slot, sizeof slot)) { puts("Invalid input."); return; }
    printf("Appointment result: %d\n", service_book_appointment(u, pid, did, date, slot));
}

static void consult(const User *u) {
    int id; char diagnosis[256], medicines[256], dosage[256], notes[512];
    if (!number("Appointment ID: ", &id) || !input("Diagnosis: ", diagnosis, sizeof diagnosis) ||
        !input("Medicines: ", medicines, sizeof medicines) || !input("Dosage: ", dosage, sizeof dosage) ||
        !input("Notes: ", notes, sizeof notes)) { puts("Invalid input."); return; }
    printf("Visit/prescription result: %d\n", service_record_prescription(
        u, id, diagnosis, medicines, dosage, notes));
}

static void create_staff(sqlite3 *db, const User *u) {
    char username[64], password[256], role[24];
    if (!input("New username: ", username, sizeof username) ||
        !password_input("Temporary password: ", password, sizeof password) ||
        !input("Role (Admin/Doctor/Receptionist): ", role, sizeof role)) return;
    puts(service_create_staff(db, u, username, password, role) ?
         "Staff account created." : "Account creation failed or permission denied.");
}

static void add_doctor(const User *u) {
    char name[128], spec[128]; int fee;
    if (!input("Doctor name: ", name, sizeof name) || !input("Specialization: ", spec, sizeof spec) ||
        !number("Fee (whole currency units): ", &fee)) return;
    printf("Doctor ID: %d\n", db_add_doctor(u->user_id, u->role, name, spec, (double)fee));
}

static void schedule(const User *u) {
    int doctor; char date[16];
    if (!number("Doctor ID: ", &doctor) || !input("Date (YYYY-MM-DD): ", date, sizeof date)) return;
    printf("Schedule rows: %d\n", appt_schedule_day(u->role, doctor, date));
}

static int role_allows(const char *role, int option) {
    if (!strcmp(role, "Admin")) return option >= 1 && option <= 12;
    if (!strcmp(role, "Receptionist")) return option >= 1 && option <= 4;
    return option == 2 || option == 4 || option == 5 || option == 6;
}

int console_run(sqlite3 *db) {
    char username[64], password[256]; User user;
    int count = staff_count(db);
    if (count < 0) { fputs("Unable to read staff accounts.\n", stderr); return 1; }
    if (count == 0) {
        puts("First run: create the initial Administrator account.");
        if (!input("Admin username: ", username, sizeof username) ||
            !password_input("Admin password: ", password, sizeof password) ||
            !create_initial_admin(db, username, password)) {
            fputs("Administrator bootstrap failed.\n", stderr); return 1;
        }
    }
    for (;;) {
        if (!input("\nUsername (or q to quit): ", username, sizeof username)) return 0;
        if (!strcmp(username, "q")) return 0;
        if (!password_input("Password: ", password, sizeof password)) return 0;
        if (service_authenticate(db, username, password, &user) != 1) {
            puts("Login failed or account is locked."); continue;
        }
        printf("Signed in as %s (%s).\n", user.username, user.role);
        for (;;) {
            puts("\n1 Add patient  2 Search patients");
            if (strcmp(user.role, "Doctor")) puts("3 Book appointment");
            puts("4 Doctor schedule");
            if (strcmp(user.role, "Receptionist"))
                puts("5 Record visit/prescription  6 Prescription history");
            if (!strcmp(user.role, "Admin"))
                puts("7 Doctors  8 Add doctor  9 Create staff  10 Backup  11 Restore  12 Daily report/health");
            puts("0 Sign out");
            int option;
            if (!number("Choice: ", &option)) { puts("Enter a valid menu number."); continue; }
            if (option == 0) break;
            if (!role_allows(user.role, option)) { puts("This role cannot use that option."); continue; }
            if (option == 1) add_patient(&user);
            else if (option == 2) search_patient(&user);
            else if (option == 3) book(&user);
            else if (option == 4) schedule(&user);
            else if (option == 5) consult(&user);
            else if (option == 6) {
                int patient; if (number("Patient ID: ", &patient))
                    printf("Prescription rows: %d\n", consult_prescriptions(user.user_id, user.role, patient));
            } else if (option == 7) db_list_doctors();
            else if (option == 8) add_doctor(&user);
            else if (option == 9) create_staff(db, &user);
            else if (option == 10) {
                char path[512]; if (input("New backup file path: ", path, sizeof path))
                    printf("Backup result: %d\n", db_backup(user.user_id, user.role, path));
            } else if (option == 11) {
                char source[512], dest[512];
                if (input("Backup file path: ", source, sizeof source) &&
                    input("New destination database path: ", dest, sizeof dest))
                    printf("Restore result: %d\n", db_restore_backup(user.user_id, user.role, source, dest));
            } else if (option == 12) {
                char date[16];
                if (input("Report date (YYYY-MM-DD): ", date, sizeof date)) db_daily_report(date);
                printf("Health result: %d\n", db_health_check());
            }
        }
    }
}
