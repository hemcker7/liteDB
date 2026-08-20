#include "Database.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace {
constexpr char JOURNAL_MAGIC[] = "LITEDBW1";
constexpr uint32_t JOURNAL_VERSION = 1;
}

Database::Database(const std::string& filename) : filename_(filename), catalogFilename_(filename + ".catalog"){
    const auto parent = std::filesystem::path(filename_).parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent);
    recoverJournal();
    loadCatalog();
}

Database::~Database() noexcept{
    if(transactionActive_){
        try{
            rollbackTransaction();
        }catch(...){
        }
    }
}

bool Database::hasTable(const std::string& name) const{
    return tables_.find(name) != tables_.end();
}

ExecuteResult Database::execute(const Statement& statement){
    if(statement.type == StatementType::Begin){
        if(transactionActive_) return ExecuteResult::TransactionAlreadyActive;
        beginTransaction();
        return ExecuteResult::Success;
    }
    if(statement.type == StatementType::Commit){
        if(!transactionActive_) return ExecuteResult::NoActiveTransaction;
        commitTransaction();
        return ExecuteResult::Success;
    }
    if(statement.type == StatementType::Rollback){
        if(!transactionActive_) return ExecuteResult::NoActiveTransaction;
        rollbackTransaction();
        return ExecuteResult::Success;
    }
    if(statement.type == StatementType::CreateTable){
        if(hasTable(statement.tableName)) return ExecuteResult::InvalidOperation;
        auto table = std::make_unique<Table>(tableFilename(statement.tableName));
        table->create(statement.schema);
        tables_.emplace(statement.tableName, std::move(table));
        saveCatalog();
        return ExecuteResult::Success;
    }
    if(statement.type == StatementType::DropTable){
        const auto iterator = tables_.find(statement.tableName);
        if(iterator == tables_.end()) return ExecuteResult::TableNotFound;
        tables_.erase(iterator);
        std::filesystem::remove(tableFilename(statement.tableName));
        std::filesystem::remove(tableFilename(statement.tableName) + ".index");
        saveCatalog();
        return ExecuteResult::Success;
    }
    const auto iterator = tables_.find(statement.tableName);
    if(iterator == tables_.end()) return ExecuteResult::TableNotFound;
    return executeStatement(statement, *iterator->second);
}

std::string Database::tableFilename(const std::string& name) const{
    return filename_ + ".table." + name;
}

std::string Database::journalFilename() const{
    return filename_ + ".wal";
}

void Database::loadCatalog(){
    std::ifstream catalog(catalogFilename_);
    if(!catalog.is_open()) return;
    std::string tableName;
    while(std::getline(catalog, tableName)){
        if(tableName.empty()) continue;
        if(hasTable(tableName)) throw std::runtime_error("Database catalog contains a duplicate table.");
        tables_.emplace(tableName, std::make_unique<Table>(tableFilename(tableName)));
    }
}

void Database::saveCatalog() const{
    std::ofstream catalog(catalogFilename_, std::ios::trunc);
    if(!catalog.is_open()) throw std::runtime_error("Unable to write database catalog.");
    for(const auto& [name, table] : tables_) catalog << name << '\n';
}

void Database::beginTransaction(){
    for(const auto& [name, table] : tables_) table->flush();
    saveCatalog();
    writeJournal();
    transactionSnapshot_.clear();
    for(const auto& [name, table] : tables_) transactionSnapshot_.emplace(name, TableSnapshot{table->schema(), table->getAllGenericRows()});
    transactionActive_ = true;
}

void Database::commitTransaction(){
    for(const auto& [name, table] : tables_) table->flush();
    saveCatalog();
    std::filesystem::remove(journalFilename());
    transactionSnapshot_.clear();
    transactionActive_ = false;
}

void Database::rollbackTransaction(){
    std::vector<std::string> currentTables;
    for(const auto& [name, table] : tables_) currentTables.push_back(name);
    tables_.clear();
    for(const auto& name : currentTables){
        std::filesystem::remove(tableFilename(name));
        std::filesystem::remove(tableFilename(name) + ".index");
    }
    for(const auto& entry : transactionSnapshot_){
        std::filesystem::remove(tableFilename(entry.first));
        std::filesystem::remove(tableFilename(entry.first) + ".index");
    }
    for(const auto& entry : transactionSnapshot_){
        auto table = std::make_unique<Table>(tableFilename(entry.first));
        table->create(entry.second.schema);
        for(const auto& row : entry.second.rows) table->insert(row);
        tables_.emplace(entry.first, std::move(table));
    }
    saveCatalog();
    std::filesystem::remove(journalFilename());
    transactionSnapshot_.clear();
    transactionActive_ = false;
}

