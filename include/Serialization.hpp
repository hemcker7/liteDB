#pragma once
#include "Row.h"
#include <cstring>
#include <cstddef>

namespace Serialization{
void serialize(const Row& row, std::byte* destination);
void deserialize(const std::byte* src, Row& row);
void serialize(const GenericRow& row, const Schema& schema, std::byte* destination);
GenericRow deserialize(const std::byte* src, const Schema& schema);
};