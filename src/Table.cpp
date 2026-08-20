#include "Table.h"
#include "Constants.hpp"
#include "Serialization.hpp"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
void writeText(std::byte* page, std::size_t offset, std::size_t size, const std::string& value){
    std::memset(page + offset, 0, size);
    std::memcpy(page + offset, value.data(), std::min(size, value.size()));
}

std::string readText(const std::byte* page, std::size_t offset, std::size_t size){
    const auto* text = reinterpret_cast<const char*>(page + offset);
    return std::string(text, strnlen(text, size));
}
}

Table::Table(const std::string& filename) : pager_(filename), indexFilename_(filename + ".index") {
    if(pager_.pageCount() == 0){
        schema_.tableName = name_;
        writeHeader();
        primaryIndex_.clear();
        primaryIndex_.save(indexFilename_);
        return;
    }
    const auto* header = pager_.getPage(DatabaseLayout::HEADER_PAGE);
    if(std::memcmp(header + DatabaseLayout::MAGIC_OFFSET, DatabaseLayout::MAGIC, DatabaseLayout::MAGIC_SIZE) != 0){
        throw std::runtime_error("Database file has an invalid magic value.");
    }
    uint32_t version = 0;
    uint32_t tableCount = 0;
    uint32_t rootPage = 0;
    std::memcpy(&version, header + DatabaseLayout::VERSION_OFFSET, sizeof(version));
    std::memcpy(&tableCount, header + DatabaseLayout::TABLE_COUNT_OFFSET, sizeof(tableCount));
    std::memcpy(&rootPage, header + DatabaseLayout::ROOT_PAGE_OFFSET, sizeof(rootPage));
    if(version != DatabaseLayout::FORMAT_VERSION || tableCount > 1 || rootPage != DatabaseLayout::FIRST_DATA_PAGE){
        throw std::runtime_error("Database file has an unsupported or corrupt header.");
    }
    exists_ = tableCount == 1;
    name_ = readText(header, DatabaseLayout::TABLE_NAME_OFFSET, DatabaseLayout::TABLE_NAME_SIZE);
    if(name_.empty()) name_ = "users";
    schema_.tableName = name_;
    uint32_t columnCount = 0;
    std::memcpy(&columnCount, header + DatabaseLayout::COLUMN_COUNT_OFFSET, sizeof(columnCount));
    if(columnCount == 0 || columnCount > DatabaseLayout::MAX_COLUMNS) throw std::runtime_error("Database file has an invalid schema.");
    schema_.columns.clear();
    for(std::size_t index = 0; index < columnCount; index++){
        const auto columnName = readText(header, DatabaseLayout::COLUMN_NAME_OFFSET + index * DatabaseLayout::COLUMN_NAME_SIZE, DatabaseLayout::COLUMN_NAME_SIZE);
        const auto columnType = readText(header, DatabaseLayout::COLUMN_TYPE_OFFSET + index * DatabaseLayout::COLUMN_TYPE_SIZE, DatabaseLayout::COLUMN_TYPE_SIZE);
        if(columnName.empty() || (columnType != "INTEGER" && columnType != "TEXT")) throw std::runtime_error("Database file has an invalid column definition.");
        const bool primaryKey = *(reinterpret_cast<const uint8_t*>(header + DatabaseLayout::PRIMARY_KEY_OFFSET + index)) != 0;
        schema_.columns.push_back({columnName, columnType == "INTEGER" ? ColumnType::Integer : ColumnType::Text, primaryKey});
    }
    if(!exists_) return;
    for(std::size_t pageNumber = DatabaseLayout::FIRST_DATA_PAGE; pageNumber < pager_.pageCount(); pageNumber++){
        const auto pageRowCount = readRowCount(pager_.getPage(pageNumber));
        if(pageRowCount > PageLayout::ROWS_PER_PAGE) throw std::runtime_error("Database file has an invalid row count.");
        rowCount_ += pageRowCount;
    }
    if(schema_.columns.front().primaryKey){
        if(!primaryIndex_.load(indexFilename_)){
            const auto storedRows = getAllGenericRows();
            for(std::size_t rowNumber = 0; rowNumber < storedRows.size(); rowNumber++){
                const auto& row = storedRows[rowNumber];
                if(!row.values.empty() && std::holds_alternative<int64_t>(row.values.front())) primaryIndex_.insert(std::get<int64_t>(row.values.front()), rowNumber);
            }
            primaryIndex_.save(indexFilename_);
        }
    }
}

// const std::vector<Row>& Table::rows() const{
//     return rows_;
// }

