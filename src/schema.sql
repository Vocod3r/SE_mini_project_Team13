PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS staff (
  staff_id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  salt TEXT NOT NULL,
  role TEXT NOT NULL CHECK(role IN ('Admin','Doctor','Receptionist','Accountant')),
  failed_attempts INTEGER NOT NULL DEFAULT 0,
  locked_until INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS patients (
  patient_id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  age INTEGER CHECK(age BETWEEN 0 AND 130),
  gender TEXT,
  phone TEXT NOT NULL,
  address TEXT,
  blood_group TEXT,
  is_archived INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_pat_name  ON patients(name);
CREATE INDEX IF NOT EXISTS idx_pat_phone ON patients(phone);

CREATE TABLE IF NOT EXISTS doctors (
  doctor_id INTEGER PRIMARY KEY AUTOINCREMENT,
  staff_id INTEGER REFERENCES staff(staff_id),
  name TEXT NOT NULL,
  specialization TEXT,
  work_start TEXT DEFAULT '09:00',
  work_end TEXT DEFAULT '17:00',
  consultation_fee REAL NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS appointments (
  appt_id INTEGER PRIMARY KEY AUTOINCREMENT,
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  doctor_id  INTEGER NOT NULL REFERENCES doctors(doctor_id),
  appt_date TEXT NOT NULL,
  time_slot TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'Booked'
    CHECK(status IN ('Booked','Rescheduled','Cancelled','Completed'))
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_slot
  ON appointments(doctor_id, appt_date, time_slot)
  WHERE status IN ('Booked','Rescheduled');
CREATE INDEX IF NOT EXISTS idx_appt_doc_date ON appointments(doctor_id, appt_date);

CREATE TABLE IF NOT EXISTS visits (
  visit_id INTEGER PRIMARY KEY AUTOINCREMENT,
  appt_id INTEGER NOT NULL REFERENCES appointments(appt_id),
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  diagnosis TEXT,
  visit_date TEXT NOT NULL DEFAULT (date('now'))
);

CREATE TABLE IF NOT EXISTS prescriptions (
  rx_id INTEGER PRIMARY KEY AUTOINCREMENT,
  visit_id INTEGER NOT NULL REFERENCES visits(visit_id),
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  medicines TEXT NOT NULL,
  dosage TEXT,
  notes TEXT
);

CREATE TABLE IF NOT EXISTS bills (
  bill_id INTEGER PRIMARY KEY AUTOINCREMENT,
  visit_id INTEGER NOT NULL REFERENCES visits(visit_id),
  amount REAL NOT NULL,
  payment_status TEXT NOT NULL DEFAULT 'Pending' CHECK(payment_status IN ('Paid','Pending')),
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  discount REAL NOT NULL DEFAULT 0 CHECK(discount >= 0),
  paid_on TEXT
);

-- SMPT-45: one full refund per paid bill, always with a reason code.
CREATE TABLE IF NOT EXISTS refunds (
  refund_id INTEGER PRIMARY KEY AUTOINCREMENT,
  bill_id INTEGER NOT NULL UNIQUE REFERENCES bills(bill_id),
  amount REAL NOT NULL CHECK(amount >= 0),
  reason_code TEXT NOT NULL CHECK(reason_code IN ('DUPLICATE','CANCELLED_SERVICE','OVERCHARGE','OTHER')),
  note TEXT,
  refunded_by INTEGER NOT NULL REFERENCES staff(staff_id),
  refunded_on TEXT NOT NULL DEFAULT (date('now'))
);

CREATE TABLE IF NOT EXISTS audit_log (
  log_id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL,
  action TEXT NOT NULL,
  table_name TEXT NOT NULL,
  record_id INTEGER,
  ts TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TRIGGER IF NOT EXISTS audit_log_reject_update
BEFORE UPDATE ON audit_log
BEGIN
  SELECT RAISE(ABORT, 'audit_log is append-only');
END;
CREATE TRIGGER IF NOT EXISTS audit_log_reject_delete
BEFORE DELETE ON audit_log
BEGIN
  SELECT RAISE(ABORT, 'audit_log is append-only');
END;
-- Legacy authentication table retained so db_init_schema can migrate its
-- existing records into the canonical staff table without deleting them.
CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL
);
