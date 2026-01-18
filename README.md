# database

[![Build](https://github.com/cs-z/database/actions/workflows/ci.yml/badge.svg)](https://github.com/cs-z/database/actions/workflows/ci.yml)

An educational relational database written in modern **C++**. The project is under development.

## Quick start

### 1. Clone the Repository

```bash
git clone https://github.com/cs-z/database.git
cd database
```

### 2. Build the Project

Requires **CMake** and a **C++20 compiler** (GCC or Clang).

### Debug Build

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

### Release Build

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

### 3. Run It

You can use the database in two ways:

- **Interactive Console Mode**

	```bash
	./database
	```

- **Script Mode**

	```bash
	./database script.sql
	```

## Features

- Parsing and validating SQL queries
- Storing data on disk
- Page buffering (mapping between disk and RAM)
- Free space map to track available space in pages
- External sorting using K-way merge sort
- Aggregation operations
- Join operations
- Expression evaluation
- Query execution using the iterator model
- System catalog for storing metadata
- Detailed error reporting

## Examples

### Create Tables

```sql
CREATE TABLE users (id INT, name VARCHAR, age INT, height REAL, city_id INT);
CREATE TABLE cities (id INT, name VARCHAR);
```

### Insert Data

```sql
-- Users
INSERT INTO users VALUES (1, 'alice', 30, 1.68, 1);
INSERT INTO users VALUES (2, 'bob', 25, 1.75, 2);
INSERT INTO users VALUES (3, 'carol', 25, 1.62, 1);
INSERT INTO users VALUES (4, 'dan', NULL, 1.80, NULL);
INSERT INTO users VALUES (5, 'eve', 30, NULL, 2);

-- Cities
INSERT INTO cities VALUES (1, 'Paris');
INSERT INTO cities VALUES (2, 'Berlin');
```

### Queries

Query using expressions

```sql
SELECT name, CAST(height * 100 AS INT) AS height_cm FROM users
WHERE id IN (2, 4);
```

```
+------+-----------+
| NAME | HEIGHT_CM |
+------+-----------+
| bob  |       175 |
| dan  |       180 |
+------+-----------+
```

Query using aggregation

```sql
SELECT age, COUNT(*) AS user_count FROM users
GROUP BY age;
```

```
+------+------------+
| AGE  | USER_COUNT |
+------+------------+
|   25 |          2 |
|   30 |          2 |
| NULL |          1 |
+------+------------+
```

Query using sorting

```sql
SELECT * FROM users
ORDER BY age, city_id DESC;
```

```
+----+-------+------+--------+---------+
| ID | NAME  | AGE  | HEIGHT | CITY_ID |
+----+-------+------+--------+---------+
|  2 | bob   |   25 |   1.75 |       2 |
|  3 | carol |   25 |   1.62 |       1 |
|  5 | eve   |   30 |   NULL |       2 |
|  1 | alice |   30 |   1.68 |       1 |
|  4 | dan   | NULL |    1.8 |    NULL |
+----+-------+------+--------+---------+
```

Query using join

```sql
SELECT u.name AS user, c.name AS city
FROM users u
INNER JOIN cities c ON u.city_id = c.id;
```

```
+-------+--------+
| USER  | CITY   |
+-------+--------+
| alice | Paris  |
| bob   | Berlin |
| carol | Paris  |
| eve   | Berlin |
+-------+--------+
```

Query using join and system tables

```sql
SELECT t.name AS table_name, c.name AS column_name , c.id as column_id
FROM SYS_COLUMNS AS c JOIN SYS_TABLES AS t ON c.table_id = t.id
ORDER BY t.name, c.id;
```

```
+------------+-------------+-----------+
| TABLE_NAME | COLUMN_NAME | COLUMN_ID |
+------------+-------------+-----------+
| CITIES     | ID          |         0 |
| CITIES     | NAME        |         1 |
| USERS      | ID          |         0 |
| USERS      | NAME        |         1 |
| USERS      | AGE         |         2 |
| USERS      | HEIGHT      |         3 |
| USERS      | CITY_ID     |         4 |
+------------+-------------+-----------+
```

## Platform

- Uses Linux system calls for file I/O
- Easily portable to other platforms by replacing the OS-specific calls in `os.cpp`.

## TODO

Planned and in-progress features:

- Set operations: `DISTINCT`, `ALL`, `UNION`, `INTERSECT`, `EXCEPT`
- `DROP`, `DELETE` and `UPDATE` commands
- Pattern matching with `LIKE`
- Subqueries and `ALL`, `ANY`, `SOME` expressions
- Keys and constraints (e.g., `PRIMARY KEY`, `UNIQUE`)
- Indexing and index-based algorithms
- Query planning
- User management and authentication
- Transactions and ACID compliance
- Network interface
