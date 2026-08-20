//to comile use the command below
// g++ -I../include serialization_test.cpp ../src/Table.cpp ../src/Serialization.cpp ../src/Pager.cpp

#include "Table.h"
#include "Serialization.hpp"
#include "Constants.hpp"
#include <iostream>
#include<array>
#include "TestUtils.h"


int main(){
    Row row = makeRow(2, std::string(33, 'A'), std::string(255, 'B'));
    std::array<std::byte, RecordLayout::ROW_SIZE> buffer;
    std::fill(buffer.begin(), buffer.end(), std::byte{0xAA});
    Serialization::serialize(row, buffer.data());
    // buffer[0] = (std::byte)'X';
    Row newRow;
    Serialization::deserialize(buffer.data(), newRow);
    std::cout << "Original username:  " << row.username << '\n';
    std::cout << "Recovered username: " << newRow.username << '\n';
    std::cout << "Original username length:  " << row.username.size() << '\n';
    std::cout << "Recovered username length: " << newRow.username.size() << '\n';

    std::cout << "Original username:  " << row.email << '\n';
    std::cout << "Recovered username: " << newRow.email << '\n';
    std::cout << "Original email length:  " << row.email.size() << '\n';
    std::cout << "Recovered email length: " << newRow.email.size() << '\n';
    assert(row == newRow);
    std::cout<<"Test Success\n";
    return 0;
}