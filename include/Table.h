#pragma once
#include "Row.h"
#include "Pager.h"
#include "Cursor.h"
#include "BTreeIndex.h"
#include <vector>

class Table{
    Pager pager_;
    std::size_t rowCount_ = 0;
public:
    explicit Table(const std::string& filename = "data/litedb.db");
    void insert(const Row& row);
    void insert(const GenericRow& row);
    const std::vector<Row> getAllRows() const;
    const std::vector<GenericRow> getAllGenericRows() const;
    Cursor begin() const;
    const Schema& schema() const;
    std::size_t getRowCount();
    void flush();
    bool containsId(uint32_t id) const;
    bool containsPrimaryKey(int64_t value) const;
    void updatePrimaryKey(int64_t value, const GenericRow& row);
    void removePrimaryKey(int64_t value);
    void update(uint32_t id, const Row& row);
    void remove(uint32_t id);
    void clear();
    void create(const std::string& name);
    void create(const Schema& schema);
    void drop(const std::string& name);
    bool exists(const std::string& name) const;
private:
    friend class Cursor;
    void writeHeader();
    std::string name_ = "users";
    std::string indexFilename_;
    bool exists_ = true;
    Schema schema_{"users", {{"id", ColumnType::Integer, true}, {"username", ColumnType::Text}, {"email", ColumnType::Text}}};
    BTreeIndex primaryIndex_;
};
