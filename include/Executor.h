#pragma once
#include "Statement.h"
#include "Table.h"

enum class ExecuteResult{
    Success,
    DuplicateKey,
    TableFull,
    TableNotFound,
    InvalidOperation,
    TransactionAlreadyActive,
    NoActiveTransaction
};

ExecuteResult executeStatement(const Statement& statement, Table& table);