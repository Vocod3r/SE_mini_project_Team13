#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "db.h"

static int fails = 0;
#define CHECK(name, cond) do { int ok_ = !!(cond); printf("%s: %s\n", ok_ ? "PASS" : "FAIL", name); \
                               if (!ok_) fails++; } while (0)

static int scalar(const char *file, const char *sql) {
    sqlite3 *d = NULL; sqlite3_stmt *st = NULL; int v = -999;
    if (sqlite3_open(file, &d) == SQLITE_OK && sqlite3_prepare_v2(d, sql, -1, &st, 0) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st); sqlite3_close(d);
    return v;
}

static int bill_for(int doc, int pat, int clin, int rec, const char *date) {
    int appt = db_book_appointment(rec, pat, doc, date, "10:00");
    int visit = db_add_visit_and_rx(clin, "Doctor", appt, "Dx", "Med", "daily", "");
    return db_create_bill(rec, "Receptionist", visit);
}

int main(void) {
    const char *F = "t_billing.db";
    remove(F); remove("t_billing.db-wal"); remove("t_billing.db-shm");
    db_open(F);
    db_init_schema("schema.sql");
    int admin = db_create_staff(0, "Admin", "admin", "h", "s", "Admin");
    int acct = db_create_staff(admin, "Admin", "acct", "h", "s", "Accountant");
    int rec = db_create_staff(admin, "Admin", "rec", "h", "s", "Receptionist");
    int doc_u = db_create_staff(admin, "Admin", "doc", "h", "s", "Doctor");
    int doc = db_add_doctor(admin, "Admin", "Dr. Mehta", "General", 1000.0);
    int pat = db_add_patient(rec, "Receptionist", "Asha Rao", 30, "F", "9876543210", "Chennai", "O+");
    CHECK("setup: accountant account can be created", acct > 0 && rec > 0 && doc_u > 0 && doc > 0 && pat > 0);

    int b1 = bill_for(doc, pat, doc_u, rec, "2026-12-01");
    int b2 = bill_for(doc, pat, doc_u, rec, "2026-12-02");
    int b3 = bill_for(doc, pat, doc_u, rec, "2026-12-03");
    CHECK("bills created (fee 1000 each)", b1 > 0 && b2 > 0 && b3 > 0);
    CHECK("accountant cannot create a bill for an unknown visit", db_create_bill(acct, "Accountant", 9999) == -1);
    CHECK("doctor cannot create bills", db_create_bill(doc_u, "Doctor", 1) == -3);

    /* SMPT-46 discount */
    CHECK("receptionist cannot discount", db_apply_discount(rec, "Receptionist", b1, 10) == -3);
    CHECK("accountant discount 200 (=20%) ok", db_apply_discount(acct, "Accountant", b1, 200) == 0);
    CHECK("accountant discount 201 refused (-4)", db_apply_discount(acct, "Accountant", b1, 201) == -4);
    CHECK("negative discount refused", db_apply_discount(acct, "Accountant", b1, -5) == -1);
    CHECK("admin may waive 100%", db_apply_discount(admin, "Admin", b3, 1000) == 0);
    CHECK("admin cannot exceed 100%", db_apply_discount(admin, "Admin", b3, 1001) == -4);
    CHECK("discount on unknown bill", db_apply_discount(acct, "Accountant", 9999, 1) == -1);
    CHECK("role claim must match stored role", db_apply_discount(rec, "Accountant", b2, 10) == -3);

    /* payment */
    CHECK("accountant marks b1 Paid", db_set_payment_status(acct, "Accountant", b1, "Paid") == 0);
    CHECK("discount refused on a Paid bill", db_apply_discount(acct, "Accountant", b1, 5) == -1);
    CHECK("b3 paid (fully waived)", db_set_payment_status(rec, "Receptionist", b3, "Paid") == 0);

    /* SMPT-47 dues */
    double due = 0;
    CHECK("dues: only b2 pending", db_print_outstanding_dues(acct, "Accountant", &due) == 1 && due == 1000.0);
    CHECK("dues: receptionist refused", db_print_outstanding_dues(rec, "Receptionist", NULL) == -3);

    /* SMPT-44 reconcile (paid_on is today's UTC date) */
    char today[16]; time_t t = time(NULL); strftime(today, sizeof today, "%Y-%m-%d", gmtime(&t));
    Reconciliation r;
    CHECK("reconcile today: 2 paid", db_reconcile_day(acct, "Accountant", today, &r) == 2);
    CHECK("reconcile collected = 800 + 0", r.collected == 800.0 && r.refund_count == 0 && r.net == 800.0);
    CHECK("reconcile bad date", db_reconcile_day(acct, "Accountant", "2026-13-40", NULL) == -1);
    CHECK("reconcile: receptionist refused", db_reconcile_day(rec, "Receptionist", today, NULL) == -3);
    CHECK("reconcile: other day is empty", db_reconcile_day(acct, "Accountant", "2000-01-01", &r) == 0 && r.collected == 0);

    /* SMPT-45 refund */
    CHECK("refund: pending bill refused (-4)", db_refund_bill(acct, "Accountant", b2, "DUPLICATE", NULL) == -4);
    CHECK("refund: bad reason code", db_refund_bill(acct, "Accountant", b1, "BECAUSE", NULL) == -1);
    CHECK("refund: missing reason code", db_refund_bill(acct, "Accountant", b1, NULL, NULL) == -1);
    CHECK("refund: receptionist refused", db_refund_bill(rec, "Receptionist", b1, "OTHER", NULL) == -3);
    CHECK("refund: unknown bill", db_refund_bill(acct, "Accountant", 9999, "OTHER", NULL) == -1);
    int rf = db_refund_bill(acct, "Accountant", b1, "OVERCHARGE", "patient was overcharged");
    CHECK("refund: paid bill refunded", rf > 0);
    CHECK("refund: second refund refused (-4)", db_refund_bill(acct, "Accountant", b1, "DUPLICATE", NULL) == -4);
    CHECK("refund: amount is net of discount (800)", scalar(F, "SELECT amount FROM refunds WHERE bill_id=1") == 800);
    CHECK("refund: reason code stored",
          scalar(F, "SELECT reason_code='OVERCHARGE' FROM refunds WHERE bill_id=1") == 1);
    CHECK("refund: audited",
          scalar(F, "SELECT COUNT(*) FROM audit_log WHERE action='REFUND' AND record_id=1") == 1);
    CHECK("refunded bill status cannot be flipped back to Pending",
          db_set_payment_status(acct, "Accountant", b1, "Pending") == -1);
    CHECK("reconcile after refund: net 0", db_reconcile_day(acct, "Accountant", today, &r) == 2 &&
          r.refund_count == 1 && r.refunded == 800.0 && r.net == 0.0);

    /* SMPT-47 revenue */
    CHECK("revenue: bad range", db_print_revenue_report(acct, "Accountant", today, "2000-01-01", NULL) == -1);
    CHECK("revenue: net 0 after refund",
          db_print_revenue_report(acct, "Accountant", "2000-01-01", "2999-12-31", &r) == 2 && r.net == 0.0);
    CHECK("revenue: doctor refused", db_print_revenue_report(doc_u, "Doctor", today, today, NULL) == -3);
    CHECK("discount audited", scalar(F, "SELECT COUNT(*) FROM audit_log WHERE action='DISCOUNT'") == 2);

    db_close();

    /* migration: an old database (3-role CHECK, no billing columns) is upgraded in place */
    const char *O = "t_billing_old.db";
    remove(O);
    sqlite3 *d = NULL; sqlite3_open(O, &d);
    sqlite3_exec(d,
        "CREATE TABLE staff (staff_id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE,"
        " password_hash TEXT NOT NULL, salt TEXT NOT NULL,"
        " role TEXT NOT NULL CHECK(role IN ('Admin','Doctor','Receptionist')),"
        " failed_attempts INTEGER NOT NULL DEFAULT 0, locked_until INTEGER NOT NULL DEFAULT 0);"
        "INSERT INTO staff(username,password_hash,salt,role) VALUES('oldadmin','h','s','Admin');"
        "CREATE TABLE bills (bill_id INTEGER PRIMARY KEY AUTOINCREMENT, visit_id INTEGER NOT NULL, amount REAL NOT NULL,"
        " payment_status TEXT NOT NULL DEFAULT 'Pending' CHECK(payment_status IN ('Paid','Pending')),"
        " created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);", 0, 0, 0);
    sqlite3_close(d);
    db_open(O);
    CHECK("migration: init on old db succeeds", db_init_schema("schema.sql") == 0);
    int oldadmin = scalar(O, "SELECT staff_id FROM staff WHERE username='oldadmin'");
    CHECK("migration: existing admin kept", oldadmin == 1);
    CHECK("migration: Accountant can now be created",
          db_create_staff(oldadmin, "Admin", "newacct", "h", "s", "Accountant") > 0);
    CHECK("migration: bills has discount column",
          scalar(O, "SELECT COUNT(*) FROM pragma_table_info('bills') WHERE name IN ('discount','paid_on')") == 2);
    CHECK("migration: second init is a no-op", db_init_schema("schema.sql") == 0);
    db_close();

    remove(F); remove("t_billing.db-wal"); remove("t_billing.db-shm");
    remove(O); remove("t_billing_old.db-wal"); remove("t_billing_old.db-shm");
    printf(fails ? "RESULT: %d FAILED\n" : "RESULT: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
