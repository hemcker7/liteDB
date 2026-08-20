#pragma once
#include <vector>
#include <array>
#include <cstring>
#include "Constants.hpp"
#include <fstream>
#include <memory>

// inline constexpr std::size_t PAGE_SIZE = 4096;

class Pager{
    std::fstream file_;
    std::string filename_;
    std::uint64_t fileLength_ = 0;
    std::vector<std::unique_ptr<std::array<std::byte, PageLayout::PAGE_SIZE>>> pages_;
public:
    explicit Pager(const std::string& filename);
    ~Pager() noexcept;
    std::byte* getPage(std::size_t pageNumber);
    const std::byte* getPage(std::size_t pageNumber) const;
    std::size_t pageCount() const;
    std::size_t getDBPageCount(); 
    void flush();
    void clear();
private:
    void loadPage(size_t pageNumber);
    void flushPage(size_t pageNUmber);
    void flushAllPages();
};

std::uint32_t readRowCount(const std::byte* page);

void writeRowCount(std::byte* page, std::uint32_t count);

std::uint32_t readNextPage(const std::byte* page);

void writeNextPage(std::byte* page, std::uint32_t pageNumber);

std::size_t pageRowSlotOffset(size_t rowIndex);