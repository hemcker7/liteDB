# SQLite Clone in Modern C++

A from-scratch SQLite-inspired relational database written in modern C++ as a learning project to understand how real database engines work internally.

> **Goal:** Learn database internals by building each layer ourselves instead of relying on existing libraries.

---

# Current Status

## Version 0.2.0 - In-Memory Page-Based Storage Engine 

The project has successfully transitioned from storing C++ `Row` objects directly to a page-oriented storage engine.

### Completed

- REPL
- Input handling
- Meta commands
- SQL statement parsing
- Statement abstraction
- Executor
- Row abstraction
- Binary row serialization/deserialization
- Page layout
- Pager
- Page-based table storage
- Unit testing
- Stress tested with 100,000 rows

### Current Storage Model

```
Executor
    │
    ▼
Table
    │
    ▼
Pager
    │
    ▼
4096-byte Pages
    │
    ▼
Serialized Rows
```

Rows are no longer stored as C++ objects inside the table.

Instead:

1. Parser constructs a temporary `Row`
2. Executor passes it to the table
3. Table computes the destination page and slot
4. Pager provides raw page memory
5. Serializer writes the row into the page

Retrieval performs the reverse process through deserialization.

---

## Tested Components

### Serialization

- Round-trip serialization
- Empty strings
- Maximum-length fields
- Buffer initialization

### Pager

- Empty pager
- Page allocation
- Repeated page access
- Sparse page allocation
- Zero initialization
- Page header operations
- Memory persistence

### Table

- Empty table
- Single insert
- Multiple inserts
- Page boundary transitions
- Multi-page tables
- Stress tested with 100,000 rows

All tests currently pass.

---

## Next Milestone

Version 0.3.0

Persistent storage.

The pager will become file-backed while keeping the higher layers unchanged.

---

## Project Goals

This project is intentionally built layer by layer to understand how real databases work.

Planned implementation includes:

* Fixed-size record serialization
* Pager
* Disk persistence
* Page cache
* B-Tree
* Cursor
* Multiple pages
* CREATE TABLE
* Multiple tables
* Basic query planner
* Networking (database server mode)
* Transactions
* Recovery

---

## Current Architecture

```
Console
    │
Input Reader
    │
Meta Command Handler
    │
SQL Parser
    │
Statement
    │
Executor
    │
Table
    │
Row
    │
Serialization
```

---

## Design Philosophy

This project follows several guiding principles:

* Single Responsibility Principle
* Clear separation between parsing and execution
* Storage layer independent of SQL layer
* Stateless helper modules where appropriate
* Binary formats explicitly defined
* Incremental development with continuous refactoring

---

## Technologies

* C++20
* STL
* g++
* VS Code

Future:

* CMake
* GoogleTest

---

## Roadmap

### Version 0.2

* Pager
* Fixed-size pages
* Persistence

### Version 0.3

* B-Tree
* Cursor
* Searching

### Version 0.4

* Internal nodes
* Root splitting

### Version 0.5

* Multiple tables
* CREATE TABLE

### Version 0.6

* Database server
* TCP networking
* Client protocol

---

## Learning Objectives

The purpose of this project is not merely to build a working database but to understand the design decisions behind systems like SQLite, including:

* Binary serialization
* Memory management
* Page-based storage
* B-Tree indexing
* Query execution
* Storage engine architecture
* Systems programming in modern C++
