#pragma once

#include "Row.h"
#include <cstddef>

class Table;

class Cursor{
public:
    bool valid() const;
    void next();
    GenericRow row() const;

private:
    friend class Table;
    explicit Cursor(const Table& table);
    void advanceToRow();

    const Table* table_;
    std::size_t pageNumber_;
    std::size_t slot_;
};