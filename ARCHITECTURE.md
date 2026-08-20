# High-Level Architecture

```
                    User
                      │
                      ▼
               Input Reader
                      │
                      ▼
              Meta Commands
                      │
                      ▼
                   Parser
                      │
                      ▼
                 Statement
                      │
                      ▼
                  Executor
                      │
                      ▼
                    Table
                      │
                      ▼
                    Pager
                      │
          ┌───────────┴───────────┐
          ▼                       ▼
      Page 0                  Page 1
          │                       │
          ▼                       ▼
     Serialized Rows        Serialized Rows
```

---

## Layer Responsibilities

### Input Reader

Responsible only for obtaining input.

No parsing.

No command execution.

---

### Parser

Responsible for converting raw SQL text into a structured `Statement`.

No execution.

---

### Executor

Responsible for executing SQL semantics.

No knowledge of page layout or serialization.

---

### Table

Represents the logical table.

Responsibilities:

- Insert rows
- Read rows
- Compute page numbers
- Compute row slots

The table does **not** know how rows are serialized.

---

### Pager

Responsible for page management.

Responsibilities:

- Allocate pages
- Return page memory
- Maintain page cache

The pager has no knowledge of SQL or rows.

---

### Serialization

Responsible only for converting between:

```
Row
⇄
Raw Bytes
```

Memory ownership always belongs to the caller.

---

## Binary Layout

### Record

```
+------------+----------------+----------------+
| ID | Username | Email |
+------------+----------------+----------------+
```

### Page

```
+----------------------+
| Row Count Header     |
+----------------------+
| Serialized Row 0     |
+----------------------+
| Serialized Row 1     |
+----------------------+
| ...                  |
+----------------------+
```

---

# Important Design Decisions

## Parser constructs domain objects

The parser creates a fully populated `Row` during INSERT parsing.

The executor does not parse or construct rows.

---

## Storage layer is SQL-independent

`Table` does not accept `Statement` objects.

It stores and retrieves rows only.

---

## Serialization is stateless

Serialization functions operate on caller-provided buffers.

Ownership of memory remains with the caller.

---

## Fixed-size records

Rows are serialized into a deterministic binary format with fixed offsets.

This simplifies:

* Page layout
* Offset calculations
* Future persistence

---

# Planned Next Layer

```
Table
 │
 ▼
Pager
 │
 ▼
4096-byte Pages
 │
 ▼
Disk
```

The pager will become the abstraction responsible for page allocation, caching, loading, and flushing.
