# LiteDB

LiteDB is a SQLite-inspired relational database implemented in modern C++. It is built as a learning project with separate SQL, execution, storage, persistence, and networking layers.

## Build

From the repository root:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

Run the test suite:

```powershell
ctest --test-dir build --output-on-failure
```

All generated build output belongs in `build/`. Generated database artifacts are stored under `data/` and ignored by Git.

## Local REPL

Start the local database shell:

```powershell
build\sqlite_clone.exe
```

The default database is `data/litedb.db`.

Example:

```sql
CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);
INSERT INTO people VALUES (1, 'Ada', 37);
SELECT name, age FROM people WHERE age >= 18 ORDER BY age DESC LIMIT 10;

BEGIN;
UPDATE people SET age = 38 WHERE id = 1;
COMMIT;
```

More commands are documented in [EXAMPLES.md](EXAMPLES.md).

## Networking

Start the TCP server with an optional port and database path:

```powershell
build\litedb_server.exe 9001 data\network.db
```

Connect with a client from another terminal:

```powershell
build\litedb_client.exe 127.0.0.1 9001
```

The client accepts the same SQL statements as the local REPL. Database access is serialized, and only the client that starts a transaction can execute statements until it sends `COMMIT` or `ROLLBACK`. Disconnecting during a transaction rolls it back.

## Supported Features

- `CREATE TABLE` and `DROP TABLE`
- Multiple tables through a persistent catalog
- `INTEGER`, `TEXT`, and `NULL` values
- `INSERT`, `SELECT`, `UPDATE`, and `DELETE`
- Column projections with `SELECT column1, column2`
- `WHERE` comparisons: `=`, `!=`, `<`, `<=`, `>`, `>=`
- `AND` and `OR` predicates
- `ORDER BY`, `ASC`, `DESC`, and `LIMIT`
- Primary-key uniqueness checks
- Page-based file storage under `data/`
- Linked sorted leaf pages and cursor scans
- Persistent B+ tree primary-key indexes
- `BEGIN`, `COMMIT`, and `ROLLBACK`
- Write-ahead journal recovery after interrupted transactions
- TCP server and client executables

## Architecture

```text
SQL input
	|
Parser
	|
Statement
	|
Database catalog
	|
Executor
	|
Table and Cursor
	|
B+ tree index + linked leaf pages
	|
Pager and Serialization
	|
Database files
```

The database uses one catalog file and separate files for each table:

```text
data/litedb.db.catalog
data/litedb.db.table.people
data/litedb.db.table.people.index
data/litedb.db.wal
```

The WAL is created during an active transaction and removed after commit or rollback.

## Current Limitations

- The database currently allows one active transaction at a time.
- Network access is serialized by the server.
- Parenthesized SQL expressions are not supported.
- `WHERE` conditions are evaluated left to right.
- The WAL stores full pre-transaction file images rather than page-level log records.
- B+ tree indexes are persisted separately from table data.