// void Table::insert(const Row& row){
//     rows_.push_back(row);
// }

void Table::insert(const Row& row){
    insert(GenericRow{{static_cast<int64_t>(row.id), row.username, row.email}});
}

void Table::insert(const GenericRow& row){
    if(row.values.size() != schema_.columns.size()) throw std::runtime_error("Row does not match schema.");
    const bool ordered = schema_.columns.front().primaryKey && std::holds_alternative<int64_t>(row.values.front());
    std::size_t pageNo = DatabaseLayout::FIRST_DATA_PAGE;
    while(true){
        const auto* page = pager_.getPage(pageNo);
        const auto pageRowCount = readRowCount(page);
        const auto nextPage = readNextPage(page);
        if(pageRowCount == 0) break;
        if(ordered){
            const auto last = Serialization::deserialize(page + pageRowSlotOffset(pageRowCount - 1), schema_);
            if(std::get<int64_t>(row.values.front()) <= std::get<int64_t>(last.values.front())) break;
        }else if(nextPage == PageLayout::NO_NEXT_PAGE) break;
        if(nextPage == PageLayout::NO_NEXT_PAGE) break;
        pageNo = nextPage;
    }

    const auto existingPage = pager_.getPage(pageNo);
    const auto existingCount = readRowCount(existingPage);
    const auto nextPage = readNextPage(existingPage);
    std::vector<GenericRow> rows;
    rows.reserve(existingCount + 1);
    for(std::size_t index = 0; index < existingCount; index++) rows.push_back(Serialization::deserialize(existingPage + pageRowSlotOffset(index), schema_));
    rows.push_back(row);
    if(ordered) std::sort(rows.begin(), rows.end(), [](const GenericRow& left, const GenericRow& right){ return std::get<int64_t>(left.values.front()) < std::get<int64_t>(right.values.front()); });

    auto writeLeaf = [this](std::size_t number, const std::vector<GenericRow>& leafRows, std::uint32_t next){
        auto* page = pager_.getPage(number);
        std::fill(page, page + PageLayout::PAGE_SIZE, std::byte{0});
        writeRowCount(page, static_cast<std::uint32_t>(leafRows.size()));
        writeNextPage(page, next);
        for(std::size_t index = 0; index < leafRows.size(); index++) Serialization::serialize(leafRows[index], schema_, page + pageRowSlotOffset(index));
    };

    if(rows.size() <= PageLayout::ROWS_PER_PAGE){
        writeLeaf(pageNo, rows, nextPage);
    }else{
        const auto middle = rows.size() / 2;
        std::vector<GenericRow> left(rows.begin(), rows.begin() + middle);
        std::vector<GenericRow> right(rows.begin() + middle, rows.end());
        const auto rightPage = pager_.pageCount();
        writeLeaf(pageNo, left, static_cast<std::uint32_t>(rightPage));
        writeLeaf(rightPage, right, nextPage);
    }
    if(schema_.columns.front().primaryKey && !row.values.empty() && std::holds_alternative<int64_t>(row.values.front())) primaryIndex_.insert(std::get<int64_t>(row.values.front()), rowCount_);
    primaryIndex_.save(indexFilename_);
    rowCount_++;
}

const std::vector<Row> Table::getAllRows() const{
    std::vector<Row> rows;
    for(const auto& genericRow : getAllGenericRows()){
        if(genericRow.values.size() != 3 || !std::holds_alternative<int64_t>(genericRow.values[0]) || !std::holds_alternative<std::string>(genericRow.values[1]) || !std::holds_alternative<std::string>(genericRow.values[2])) throw std::runtime_error("Table schema is not compatible with Row.");
        rows.push_back(Row{static_cast<uint32_t>(std::get<int64_t>(genericRow.values[0])), std::get<std::string>(genericRow.values[1]), std::get<std::string>(genericRow.values[2])});
    }
    return rows;
}

const std::vector<GenericRow> Table::getAllGenericRows() const{
    std::vector<GenericRow> rows;
    rows.reserve(rowCount_);
    for(auto cursor = begin(); cursor.valid(); cursor.next()) rows.push_back(cursor.row());
    if(rows.size() != rowCount_) throw std::runtime_error("Database file is missing rows.");
    return rows;
}

Cursor Table::begin() const{
    return Cursor(*this);
}

std::size_t Table::getRowCount(){
    return rowCount_;
}

void Table::flush(){
    pager_.flush();
}

const Schema& Table::schema() const{
    return schema_;
}

