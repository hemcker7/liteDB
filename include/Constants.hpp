#pragma once
#include <cstdint>

namespace RecordLayout{
    inline constexpr size_t ID_SIZE = sizeof(uint32_t);
    inline constexpr size_t USERNAME_SIZE = 32;
    inline constexpr size_t EMAIL_SIZE = 255;
    inline constexpr size_t ID_OFFSET = 0;
    inline constexpr size_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
    inline constexpr size_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
    inline constexpr size_t ROW_SIZE = 1024;
};

namespace DatabaseLayout{
    inline constexpr std::size_t HEADER_PAGE = 0;
    inline constexpr std::size_t FIRST_DATA_PAGE = 1;
    inline constexpr std::size_t MAGIC_OFFSET = 0;
    inline constexpr std::size_t MAGIC_SIZE = 8;
    inline constexpr std::size_t VERSION_OFFSET = 8;
    inline constexpr std::size_t TABLE_COUNT_OFFSET = 12;
    inline constexpr std::size_t ROOT_PAGE_OFFSET = 16;
    inline constexpr std::size_t COLUMN_COUNT_OFFSET = 20;
    inline constexpr std::size_t TABLE_NAME_OFFSET = 24;
    inline constexpr std::size_t TABLE_NAME_SIZE = 64;
    inline constexpr std::size_t COLUMN_NAME_OFFSET = 96;
    inline constexpr std::size_t COLUMN_NAME_SIZE = 32;
    inline constexpr std::size_t COLUMN_TYPE_OFFSET = 640;
    inline constexpr std::size_t COLUMN_TYPE_SIZE = 16;
    inline constexpr std::size_t PRIMARY_KEY_OFFSET = 896;
    inline constexpr std::size_t MAX_COLUMNS = 16;
    inline constexpr std::uint32_t FORMAT_VERSION = 3;
    inline constexpr char MAGIC[MAGIC_SIZE] = {'L', 'I', 'T', 'E', 'D', 'B', '0', '1'};
}

namespace PageLayout{
    inline constexpr std::size_t PAGE_SIZE = 4096;
    inline constexpr std::size_t HEADER_SIZE = sizeof(uint32_t) * 2;
    inline constexpr std::size_t ROW_COUNT_OFFSET = 0;
    inline constexpr std::size_t NEXT_PAGE_OFFSET = sizeof(uint32_t);
    inline constexpr std::uint32_t NO_NEXT_PAGE = 0;
    inline constexpr std::size_t ROW_OFFSET = HEADER_SIZE;
    inline constexpr std::size_t ROWS_PER_PAGE = (PAGE_SIZE - HEADER_SIZE) / RecordLayout::ROW_SIZE;
}