#include "Statement.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string trim(std::string value){
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c){ return !std::isspace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), value.end());
    return value;
}
std::string lower(std::string value){
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}
std::string unquote(std::string value){
    value = trim(value);
    if(value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') || (value.front() == '"' && value.back() == '"'))) return value.substr(1, value.size() - 2);
    return value;
}
Value parseValue(std::string value){
    value = trim(value);
    if(lower(value) == "null") return std::monostate{};
    if(value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') || (value.front() == '"' && value.back() == '"'))) return unquote(value);
    try { return static_cast<int64_t>(std::stoll(value)); } catch(...) { return value; }
}
bool parseCondition(const std::string& text, Statement& statement){
    const std::string operators[] = {"<=", ">=", "!=", "=", "<", ">"};
    const ComparisonOperator operations[] = {ComparisonOperator::LessEqual, ComparisonOperator::GreaterEqual, ComparisonOperator::NotEqual, ComparisonOperator::Equal, ComparisonOperator::Less, ComparisonOperator::Greater};
    for(std::size_t index = 0; index < std::size(operators); index++){
        const auto position = text.find(operators[index]);
        if(position == std::string::npos) continue;
        const auto column = lower(trim(text.substr(0, position)));
        if(column.empty()) return false;
        const auto value = parseValue(text.substr(position + operators[index].size()));
        statement.conditions.push_back({column, operations[index], value});
        if(column == "id" && operations[index] == ComparisonOperator::Equal && std::holds_alternative<int64_t>(value)) statement.whereId = static_cast<uint32_t>(std::get<int64_t>(value));
        return true;
    }
    return false;
}

bool parseWhere(const std::string& text, Statement& statement){
    std::string current;
    char quote = 0;
    for(std::size_t index = 0; index < text.size(); index++){
        const char character = text[index];
        if((character == '\'' || character == '"') && (quote == 0 || quote == character)) quote = quote == 0 ? character : 0;
        if(quote == 0 && std::isalpha(static_cast<unsigned char>(character))){
            std::size_t end = index;
            while(end < text.size() && std::isalpha(static_cast<unsigned char>(text[end]))) end++;
            const auto word = lower(text.substr(index, end - index));
            if((word == "and" || word == "or") && (index == 0 || std::isspace(static_cast<unsigned char>(text[index - 1]))) && (end == text.size() || std::isspace(static_cast<unsigned char>(text[end])))){
                if(!parseCondition(current, statement)) return false;
                statement.logicalOperators.push_back(word == "and" ? LogicalOperator::And : LogicalOperator::Or);
                current.clear();
                index = end - 1;
                continue;
            }
        }
        current += character;
    }
    return !trim(current).empty() && parseCondition(current, statement);
}

bool parseQueryModifiers(const std::string& text, Statement& statement){
    const auto normalized = lower(text);
    const auto where = normalized.find("where ");
    const auto order = normalized.find("order by ");
    const auto limit = normalized.find("limit ");
    if(where != std::string::npos){
        auto end = text.size();
        if(order != std::string::npos && order > where) end = order;
        if(limit != std::string::npos && limit > where && limit < end) end = limit;
        if(!parseWhere(text.substr(where + 6, end - where - 6), statement)) return false;
    }
    if(order != std::string::npos){
        auto end = limit != std::string::npos && limit > order ? limit : text.size();
        std::istringstream orderWords(text.substr(order + 9, end - order - 9));
        orderWords >> statement.orderBy;
        std::string direction;
        if(orderWords >> direction) statement.orderDescending = lower(direction) == "desc";
        if(statement.orderBy.empty()) return false;
    }
    if(limit != std::string::npos){
        try { statement.limit = static_cast<std::size_t>(std::stoul(trim(text.substr(limit + 6)))); }
        catch(...) { return false; }
    }
    return where != std::string::npos || order != std::string::npos || limit != std::string::npos || trim(text).empty();
}
std::vector<std::string> splitValues(const std::string& text){
    std::vector<std::string> values;
    std::string current;
    char quote = 0;
    for(char c : text){
        if((c == '\'' || c == '"') && (quote == 0 || quote == c)) quote = quote == 0 ? c : 0;
        if(c == ',' && quote == 0){ values.push_back(unquote(current)); current.clear(); } else current += c;
    }
    if(!current.empty()) values.push_back(unquote(current));
    return values;
}
}

PrepareResult prepareStatement(const std::string& rawInput, Statement& statement){
    std::string input = trim(rawInput);
    if(!input.empty() && input.back() == ';') input.pop_back();
    std::istringstream words(input);
    std::string command;
    words >> command;
    command = lower(command);
    if(command == "begin" || command == "commit" || command == "rollback"){
        if(command == "begin") statement.type = StatementType::Begin;
        else if(command == "commit") statement.type = StatementType::Commit;
        else statement.type = StatementType::Rollback;
        return PrepareResult::Success;
    }
    if(command == "insert"){
        statement.type = StatementType::Insert;
        const auto valuesPos = lower(input).find("values");
        if(valuesPos != std::string::npos){
            std::istringstream header(input.substr(0, valuesPos));
            std::string into;
            header >> command >> into >> statement.tableName;
            const auto open = input.find('(', valuesPos), close = input.rfind(')');
            if(open == std::string::npos || close <= open) return PrepareResult::SyntaxError;
            const auto values = splitValues(input.substr(open + 1, close - open - 1));
            if(values.empty()) return PrepareResult::SyntaxError;
            statement.generic = true;
            for(const auto& value : values) statement.values.push_back(parseValue(value));
            if(values.size() == 3){
                try { statement.row.id = static_cast<uint32_t>(std::stoul(values[0])); } catch(...) { }
                statement.row.username = values[1]; statement.row.email = values[2];
            }
            return PrepareResult::Success;
        }
        return words >> statement.row.id >> statement.row.username >> statement.row.email ? PrepareResult::Success : PrepareResult::SyntaxError;
    }
    if(command == "select"){
        statement.type = StatementType::Select;
        const auto fromKeyword = lower(input).find(" from ");
        if(fromKeyword != std::string::npos){
            for(const auto& column : splitValues(input.substr(6, fromKeyword - 6))) statement.selectedColumns.push_back(trim(column));
        }
        const auto from = fromKeyword;
        if(from == std::string::npos) return PrepareResult::Success;
        const auto sourceText = input.substr(from + 6);
        std::istringstream source(sourceText);
        source >> statement.tableName;
        const auto tableEnd = sourceText.find_first_of(" \t");
        return parseQueryModifiers(tableEnd == std::string::npos ? "" : sourceText.substr(tableEnd + 1), statement) ? PrepareResult::Success : PrepareResult::SyntaxError;
    }
    if(command == "create" || command == "drop"){
        statement.type = command == "create" ? StatementType::CreateTable : StatementType::DropTable;
        std::string tableKeyword;
        if(!(words >> tableKeyword >> statement.tableName) || lower(tableKeyword) != "table") return PrepareResult::SyntaxError;
        if(command == "create"){
            const auto open = input.find('('), close = input.rfind(')');
            statement.schema.tableName = statement.tableName;
            if(open == std::string::npos || close <= open){
                statement.schema.columns = {{"id", ColumnType::Integer, true}, {"username", ColumnType::Text}, {"email", ColumnType::Text}};
                return PrepareResult::Success;
            }
            for(const auto& definition : splitValues(input.substr(open + 1, close - open - 1))){
                std::istringstream columnWords(definition);
                std::string name, type, constraint;
                if(!(columnWords >> name >> type)) return PrepareResult::SyntaxError;
                type = lower(type);
                if(type != "integer" && type != "text") return PrepareResult::SyntaxError;
                bool primaryKey = false;
                if(columnWords >> constraint){
                    std::string second;
                    if(lower(constraint) != "primary" || !(columnWords >> second) || lower(second) != "key") return PrepareResult::SyntaxError;
                    primaryKey = true;
                }
                if(primaryKey && !statement.schema.columns.empty()) return PrepareResult::SyntaxError;
                statement.schema.columns.push_back({name, type == "integer" ? ColumnType::Integer : ColumnType::Text, primaryKey});
            }
            if(statement.schema.columns.empty()) return PrepareResult::SyntaxError;
        }
        return PrepareResult::Success;
    }
    if(command == "delete"){
        statement.type = StatementType::Delete;
        std::string from;
        if(!(words >> from >> statement.tableName) || lower(from) != "from") return PrepareResult::SyntaxError;
        std::string remainder;
        std::getline(words, remainder);
        if(!trim(remainder).empty() && lower(trim(remainder)).starts_with("where ") && !parseWhere(trim(remainder).substr(6), statement)) return PrepareResult::SyntaxError;
        return trim(remainder).empty() || !statement.conditions.empty() ? PrepareResult::Success : PrepareResult::SyntaxError;
    }
    if(command == "update"){
        statement.type = StatementType::Update;
        if(!(words >> statement.tableName)) return PrepareResult::SyntaxError;
        std::string setKeyword; if(!(words >> setKeyword) || lower(setKeyword) != "set") return PrepareResult::SyntaxError;
        std::string assignments; std::getline(words, assignments);
        const auto where = lower(assignments).find(" where ");
        if(where != std::string::npos){ if(!parseWhere(assignments.substr(where + 7), statement)) return PrepareResult::SyntaxError; assignments = assignments.substr(0, where); }
        for(const auto& assignment : splitValues(assignments)){
            const auto equals = assignment.find('='); if(equals == std::string::npos) return PrepareResult::SyntaxError;
            const auto column = lower(trim(assignment.substr(0, equals))), value = unquote(assignment.substr(equals + 1));
            statement.assignments.push_back({column, parseValue(value)});
            if(column == "username") statement.username = value; else if(column == "email") statement.email = value;
        }
        return PrepareResult::Success;
    }
    return PrepareResult::UnrecognizedStatement;
}