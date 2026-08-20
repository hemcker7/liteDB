# SQL Examples

The REPL accepts one command per line. End SQL statements with `;` for standard SQL-style input.

## Meta command

```sql
.exit
```

Exits the database REPL.

## DDL commands

```sql
CREATE TABLE users;
```

Creates or resets the active table named `users`.

```sql
DROP TABLE users;
```

Drops the active table and removes its rows.

## DML commands

Phase 2 supports explicit schemas with `INTEGER`, `TEXT`, and `NULL` values:

```sql
CREATE TABLE people (
	id INTEGER PRIMARY KEY,
	name TEXT,
	age INTEGER
);
```

The first column is the supported primary key column. Insert and query typed rows:

```sql
INSERT INTO people VALUES (1, 'Ada', 37);
INSERT INTO people VALUES (2, NULL, NULL);
SELECT name, age FROM people;
UPDATE people SET age = 38 WHERE id = 1;
DELETE FROM people WHERE id = 2;
```

The schema and rows are persisted in the database file and restored when the database is reopened.

## Multiple tables

Phase 3 adds a database catalog, so tables can be created and used independently:

```sql
CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT);
CREATE TABLE products (id INTEGER PRIMARY KEY, title TEXT);

INSERT INTO people VALUES (1, 'Ada');
INSERT INTO products VALUES (1, 'Book');

SELECT * FROM people;
SELECT * FROM products;
DROP TABLE people;
```

The catalog is stored beside the database as `litedb.db.catalog`. Each table has its own page-backed file with linked, sorted leaf pages and a persisted primary-key index file such as `litedb.db.table.people.index`.

## Indexed primary keys

The storage layer now maintains a B+ tree primary-key index with split leaves and internal routing. Duplicate-key checks use the index, while table scans continue through the cursor API:

```sql
INSERT INTO products VALUES (10, 'Notebook');
SELECT * FROM products WHERE id = 10;
```

## Querying rows

Phase 4 supports comparison operators, `AND`, `OR`, `ORDER BY`, and `LIMIT`:

```sql
SELECT name, age
FROM people
WHERE age >= 18 AND name != NULL
ORDER BY age DESC
LIMIT 10;
```

The same predicates can be used for mutations:

```sql
UPDATE people SET age = 39 WHERE name = 'Ada' OR age < 18;
DELETE FROM people WHERE age < 13;
```

## Transactions

Use transactions to group changes. Uncommitted changes are rolled back automatically when the database session closes:

```sql
BEGIN;
INSERT INTO people VALUES (3, 'Lin');
UPDATE people SET age = 30 WHERE id = 3;
COMMIT;
```

Discard a transaction explicitly:

```sql
BEGIN;
DELETE FROM people WHERE id = 3;
ROLLBACK;
```

Transactions use a write-ahead journal beside the database. If the process stops before commit, the next database open restores the pre-transaction files from the journal.

## Networking

Build the network executables with CMake, then start the server:

```text
build\litedb_server.exe 9001 data\network.db
```

Connect from another terminal:

```text
build\litedb_client.exe 127.0.0.1 9001
```

The client sends the same SQL commands as the local REPL. The server serializes database access and allows only the client that issued `BEGIN` to execute statements until `COMMIT` or `ROLLBACK`. Disconnecting during a transaction rolls it back automatically.

```sql
INSERT INTO users VALUES (1, 'alice', 'alice@example.com');
```

Inserts a row. The `id` value must be unique.

The original development syntax is also supported:

```text
insert 1 alice alice@example.com
```

```sql
SELECT * FROM users;
```

Prints every row in the table.

```sql
SELECT * FROM users WHERE id = 1;
```

Prints the row whose `id` matches the condition.

```sql
UPDATE users SET email = 'new@example.com' WHERE id = 1;
```

Updates the matching row's email.

```sql
UPDATE users SET username = 'new_name', email = 'new@example.com' WHERE id = 1;
```

Updates one or both supported text columns for the matching row.

```sql
DELETE FROM users WHERE id = 1;
```

Deletes the row whose `id` matches the condition.

```sql
DELETE FROM users;
```

Deletes every row in the active table.

## Current limitations

- Supported column types are `INTEGER` and `TEXT`; `NULL` is supported as a value.
- Only the first column can be a primary key.
- `WHERE` conditions are evaluated as a left-to-right chain; parentheses are not supported yet.
