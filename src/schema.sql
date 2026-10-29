PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS staff (
  staff_id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  salt TEXT NOT NULL,
  role TEXT NOT NULL CHECK(role IN ('Admin','Doctor','Receptionist')),
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
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS audit_log (
  log_id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL,
  action TEXT NOT NULL,
  table_name TEXT NOT NULL,
  record_id INTEGER,
  ts TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);