#include "Table.h"
#include "TestUtils.h"
#include <cassert>
#include<iostream>

void test1(){
    Table table;
    auto rows = table.getAllRows();
    assert(rows.empty());
}

void test2(){
    Table table;
    Row row = makeRow(1, "name", "email");
    table.insert(row);

    assert(table.getRowCount() == 1);
    assert(table.getAllRows().size() == (size_t)1);
}

void test3(){
    Table table;
    std::vector<Row> rows(100000);
    for(int i=0; i<100000; i++){
        Row row = makeRow(i, "name"+std::to_string(i), "email"+std::to_string(i));
        table.insert(row);
        rows[i]=row;
    }
    std::vector<Row> mrows = table.getAllRows();
    for(int i=0; i<100000; i++){
        assert(mrows[i] == rows[i]);
    }
}

int main(){
    test1();
    std::cout<<"test1 passed\n";
    test2();
    std::cout<<"test2 passed\n";
    test3();
    std::cout<<"test3 passed\n";
    return 0;
}