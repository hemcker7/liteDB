#include "Executor.h"
#include "Table.h"
#include <iostream>
#include <algorithm>
#include <compare>
#include <variant>

namespace {
void printValue(const Value& value){
    if(std::holds_alternative<std::monostate>(value)) std::cout << "NULL";
    else if(std::holds_alternative<int64_t>(value)) std::cout << std::get<int64_t>(value);
    else std::cout << std::get<std::string>(value);
}
void printRows(const std::vector<GenericRow>& rows, const Schema& schema, const std::vector<std::string>& selectedColumns){
    std::vector<std::size_t> indexes;
    if(selectedColumns.empty() || (selectedColumns.size() == 1 && selectedColumns.front() == "*")){
        for(std::size_t index = 0; index < schema.columns.size(); index++) indexes.push_back(index);
    }else{
        for(const auto& selected : selectedColumns){
            for(std::size_t index = 0; index < schema.columns.size(); index++) if(schema.columns[index].name == selected) indexes.push_back(index);
        }
    }
    for(const auto& row : rows){
        for(std::size_t position = 0; position < indexes.size(); position++){
            if(position) std::cout << "\t";
            printValue(row.values[indexes[position]]);
        }
        std::cout << "\n";
    }
}
bool isLegacySchema(const Schema& schema){
    return schema.columns.size() == 3 && schema.columns[0].name == "id" && schema.columns[1].name == "username" && schema.columns[2].name == "email";
}
int compareValues(const Value& left, const Value& right){
    if(std::holds_alternative<std::monostate>(left) || std::holds_alternative<std::monostate>(right)){
        if(std::holds_alternative<std::monostate>(left) && std::holds_alternative<std::monostate>(right)) return 0;
        return std::holds_alternative<std::monostate>(left) ? -1 : 1;
    }
    if(std::holds_alternative<int64_t>(left) && std::holds_alternative<int64_t>(right)){
        const auto leftInteger = std::get<int64_t>(left), rightInteger = std::get<int64_t>(right);
        return leftInteger < rightInteger ? -1 : (leftInteger > rightInteger ? 1 : 0);
    }
    if(std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) return std::get<std::string>(left).compare(std::get<std::string>(right));
    return 0;
}
bool matchesCondition(const GenericRow& row, const Schema& schema, const Condition& condition){
    std::size_t columnIndex = schema.columns.size();
    for(std::size_t index = 0; index < schema.columns.size(); index++) if(schema.columns[index].name == condition.column) columnIndex = index;
    if(columnIndex == schema.columns.size()) return false;
    const auto comparison = compareValues(row.values[columnIndex], condition.value);
    switch(condition.operation){
        case ComparisonOperator::Equal: return comparison == 0;
        case ComparisonOperator::NotEqual: return comparison != 0;
        case ComparisonOperator::Less: return comparison < 0;
        case ComparisonOperator::LessEqual: return comparison <= 0;
        case ComparisonOperator::Greater: return comparison > 0;
        case ComparisonOperator::GreaterEqual: return comparison >= 0;
    }
    return false;
}
bool matches(const GenericRow& row, const Schema& schema, const Statement& statement){
    if(statement.conditions.empty()) return true;
    bool result = matchesCondition(row, schema, statement.conditions.front());
    for(std::size_t index = 1; index < statement.conditions.size(); index++){
        const bool current = matchesCondition(row, schema, statement.conditions[index]);
        if(statement.logicalOperators[index - 1] == LogicalOperator::And) result = result && current;
        else result = result || current;
    }
    return result;
}
}

