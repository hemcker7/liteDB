#pragma once
#include <string>
#include <cstdint>
#include <variant>
#include <vector>

enum class ColumnType { Integer, Text };
using Value = std::variant<std::monostate, int64_t, std::string>;

struct Column{
    std::string name;
    ColumnType type;
    bool primaryKey = false;
};

struct Schema{
    std::string tableName;
    std::vector<Column> columns;
};

struct GenericRow{
    std::vector<Value> values;
};

struct Row{
    uint32_t id;
    std::string username;
    std::string email;
};