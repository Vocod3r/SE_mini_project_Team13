WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<10000)
INSERT INTO patients(name,age,gender,phone,address,blood_group)
SELECT 'Patient'||i, 20+i%60, 'M', '9'||printf('%09d',i), 'Addr '||i, 'O+' FROM n;