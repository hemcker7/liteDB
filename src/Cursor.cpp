#include "Cursor.h"
#include "Constants.hpp"
#include "Serialization.hpp"
#include "Table.h"
#include <stdexcept>

Cursor::Cursor(const Table& table) : table_(&table), pageNumber_(DatabaseLayout::FIRST_DATA_PAGE), slot_(0){
    advanceToRow();
}

bool Cursor::valid() const{
    if(pageNumber_ >= table_->pager_.pageCount()) return false;
    return slot_ < readRowCount(table_->pager_.getPage(pageNumber_));
}

void Cursor::advanceToRow(){
    while(pageNumber_ < table_->pager_.pageCount() && slot_ >= readRowCount(table_->pager_.getPage(pageNumber_))){
        const auto nextPage = readNextPage(table_->pager_.getPage(pageNumber_));
        if(nextPage == PageLayout::NO_NEXT_PAGE){
            pageNumber_ = table_->pager_.pageCount();
            return;
        }
        if(nextPage >= table_->pager_.pageCount()) throw std::runtime_error("Leaf chain points outside the database.");
        pageNumber_ = nextPage;
        slot_ = 0;
    }
}

void Cursor::next(){
    if(!valid()) return;
    slot_++;
    advanceToRow();
}

GenericRow Cursor::row() const{
    if(!valid()) throw std::runtime_error("Cannot read an invalid cursor.");
    const auto* page = table_->pager_.getPage(pageNumber_);
    return Serialization::deserialize(page + pageRowSlotOffset(slot_), table_->schema_);
}