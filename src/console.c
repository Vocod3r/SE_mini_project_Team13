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
#include "input.h"
#include "report.h"
#include "session.h"

/* HMS-SR-005: while a user is signed in, every prompt watches the idle timer. */
static Session *g_session = NULL;
static time_t (*g_clock)(void) = NULL;
void console_set_clock(time_t (*clock)(void)) { g_clock = clock; }
static int g_idle_out = 0;

static int input(const char *label, char *out, size_t cap) {
    printf("%s", label);
    fflush(stdout);
    if (g_session && session_wait_input(stdin, g_session) == 0) { g_idle_out = 1; return 0; }
    if (!fgets(out, (int)cap, stdin)) return 0;
    if (g_session) {
        if (session_expired(g_session)) { g_idle_out = 1; out[0] = '\0'; return 0; }  /* typed after the timeout: discard */
        session_touch(g_session);
    }
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

/* Date prompt built on input() so the idle timer watches it too; re-asks up to 3 times. */
static int ask_date(const char *label, char *out, size_t cap) {
    for (int tries = 0; tries < 3; tries++) {
        if (!input(label, out, cap)) return -1;
        if (input_valid_date(out)) return 0;
        puts("  Please enter a real date as YYYY-MM-DD, for example 2026-11-03.");
    }
    return -1;
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
        !input("Gender (M/F/O): ", gender, sizeof gender) || !input("Phone: ", phone, sizeof phone) ||
        !input("Address: ", address, sizeof address) || !input("Blood group (e.g. O+): ", bg, sizeof bg)) {
        if (!g_idle_out) puts("Invalid or incomplete input.");
        return;
    }
    if (!input_valid_name(name, 100)) { puts("Name must contain letters only (spaces, . ' - allowed), up to 100 characters."); return; }
    if (age > 130) { puts("Age must be between 0 and 130."); return; }
    if (!input_valid_phone(phone)) { puts("Phone must be 10 to 13 digits, with an optional leading +."); return; }
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
        !input("Time slot (HH:MM, e.g. 09:30): ", slot, sizeof slot)) {
        if (!g_idle_out) puts("Invalid input.");
        return;
    }
    if (!input_valid_date(date)) { puts("That is not a real date. Use YYYY-MM-DD, for example 2026-11-10."); return; }
    if (!input_valid_time(slot)) { puts("That is not a valid time. Use HH:MM, for example 09:30."); return; }
    int rc = service_book_appointment(u, pid, did, date, slot);
    if (rc > 0) printf("Appointment booked. Appointment ID: %d\n", rc);
    else if (rc == -2) puts("That doctor is already booked at that time. Please choose another slot.");
    else if (rc == -3) puts("You are not allowed to book appointments.");
    else puts("Could not book. Check that the patient (not archived) and doctor IDs exist.");
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
        !input("Role (Admin/Doctor/Receptionist/Accountant): ", role, sizeof role)) return;
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
    int doctor; char date[16], mode[8];
    if (!number("Doctor ID: ", &doctor) || !input("Show one day or a week? (d/w): ", mode, sizeof mode)) return;
    int week = (mode[0] == 'w' || mode[0] == 'W');
    if (ask_date(week ? "First day of the week (YYYY-MM-DD): " : "Date (YYYY-MM-DD): ",
                 date, sizeof date) != 0) return;
    int rows = week ? appt_schedule_week(u->role, doctor, date) : appt_schedule_day(u->role, doctor, date);
    if (rows == 0) puts("No appointments for that doctor in that period.");
    else if (rows < 0) puts("Could not show the schedule.");
}

static void queue(const User *u) {
    int doctor; char date[16];
    if (!number("Doctor ID: ", &doctor) ||
        ask_date("Date (YYYY-MM-DD): ", date, sizeof date) != 0) return;
    int rc = appt_queue(u->user_id, u->role, doctor, date);
    if (rc == -3) puts("You are not allowed to view this queue.");
    else if (rc < 0) puts("Could not show the queue. Check the doctor ID.");
}

static void slip(const User *u) {
    int appt;
    if (!number("Appointment ID: ", &appt)) return;
    int rc = appt_print_slip(u->role, appt);
    if (rc == -1) puts("No appointment with that ID.");
    else if (rc == -2) puts("That appointment is cancelled or already completed, so no slip is issued.");
    else if (rc == -3) puts("You are not allowed to print slips.");
}

static void bill_create(const User *u) {
    int visit; if (!number("Visit ID: ", &visit)) return;
    int rc = db_create_bill(u->user_id, u->role, visit);
    if (rc > 0) printf("Bill %d created.\n", rc);
    else if (rc == -3) puts("You are not allowed to create bills.");
    else puts("Could not create a bill (unknown visit or already billed).");
}

static void bill_status(const User *u) {
    int bill; char st[16];
    if (!number("Bill ID: ", &bill) || !input("Status (Paid/Pending): ", st, sizeof st)) return;
    int rc = db_set_payment_status(u->user_id, u->role, bill, st);
    puts(rc == 0 ? "Payment status updated." : rc == -3 ? "You are not allowed to do that." :
         "Could not update (bad status, unknown bill, or bill already refunded).");
}

