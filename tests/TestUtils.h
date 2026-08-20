#pragma once
#include "Row.h"
#include <cassert>

bool operator==(Row& row1, Row& row2){
    return (row1.id == row2.id) && (row1.username == row2.username) && (row1.email == row2.email);
}



Row makeRow(uint32_t id, std::string username, std::string email){
    Row row;
    row.id = id;
    row.username = username;
    row.email = email;
    return row;
}