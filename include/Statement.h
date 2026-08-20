#pragma once
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include "Row.h"
enum class StatementType{
    Insert,
    Select,
    CreateTable,
    DropTable,
    Update,
    Delete,
    Begin,
    Commit,
    Rollback
};

enum class PrepareResult{
    Success,
    UnrecognizedStatement,
    SyntaxError
    //later SyntaxError, Missing values etc
};

enum class ComparisonOperator { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
enum class LogicalOperator { And, Or };

struct Condition{
    std::string column;
    ComparisonOperator operation;
    Value value;
};

struct Statement{
    StatementType type = StatementType::Select;
    std::string tableName = "users";
    Row row;
    Schema schema;
    std::vector<Value> values;
    bool generic = false;
    std::vector<std::string> selectedColumns;
    std::vector<std::pair<std::string, Value>> assignments;
    std::optional<uint32_t> whereId;
    std::optional<std::string> username;
    std::optional<std::string> email;
    std::vector<Condition> conditions;
    std::vector<LogicalOperator> logicalOperators;
    std::string orderBy;
    bool orderDescending = false;
    std::optional<std::size_t> limit;

};

PrepareResult prepareStatement(const std::string& input, Statement& statement);