static void bill_discount(const User *u) {
    int bill, amt;
    if (!number("Bill ID: ", &bill) || !number("Discount amount (whole currency units): ", &amt)) return;
    int rc = db_apply_discount(u->user_id, u->role, bill, (double)amt);
    puts(rc == 0 ? "Discount applied." : rc == -3 ? "You are not allowed to give discounts." :
         rc == -4 ? "Discount is above your limit (Accountant 20%, Admin 100%)." :
         "Could not apply (unknown bill, or bill is not Pending).");
}

static void bill_refund(const User *u) {
    int bill; char code[32], note[220];
    if (!number("Bill ID: ", &bill) ||
        !input("Reason code (DUPLICATE/CANCELLED_SERVICE/OVERCHARGE/OTHER): ", code, sizeof code) ||
        !input("Note (optional, press Enter to skip): ", note, sizeof note)) return;
    int rc = db_refund_bill(u->user_id, u->role, bill, code, note[0] ? note : NULL);
    if (rc > 0) printf("Refund %d recorded.\n", rc);
    else if (rc == -3) puts("You are not allowed to refund.");
    else if (rc == -4) puts("Only a Paid bill that has not been refunded can be refunded.");
    else puts("Refund failed: check the bill ID and the reason code.");
}

static void bill_reconcile(const User *u) {
    char date[16];
    if (ask_date("Date to reconcile (YYYY-MM-DD): ", date, sizeof date) != 0) return;
    int rc = db_reconcile_day(u->user_id, u->role, date, NULL);
    if (rc == -3) puts("You are not allowed to reconcile.");
    else if (rc < 0) puts("Could not reconcile that date.");
}

static void bill_revenue(const User *u) {
    char from[16], to[16];
    if (ask_date("From date (YYYY-MM-DD): ", from, sizeof from) != 0 ||
        ask_date("To date (YYYY-MM-DD): ", to, sizeof to) != 0) return;
    int rc = db_print_revenue_report(u->user_id, u->role, from, to, NULL);
    if (rc == -3) puts("You are not allowed to view revenue.");
    else if (rc < 0) puts("Could not produce the report (check the dates).");
}

static int role_allows(const char *role, int option) {
    if (!strcmp(role, "Admin")) return option >= 1 && option <= 21;
    if (!strcmp(role, "Accountant")) return option >= 15 && option <= 21;
    if (!strcmp(role, "Receptionist")) return (option >= 1 && option <= 4) || option == 13 || option == 14 ||
                                              option == 15 || option == 16;
    return option == 2 || option == 4 || option == 5 || option == 6 || option == 13;
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
        Session sess;
        session_init(&sess, SESSION_IDLE_SECONDS, g_clock);
        g_session = &sess; g_idle_out = 0;
        for (;;) {
            int is_acct = !strcmp(user.role, "Accountant");
            if (!is_acct) {
                puts("\n1 Add patient  2 Search patients");
                if (strcmp(user.role, "Doctor")) puts("3 Book appointment");
                puts("4 Doctor schedule (day or week)  13 Doctor patient queue");
                if (strcmp(user.role, "Doctor")) puts("14 Print appointment slip");
                if (strcmp(user.role, "Receptionist"))
                    puts("5 Record visit/prescription  6 Prescription history");
                if (!strcmp(user.role, "Admin"))
                    puts("7 Doctors  8 Add doctor  9 Create staff  10 Backup  11 Restore  12 Daily report/health");
            }
            if (!strcmp(user.role, "Receptionist"))
                puts("15 Create bill  16 Set payment status");
            if (is_acct || !strcmp(user.role, "Admin")) {
                puts("15 Create bill  16 Set payment status  17 Apply discount  18 Refund");
                puts("19 Daily reconciliation  20 Revenue report  21 Outstanding dues");
            }
            puts("0 Sign out");
            int option;
            if (!number("Choice: ", &option)) {
                if (g_idle_out) break;
                puts("Enter a valid menu number."); continue;
            }
            if (option == 0) break;
            if (!role_allows(user.role, option)) { puts("This role cannot use that option."); continue; }
            if (option == 1) add_patient(&user);
            else if (option == 2) search_patient(&user);
            else if (option == 3) book(&user);
            else if (option == 4) schedule(&user);
            else if (option == 13) queue(&user);
            else if (option == 14) slip(&user);
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
                if (ask_date("Report date (YYYY-MM-DD): ", date, sizeof date) == 0 &&
                    report_daily(user.user_id, user.role, date) < 0)
                    puts("Could not produce the report.");
                if (!g_idle_out) printf("Health result: %d\n", db_health_check());
            }
            else if (option == 15) bill_create(&user);
            else if (option == 16) bill_status(&user);
            else if (option == 17) bill_discount(&user);
            else if (option == 18) bill_refund(&user);
            else if (option == 19) bill_reconcile(&user);
            else if (option == 20) bill_revenue(&user);
            else if (option == 21) {
                if (db_print_outstanding_dues(user.user_id, user.role, NULL) < 0) puts("Could not list dues.");
            }
            if (g_idle_out) break;
        }
        g_session = NULL;
        if (g_idle_out) {
            puts("\nYou were logged out after 10 minutes of inactivity. Please log in again.");
            g_idle_out = 0;
        }
    }
}
