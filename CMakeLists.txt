cmake_minimum_required(VERSION 3.16)

project(SQLiteClone)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

enable_testing()

add_executable(sqlite_clone
    src/main.cpp
    src/InputBuffer.cpp
    src/MetaCommand.cpp
    src/Statement.cpp
    src/Table.cpp
    src/Executor.cpp
    src/Database.cpp
    src/Cursor.cpp
    src/BTreeIndex.cpp
    src/Pager.cpp
    src/Serialization.cpp
)

add_executable(sqlite_clone_sql_test
    tests/SqlTest.cpp
    src/Statement.cpp
    src/Table.cpp
    src/Pager.cpp
    src/Serialization.cpp
    src/Executor.cpp
    src/Database.cpp
    src/Cursor.cpp
    src/BTreeIndex.cpp
)
add_test(NAME sqlite_clone_sql_test COMMAND sqlite_clone_sql_test)

add_executable(litedb_server
    src/server_main.cpp
    src/Network.cpp
    src/Statement.cpp
    src/Table.cpp
    src/Executor.cpp
    src/Database.cpp
    src/Cursor.cpp
    src/BTreeIndex.cpp
    src/Pager.cpp
    src/Serialization.cpp
)

add_executable(litedb_client
    src/client_main.cpp
    src/Network.cpp
)

if(WIN32)
    target_link_libraries(litedb_server PRIVATE ws2_32)
    target_link_libraries(litedb_client PRIVATE ws2_32)
endif()