void Database::writeJournal() const{
    std::ofstream journal(journalFilename(), std::ios::binary | std::ios::trunc);
    if(!journal.is_open()) throw std::runtime_error("Unable to create write-ahead log.");
    journal.write(JOURNAL_MAGIC, sizeof(JOURNAL_MAGIC) - 1);
    journal.write(reinterpret_cast<const char*>(&JOURNAL_VERSION), sizeof(JOURNAL_VERSION));
    std::vector<std::string> files{catalogFilename_};
    for(const auto& [name, table] : tables_){
        files.push_back(tableFilename(name));
        files.push_back(tableFilename(name) + ".index");
    }
    const uint64_t fileCount = files.size();
    journal.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));
    for(const auto& filename : files){
        const uint64_t nameLength = filename.size();
        const bool exists = std::filesystem::exists(filename);
        const uint64_t size = exists ? std::filesystem::file_size(filename) : 0;
        journal.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        journal.write(filename.data(), static_cast<std::streamsize>(nameLength));
        const uint8_t present = exists ? 1 : 0;
        journal.write(reinterpret_cast<const char*>(&present), sizeof(present));
        journal.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if(exists){
            std::ifstream source(filename, std::ios::binary);
            std::vector<char> data(size);
            source.read(data.data(), static_cast<std::streamsize>(size));
            if(!source) throw std::runtime_error("Unable to read database file for journal.");
            journal.write(data.data(), static_cast<std::streamsize>(size));
        }
    }
    journal.flush();
    if(!journal) throw std::runtime_error("Unable to flush write-ahead log.");
}

void Database::recoverJournal(){
    if(!std::filesystem::exists(journalFilename())) return;
    std::ifstream journal(journalFilename(), std::ios::binary);
    if(!journal.is_open()) throw std::runtime_error("Unable to open write-ahead log.");
    char magic[sizeof(JOURNAL_MAGIC) - 1]{};
    uint32_t version = 0;
    uint64_t fileCount = 0;
    journal.read(magic, sizeof(magic));
    journal.read(reinterpret_cast<char*>(&version), sizeof(version));
    journal.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    if(!journal || std::memcmp(magic, JOURNAL_MAGIC, sizeof(magic)) != 0 || version != JOURNAL_VERSION || fileCount > 10000) throw std::runtime_error("Write-ahead log is corrupt.");
    struct FileImage{ std::string name; bool present; std::vector<char> data; };
    std::vector<FileImage> images;
    for(uint64_t index = 0; index < fileCount; index++){
        uint64_t nameLength = 0, size = 0;
        uint8_t present = 0;
        journal.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        if(nameLength > 4096) throw std::runtime_error("Write-ahead log is corrupt.");
        std::string name(nameLength, '\0');
        journal.read(name.data(), static_cast<std::streamsize>(nameLength));
        journal.read(reinterpret_cast<char*>(&present), sizeof(present));
        journal.read(reinterpret_cast<char*>(&size), sizeof(size));
        if(!journal || present > 1) throw std::runtime_error("Write-ahead log is corrupt.");
        std::vector<char> data(size);
        if(present) journal.read(data.data(), static_cast<std::streamsize>(size));
        if(!journal) throw std::runtime_error("Write-ahead log is corrupt.");
        images.push_back({std::move(name), present != 0, std::move(data)});
    }
    journal.close();
    const auto databasePath = std::filesystem::path(filename_);
    const auto directory = databasePath.parent_path().empty() ? std::filesystem::path(".") : databasePath.parent_path();
    const auto tablePrefix = databasePath.filename().string() + ".table.";
    for(const auto& entry : std::filesystem::directory_iterator(directory)){
        const auto entryName = entry.path().filename().string();
        if(entryName.starts_with(tablePrefix)) std::filesystem::remove(entry.path());
    }
    std::filesystem::remove(catalogFilename_);
    for(const auto& image : images) std::filesystem::remove(image.name);
    for(const auto& image : images){
        if(!image.present) continue;
        std::ofstream output(image.name, std::ios::binary | std::ios::trunc);
        if(!output.is_open()) throw std::runtime_error("Unable to restore database from write-ahead log.");
        output.write(image.data.data(), static_cast<std::streamsize>(image.data.size()));
    }
    std::filesystem::remove(journalFilename());
}