ExecuteResult executeStatement(const Statement& statement, Table& table){
    if(statement.type == StatementType::CreateTable){ table.create(statement.schema); return ExecuteResult::Success; }
    if(statement.type == StatementType::DropTable){ table.drop(statement.tableName); return ExecuteResult::Success; }
    if(!table.exists(statement.tableName)) return ExecuteResult::TableNotFound;
    switch(statement.type){
        case StatementType::Insert :
            if(statement.generic){
                if(statement.values.size() != table.schema().columns.size()) return ExecuteResult::InvalidOperation;
                for(std::size_t index = 0; index < statement.values.size(); index++){
                    const auto& value = statement.values[index];
                    if(std::holds_alternative<std::monostate>(value)) continue;
                    if(table.schema().columns[index].type == ColumnType::Integer && !std::holds_alternative<int64_t>(value)) return ExecuteResult::InvalidOperation;
                    if(table.schema().columns[index].type == ColumnType::Text && !std::holds_alternative<std::string>(value)) return ExecuteResult::InvalidOperation;
                }
                if(table.schema().columns.front().primaryKey){
                    if(!std::holds_alternative<int64_t>(statement.values.front())) return ExecuteResult::InvalidOperation;
                    if(table.containsPrimaryKey(std::get<int64_t>(statement.values.front()))) return ExecuteResult::DuplicateKey;
                }
                table.insert(GenericRow{statement.values});
                return ExecuteResult::Success;
            }
            if(table.containsId(statement.row.id)) return ExecuteResult::DuplicateKey;
            table.insert(statement.row);
            return ExecuteResult::Success;
        case StatementType::Select :
            {
                auto rows = table.getAllGenericRows();
                if(!statement.orderBy.empty()){
                    std::size_t orderIndex = table.schema().columns.size();
                    for(std::size_t index = 0; index < table.schema().columns.size(); index++) if(table.schema().columns[index].name == statement.orderBy) orderIndex = index;
                    if(orderIndex == table.schema().columns.size()) return ExecuteResult::InvalidOperation;
                    std::sort(rows.begin(), rows.end(), [&statement, orderIndex](const GenericRow& left, const GenericRow& right){
                        const auto comparison = compareValues(left.values[orderIndex], right.values[orderIndex]);
                        return statement.orderDescending ? comparison > 0 : comparison < 0;
                    });
                }
                std::vector<GenericRow> filtered;
                for(const auto& row : rows) if(matches(row, table.schema(), statement)) filtered.push_back(row);
                if(statement.limit && *statement.limit < filtered.size()) filtered.resize(*statement.limit);
                for(const auto& row : filtered) printRows({row}, table.schema(), statement.selectedColumns);
            }
            return ExecuteResult::Success;
        case StatementType::Update: {
            auto rows = table.getAllGenericRows();
            bool updatedAny = false;
            for(auto& row : rows){
                if(!matches(row, table.schema(), statement)) continue;
                updatedAny = true;
                for(const auto& [columnName, value] : statement.assignments){
                    std::size_t index = table.schema().columns.size();
                    for(std::size_t candidate = 0; candidate < table.schema().columns.size(); candidate++) if(table.schema().columns[candidate].name == columnName) index = candidate;
                    if(index == table.schema().columns.size() || (table.schema().columns[index].primaryKey && !std::holds_alternative<std::monostate>(value))) return ExecuteResult::InvalidOperation;
                    if(!std::holds_alternative<std::monostate>(value) && ((table.schema().columns[index].type == ColumnType::Integer && !std::holds_alternative<int64_t>(value)) || (table.schema().columns[index].type == ColumnType::Text && !std::holds_alternative<std::string>(value)))) return ExecuteResult::InvalidOperation;
                    row.values[index] = value;
                }
            }
            if(!updatedAny) return ExecuteResult::InvalidOperation;
            table.clear();
            for(const auto& row : rows) table.insert(row);
            return ExecuteResult::Success;
        }
        case StatementType::Delete:
            {
                const auto rows = table.getAllGenericRows();
                table.clear();
                for(const auto& row : rows) if(!matches(row, table.schema(), statement)) table.insert(row);
            }
            return ExecuteResult::Success;
        case StatementType::CreateTable:
        case StatementType::DropTable:
            return ExecuteResult::Success;
    }
    return ExecuteResult::Success;
}