bool Table::containsId(uint32_t id) const{
    const auto rows = getAllRows();
    return std::any_of(rows.begin(), rows.end(), [id](const Row& row){ return row.id == id; });
}

void Table::clear(){
    pager_.clear();
    rowCount_ = 0;
    primaryIndex_.clear();
    primaryIndex_.save(indexFilename_);
    writeHeader();
}

void Table::update(uint32_t id, const Row& row){
    auto rows = getAllRows();
    for(auto& current : rows) if(current.id == id) current = row;
    clear();
    for(const auto& current : rows) insert(current);
}

void Table::remove(uint32_t id){
    const auto rows = getAllRows();
    clear();
    for(const auto& row : rows) if(row.id != id) insert(row);
}

void Table::create(const std::string& name){
    create(Schema{name, {{"id", ColumnType::Integer, true}, {"username", ColumnType::Text}, {"email", ColumnType::Text}}});
}

void Table::create(const Schema& schema){
    if(schema.columns.empty() || schema.columns.size() > DatabaseLayout::MAX_COLUMNS) throw std::runtime_error("Invalid schema.");
    for(std::size_t index = 1; index < schema.columns.size(); index++) if(schema.columns[index].primaryKey) throw std::runtime_error("Only the first column can be a primary key.");
    schema_ = schema;
    name_ = schema.tableName;
    exists_ = true;
    clear();
}

void Table::drop(const std::string& name){
    if(name == name_){
        clear();
        exists_ = false;
        writeHeader();
    }
}

bool Table::exists(const std::string& name) const{
    return exists_ && name == name_;
}

void Table::writeHeader(){
    auto* header = pager_.getPage(DatabaseLayout::HEADER_PAGE);
    std::fill(header, header + PageLayout::PAGE_SIZE, std::byte{0});
    std::memcpy(header + DatabaseLayout::MAGIC_OFFSET, DatabaseLayout::MAGIC, DatabaseLayout::MAGIC_SIZE);
    const uint32_t version = DatabaseLayout::FORMAT_VERSION;
    const uint32_t tableCount = exists_ ? 1 : 0;
    const uint32_t rootPage = DatabaseLayout::FIRST_DATA_PAGE;
    const uint32_t columnCount = static_cast<uint32_t>(schema_.columns.size());
    std::memcpy(header + DatabaseLayout::VERSION_OFFSET, &version, sizeof(version));
    std::memcpy(header + DatabaseLayout::TABLE_COUNT_OFFSET, &tableCount, sizeof(tableCount));
    std::memcpy(header + DatabaseLayout::ROOT_PAGE_OFFSET, &rootPage, sizeof(rootPage));
    std::memcpy(header + DatabaseLayout::COLUMN_COUNT_OFFSET, &columnCount, sizeof(columnCount));
    writeText(header, DatabaseLayout::TABLE_NAME_OFFSET, DatabaseLayout::TABLE_NAME_SIZE, name_);
    for(std::size_t index = 0; index < schema_.columns.size(); index++){
        writeText(header, DatabaseLayout::COLUMN_NAME_OFFSET + index * DatabaseLayout::COLUMN_NAME_SIZE, DatabaseLayout::COLUMN_NAME_SIZE, schema_.columns[index].name);
        writeText(header, DatabaseLayout::COLUMN_TYPE_OFFSET + index * DatabaseLayout::COLUMN_TYPE_SIZE, DatabaseLayout::COLUMN_TYPE_SIZE, schema_.columns[index].type == ColumnType::Integer ? "INTEGER" : "TEXT");
        const uint8_t primaryKey = schema_.columns[index].primaryKey ? 1 : 0;
        std::memcpy(header + DatabaseLayout::PRIMARY_KEY_OFFSET + index, &primaryKey, sizeof(primaryKey));
    }
}

bool Table::containsPrimaryKey(int64_t value) const{
    return primaryIndex_.contains(value);
}

void Table::updatePrimaryKey(int64_t value, const GenericRow& replacement){
    auto rows = getAllGenericRows();
    for(auto& row : rows) if(!row.values.empty() && std::holds_alternative<int64_t>(row.values.front()) && std::get<int64_t>(row.values.front()) == value) row = replacement;
    clear();
    for(const auto& row : rows) insert(row);
}

void Table::removePrimaryKey(int64_t value){
    const auto rows = getAllGenericRows();
    clear();
    for(const auto& row : rows) if(row.values.empty() || !std::holds_alternative<int64_t>(row.values.front()) || std::get<int64_t>(row.values.front()) != value) insert(row);
}