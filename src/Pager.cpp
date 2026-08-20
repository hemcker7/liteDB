#include "Pager.h"
#include "Constants.hpp"
#include <cassert>

Pager::Pager(const std::string& filename) : filename_(filename){
    file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if(!file_.is_open()){
        file_.clear();
        file_.open(filename, std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);        
    }
    if (!file_.is_open()) {
        throw std::runtime_error("Unable to open or create database file.");
    }
    file_.seekg(0, std::ios::end);//jumps to end of the file
    const std::streampos end = file_.tellg(); //get the curr pos which is the size of the file

    if(end == std::streampos(-1)){
        throw std::runtime_error("Failed to determine database filesize");
    }

    fileLength_ = static_cast<std::uint64_t>(end);

    if(fileLength_ % PageLayout::PAGE_SIZE != 0){
        throw std::runtime_error("Database file is corrupt: file size is not multiple PAGE_SIZE");
    }

    // file_.seekg(0, std::ios::beg);//reset cursor to beggining of the file

    pages_.resize(fileLength_ / PageLayout::PAGE_SIZE);

}

Pager::~Pager() noexcept{
    try{
        flushAllPages();
    }catch(...){
    }
}

std::size_t Pager::getDBPageCount(){
    return static_cast<std::size_t>(fileLength_ / PageLayout::PAGE_SIZE);
}

void Pager::flush(){
    flushAllPages();
}

std::byte* Pager::getPage(std::size_t pageNumber){
    //return ptr to page, if not exist allocate memory for the page and return ptr
    if(pageNumber >= pages_.size()){
        pages_.resize(pageNumber+1);
    }
    if(!pages_[pageNumber]) loadPage(pageNumber);
    return pages_[pageNumber]->data();
}

const std::byte* Pager::getPage(std::size_t pageNumber) const{
    //return ptr to page, if not exist allocate memory for the page and return ptr
    // if(pageNumber >= pages_.size()){
    //     pages_.resize(pageNumber+1);
    //     return pages_[pageNumber].data();
    // }
    assert(pageNumber < pages_.size());
    return pages_[pageNumber]->data();
}

std::size_t Pager::pageCount() const{
    return pages_.size();
}

void Pager::clear(){
    pages_.clear();
    file_.close();
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if(!file_.is_open()) throw std::runtime_error("Unable to truncate database file.");
    fileLength_ = 0;
}

void Pager::loadPage(size_t pageNumber){
    // std::array<std::byte, PageLayout::PAGE_SIZE> page;
    pages_[pageNumber] = std::make_unique<std::array<std::byte, PageLayout::PAGE_SIZE>>();
    std::streampos targetPos = PageLayout::PAGE_SIZE * pageNumber;
    if(pageNumber < getDBPageCount()){
        file_.clear();
        file_.seekg(targetPos, std::ios::beg);
        file_.read(reinterpret_cast<char*>(pages_[pageNumber]->data()), PageLayout::PAGE_SIZE);
    }
    else{
        std::fill(pages_[pageNumber]->data(), pages_[pageNumber]->data() + PageLayout::PAGE_SIZE, std::byte{0});
    }
}

void Pager::flushPage(size_t pageNumber){
    if(!pages_[pageNumber]) return;
    file_.clear();
    file_.seekp(static_cast<std::streamoff>(PageLayout::PAGE_SIZE * pageNumber), std::ios::beg);
    file_.write(reinterpret_cast<const char*>(pages_[pageNumber]->data()), PageLayout::PAGE_SIZE);
    if(!file_) throw std::runtime_error("Failed to write database page.");
    file_.flush();
}

void Pager::flushAllPages(){
    for(std::size_t pageNumber = 0; pageNumber < pages_.size(); pageNumber++) flushPage(pageNumber);
}

std::uint32_t readRowCount(const std::byte* page){
    uint32_t count;
    memcpy(&count, page+PageLayout::ROW_COUNT_OFFSET, sizeof(count));
    return count;
}

void writeRowCount(std::byte* page, std::uint32_t count){
    memcpy(page + PageLayout::ROW_COUNT_OFFSET, &count, sizeof(count));
}

std::uint32_t readNextPage(const std::byte* page){
    uint32_t nextPage = 0;
    memcpy(&nextPage, page + PageLayout::NEXT_PAGE_OFFSET, sizeof(nextPage));
    return nextPage;
}

void writeNextPage(std::byte* page, std::uint32_t pageNumber){
    memcpy(page + PageLayout::NEXT_PAGE_OFFSET, &pageNumber, sizeof(pageNumber));
}

std::size_t pageRowSlotOffset(size_t rowIndex){
    return PageLayout::ROW_OFFSET + rowIndex*RecordLayout::ROW_SIZE;
}

