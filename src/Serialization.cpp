#include <string>
#include<algorithm>
#include "Constants.hpp"
#include "Serialization.hpp"
#include <stdexcept>

void Serialization::serialize(const Row& row, std::byte* destination){
    std::memset(destination, 0, RecordLayout::ROW_SIZE);
    std::memcpy(destination+RecordLayout::ID_OFFSET, &row.id, RecordLayout::ID_SIZE);
    std::memcpy(destination+RecordLayout::USERNAME_OFFSET, row.username.c_str(), std::min(static_cast<std::size_t>(row.username.size()), RecordLayout::USERNAME_SIZE));
    std::memcpy(destination+RecordLayout::EMAIL_OFFSET, row.email.c_str(), std::min(static_cast<std::size_t>(row.email.size()), RecordLayout::EMAIL_SIZE));
}


void Serialization::deserialize(const std::byte* src, Row& row){
    std::memcpy(&row.id, src, RecordLayout::ID_SIZE);
    std::size_t username_length = strnlen(reinterpret_cast<const char*>(src + RecordLayout::USERNAME_OFFSET), RecordLayout::USERNAME_SIZE);
    row.username = std::string(reinterpret_cast<const char*>(src + RecordLayout::USERNAME_OFFSET), username_length);
    std::size_t email_length = strnlen(reinterpret_cast<const char*>(src + RecordLayout::EMAIL_OFFSET), RecordLayout::EMAIL_SIZE);
    row.email = std::string(reinterpret_cast<const char*>(src+ RecordLayout::EMAIL_OFFSET), email_length);
}

void Serialization::serialize(const GenericRow& row, const Schema& schema, std::byte* destination){
    if(row.values.size() != schema.columns.size() || row.values.size() > UINT16_MAX) throw std::runtime_error("Row does not match schema.");
    std::memset(destination, 0, RecordLayout::ROW_SIZE);
    uint16_t count = static_cast<uint16_t>(row.values.size());
    std::memcpy(destination, &count, sizeof(count));
    std::size_t offset = sizeof(count);
    for(std::size_t index = 0; index < row.values.size(); index++){
        const auto& value = row.values[index];
        const auto& column = schema.columns[index];
        uint8_t tag = std::holds_alternative<std::monostate>(value) ? 0 : (column.type == ColumnType::Integer ? 1 : 2);
        uint32_t length = tag == 1 ? sizeof(int64_t) : (tag == 2 ? static_cast<uint32_t>(std::get<std::string>(value).size()) : 0);
        if(offset + sizeof(tag) + sizeof(length) + length > RecordLayout::ROW_SIZE) throw std::runtime_error("Row is too large for a page slot.");
        std::memcpy(destination + offset, &tag, sizeof(tag)); offset += sizeof(tag);
        std::memcpy(destination + offset, &length, sizeof(length)); offset += sizeof(length);
        if(tag == 1){ const auto integer = std::get<int64_t>(value); std::memcpy(destination + offset, &integer, sizeof(integer)); }
        else if(tag == 2) std::memcpy(destination + offset, std::get<std::string>(value).data(), length);
        offset += length;
    }
}

GenericRow Serialization::deserialize(const std::byte* src, const Schema& schema){
    uint16_t count = 0;
    std::memcpy(&count, src, sizeof(count));
    if(count != schema.columns.size()) throw std::runtime_error("Stored row does not match schema.");
    GenericRow row;
    row.values.reserve(count);
    std::size_t offset = sizeof(count);
    for(const auto& column : schema.columns){
        uint8_t tag = 0;
        uint32_t length = 0;
        if(offset + sizeof(tag) + sizeof(length) > RecordLayout::ROW_SIZE) throw std::runtime_error("Corrupt row record.");
        std::memcpy(&tag, src + offset, sizeof(tag)); offset += sizeof(tag);
        std::memcpy(&length, src + offset, sizeof(length)); offset += sizeof(length);
        if(offset + length > RecordLayout::ROW_SIZE) throw std::runtime_error("Corrupt row record.");
        if(tag == 0) row.values.emplace_back(std::monostate{});
        else if(tag == 1 && length == sizeof(int64_t)){ int64_t value; std::memcpy(&value, src + offset, sizeof(value)); row.values.emplace_back(value); }
        else if(tag == 2 && column.type == ColumnType::Text) row.values.emplace_back(std::string(reinterpret_cast<const char*>(src + offset), length));
        else throw std::runtime_error("Stored value does not match schema.");
        offset += length;
    }
    return row;
}
