CREATE TABLE users (id INT, name VARCHAR, age INT, height REAL, city_id INT);
CREATE TABLE cities (id INT, name VARCHAR);

INSERT INTO users VALUES (1, 'alice', 30, 1.68, 1);
INSERT INTO users VALUES (2, 'bob', 25, 1.75, 2);
INSERT INTO users VALUES (3, 'carol', 25, 1.62, 1);
INSERT INTO users VALUES (4, 'dan', NULL, 1.80, NULL);
INSERT INTO users VALUES (5, 'eve', 30, NULL, 2);

INSERT INTO cities VALUES (1, 'Paris');
INSERT INTO cities VALUES (2, 'Berlin');

SELECT name, CAST(height * 100 AS INT) AS height_cm FROM users
WHERE id IN (2, 4);

SELECT age, COUNT(*) AS user_count FROM users
GROUP BY age;

SELECT * FROM users
ORDER BY age, city_id DESC;

SELECT u.name AS user, c.name AS city
FROM users u
INNER JOIN cities c ON u.city_id = c.id;

SELECT t.name AS table_name, c.name AS column_name , c.id as column_id
FROM SYS_COLUMNS AS c JOIN SYS_TABLES AS t ON c.table_id = t.id
ORDER BY t.name, c.id;