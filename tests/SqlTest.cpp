#include "Executor.h"
#include "Database.h"
#include "BTreeIndex.h"
#include "Statement.h"
#include <array>
#include <cassert>
#include <fstream>
#include <filesystem>

Statement parse(const std::string& sql){
    Statement statement;
    assert(prepareStatement(sql, statement) == PrepareResult::Success);
    return statement;
}

int main(){
    const std::string dataDirectory = "../data/";
    BTreeIndex index;
    for(int64_t key = 30; key >= 1; key--) assert(index.insert(key, static_cast<std::size_t>(key * 10)));
    const auto entries = index.orderedEntries();
    assert(index.contains(1));
    assert(index.contains(30));
    assert(!index.contains(31));
    assert(entries.size() == 30);
    for(std::size_t position = 0; position < entries.size(); position++) assert(entries[position].first == static_cast<int64_t>(position + 1));
    index.save(dataDirectory + "btree-test.index");
    BTreeIndex reopenedIndex;
    assert(reopenedIndex.load(dataDirectory + "btree-test.index"));
    assert(reopenedIndex.find(30).value() == 300);
    std::filesystem::remove(dataDirectory + "btree-test.index");

    const std::string sqlFilename = dataDirectory + "sql-test.db";
    std::filesystem::remove(sqlFilename);
    std::filesystem::remove(sqlFilename + ".index");
    {
        Table table(sqlFilename);
        assert(executeStatement(parse("CREATE TABLE users;"), table) == ExecuteResult::Success);
        assert(executeStatement(parse("INSERT INTO users VALUES (1, 'alice', 'a@example.com');"), table) == ExecuteResult::Success);
        assert(executeStatement(parse("INSERT INTO users VALUES (1, 'copy', 'c@example.com');"), table) == ExecuteResult::DuplicateKey);
        assert(executeStatement(parse("UPDATE users SET email='new@example.com' WHERE id=1;"), table) == ExecuteResult::Success);
        assert(table.getAllRows().at(0).email == "new@example.com");
        assert(executeStatement(parse("DELETE FROM users WHERE id=1;"), table) == ExecuteResult::Success);
        assert(table.getAllRows().empty());
        assert(executeStatement(parse("DROP TABLE users;"), table) == ExecuteResult::Success);
        assert(executeStatement(parse("SELECT * FROM users;"), table) == ExecuteResult::TableNotFound);
    }
    std::filesystem::remove(sqlFilename);

    const std::string filename = dataDirectory + "persistence-test.db";
    std::filesystem::remove(filename);
    std::filesystem::remove(filename + ".index");
    {
        Table persistentTable(filename);
        persistentTable.insert(Row{7, "persisted", "row@example.com"});
    }
    {
        Table reopenedTable(filename);
        const auto rows = reopenedTable.getAllRows();
        assert(rows.size() == 1);
        assert(rows.front().id == 7);
        assert(rows.front().username == "persisted");
        assert(rows.front().email == "row@example.com");
    }
    std::filesystem::remove(filename);
    std::filesystem::remove(filename + ".index");

    const std::string corruptFilename = dataDirectory + "corrupt-test.db";
    std::filesystem::remove(corruptFilename);
    {
        std::ofstream corruptFile(corruptFilename, std::ios::binary);
        std::array<char, 4096> zeroPage{};
        corruptFile.write(zeroPage.data(), zeroPage.size());
    }
    bool rejected = false;
    try{
        Table corruptTable(corruptFilename);
    }catch(const std::runtime_error&){
        rejected = true;
    }
    assert(rejected);
    std::filesystem::remove(corruptFilename);

    const std::string schemaFilename = dataDirectory + "schema-test.db";
    std::filesystem::remove(schemaFilename);
    std::filesystem::remove(schemaFilename + ".index");

    const std::string cursorFilename = dataDirectory + "cursor-test.db";
    std::filesystem::remove(cursorFilename);
    std::filesystem::remove(cursorFilename + ".index");
    {
        Table cursorTable(cursorFilename);
        cursorTable.create("items");
        for(uint32_t id = 5; id >= 1; id--) cursorTable.insert(Row{id, "item", "value"});
        std::size_t count = 0;
        for(auto cursor = cursorTable.begin(); cursor.valid(); cursor.next()){
            const auto row = cursor.row();
            assert(std::get<int64_t>(row.values.front()) == static_cast<int64_t>(count + 1));
            count++;
        }
        assert(count == 5);
    }
    std::filesystem::remove(cursorFilename);
    std::filesystem::remove(cursorFilename + ".index");
    {
        Table schemaTable(schemaFilename);
        assert(executeStatement(parse("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);"), schemaTable) == ExecuteResult::Success);
        assert(executeStatement(parse("INSERT INTO people VALUES (1, 'Ada', 37);"), schemaTable) == ExecuteResult::Success);
        assert(executeStatement(parse("INSERT INTO people VALUES (2, NULL, NULL);"), schemaTable) == ExecuteResult::Success);
        assert(executeStatement(parse("UPDATE people SET age = 38 WHERE id = 1;"), schemaTable) == ExecuteResult::Success);
        const auto selected = parse("SELECT name, age FROM people WHERE age >= 37 AND name != NULL ORDER BY age DESC LIMIT 1;");
        assert(selected.selectedColumns.size() == 2);
        assert(selected.conditions.size() == 2);
        assert(selected.logicalOperators.front() == LogicalOperator::And);
        assert(selected.orderBy == "age");
        assert(selected.orderDescending);
        assert(selected.limit == 1);
        assert(executeStatement(selected, schemaTable) == ExecuteResult::Success);
        const auto alternate = parse("SELECT * FROM people WHERE age < 10 OR name = 'Ada';");
        assert(alternate.conditions.size() == 2);
        assert(alternate.logicalOperators.front() == LogicalOperator::Or);
        const auto rows = schemaTable.getAllGenericRows();
        assert(rows.size() == 2);
        assert(std::get<std::string>(rows[0].values[1]) == "Ada");
        assert(std::get<int64_t>(rows[0].values[2]) == 38);
        assert(std::holds_alternative<std::monostate>(rows[1].values[1]));
        assert(executeStatement(parse("UPDATE people SET name = 'Grace' WHERE age >= 38;"), schemaTable) == ExecuteResult::Success);
        assert(executeStatement(parse("DELETE FROM people WHERE name = 'Grace';"), schemaTable) == ExecuteResult::Success);
        assert(schemaTable.getAllGenericRows().size() == 1);
    }
    {
        Table reopenedSchemaTable(schemaFilename);
        assert(reopenedSchemaTable.schema().columns.size() == 3);
        assert(reopenedSchemaTable.schema().columns[1].name == "name");
        assert(reopenedSchemaTable.getAllGenericRows().size() == 1);
    }
    std::filesystem::remove(schemaFilename);
    std::filesystem::remove(schemaFilename + ".index");

    const std::string databaseFilename = dataDirectory + "multi-table-test.db";
    std::filesystem::remove(databaseFilename + ".catalog");
    std::filesystem::remove(databaseFilename + ".wal");
    std::filesystem::remove(databaseFilename + ".table.people");
    std::filesystem::remove(databaseFilename + ".table.people.index");
    std::filesystem::remove(databaseFilename + ".table.products");
    std::filesystem::remove(databaseFilename + ".table.products.index");
    {
        Database database(databaseFilename);
        assert(database.execute(parse("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT);")) == ExecuteResult::Success);
        assert(database.execute(parse("CREATE TABLE products (id INTEGER PRIMARY KEY, title TEXT);")) == ExecuteResult::Success);
        assert(database.execute(parse("INSERT INTO people VALUES (1, 'Ada');")) == ExecuteResult::Success);
        assert(database.execute(parse("INSERT INTO products VALUES (1, 'Book');")) == ExecuteResult::Success);
        assert(database.hasTable("people"));
        assert(database.hasTable("products"));
        assert(database.execute(parse("BEGIN;")) == ExecuteResult::Success);
        assert(std::filesystem::exists(databaseFilename + ".wal"));
        assert(database.execute(parse("INSERT INTO people VALUES (2, 'Rollback');")) == ExecuteResult::Success);
        assert(database.execute(parse("CREATE TABLE temporary (id INTEGER PRIMARY KEY, value TEXT);")) == ExecuteResult::Success);
        assert(database.execute(parse("ROLLBACK;")) == ExecuteResult::Success);
        assert(!database.hasTable("temporary"));
        assert(database.execute(parse("INSERT INTO people VALUES (2, 'Committed later');")) == ExecuteResult::Success);
        assert(database.execute(parse("BEGIN;")) == ExecuteResult::Success);
        assert(database.execute(parse("INSERT INTO products VALUES (2, 'Pen');")) == ExecuteResult::Success);
        assert(database.execute(parse("COMMIT;")) == ExecuteResult::Success);
        assert(!std::filesystem::exists(databaseFilename + ".wal"));
    }
    {
        Database reopenedDatabase(databaseFilename);
        assert(reopenedDatabase.hasTable("people"));
        assert(reopenedDatabase.hasTable("products"));
        assert(reopenedDatabase.execute(parse("INSERT INTO products VALUES (3, 'Pencil');")) == ExecuteResult::Success);
        assert(reopenedDatabase.execute(parse("DROP TABLE people;")) == ExecuteResult::Success);
    }
    {
        Database finalDatabase(databaseFilename);
        assert(!finalDatabase.hasTable("people"));
        assert(finalDatabase.hasTable("products"));
    }
    std::filesystem::remove(databaseFilename + ".catalog");
    std::filesystem::remove(databaseFilename + ".wal");
    std::filesystem::remove(databaseFilename + ".table.people");
    std::filesystem::remove(databaseFilename + ".table.people.index");
    std::filesystem::remove(databaseFilename + ".table.products");
    std::filesystem::remove(databaseFilename + ".table.products.index");

    const std::string crashFilename = dataDirectory + "wal-recovery-test.db";
    std::filesystem::remove(crashFilename + ".catalog");
    std::filesystem::remove(crashFilename + ".wal");
    std::filesystem::remove(crashFilename + ".table.people");
    std::filesystem::remove(crashFilename + ".table.people.index");
    const std::string savedWal = crashFilename + ".saved-wal";
    std::filesystem::remove(savedWal);
    {
        Database database(crashFilename);
        assert(database.execute(parse("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT);")) == ExecuteResult::Success);
        assert(database.execute(parse("INSERT INTO people VALUES (1, 'Original');")) == ExecuteResult::Success);
        assert(database.execute(parse("BEGIN;")) == ExecuteResult::Success);
        std::filesystem::copy_file(crashFilename + ".wal", savedWal);
        assert(database.execute(parse("INSERT INTO people VALUES (2, 'Uncommitted');")) == ExecuteResult::Success);
    }
    {
        Table dirtyTable(crashFilename + ".table.people");
        dirtyTable.insert(GenericRow{{static_cast<int64_t>(2), std::string("Dirty")}});
    }
    std::filesystem::copy_file(savedWal, crashFilename + ".wal");
    {
        Database recoveredDatabase(crashFilename);
        assert(recoveredDatabase.execute(parse("INSERT INTO people VALUES (2, 'Recovered');")) == ExecuteResult::Success);
    }
    std::filesystem::remove(savedWal);
    std::filesystem::remove(crashFilename + ".catalog");
    std::filesystem::remove(crashFilename + ".wal");
    std::filesystem::remove(crashFilename + ".table.people");
    std::filesystem::remove(crashFilename + ".table.people.index");
}