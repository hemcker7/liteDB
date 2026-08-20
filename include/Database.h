#pragma once

#include "Executor.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Database{
public:
    explicit Database(const std::string& filename = "data/litedb.db");
    ~Database() noexcept;
    ExecuteResult execute(const Statement& statement);
    bool hasTable(const std::string& name) const;

private:
    std::string filename_;
    std::string catalogFilename_;
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    struct TableSnapshot{
        Schema schema;
        std::vector<GenericRow> rows;
    };
    bool transactionActive_ = false;
    std::unordered_map<std::string, TableSnapshot> transactionSnapshot_;

    std::string tableFilename(const std::string& name) const;
    std::string journalFilename() const;
    void loadCatalog();
    void saveCatalog() const;
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    void writeJournal() const;
    void recoverJournal();
};