// compile using
// g++ -I../include PagerTest.cpp  ../src/Pager.cpp
#include "Pager.h"
#include <cassert>
#include <iostream>

void test1(){
    Pager pager;
    assert(pager.pageCount() == 0);
}

void test2(){
    Pager pager;
    std::byte* page = pager.getPage(0);
    assert(page != nullptr);
    assert(pager.pageCount() == 1);
}

void test3(){
    Pager pager;
    std::byte* page1 = pager.getPage(0);
    std::byte* page2 = pager.getPage(0);
    assert(page1 == page2);
    assert(pager.pageCount() == 1);
}

void test4(){
    Pager pager;
    std::byte* page = pager.getPage(5);
    assert(page != nullptr);
    assert(pager.pageCount() == 6);
}

void test5(){
    Pager pager;
    std::byte* page = pager.getPage(0);
    for(int i=0; i<PageLayout::PAGE_SIZE; i++){
        assert(*(page+i) == std::byte{0});
    }
    assert(pager.pageCount() == 1);
}

void test6(){
    Pager pager;
    std::byte* page = pager.getPage(0);
    writeRowCount(page, 7);
    assert(readRowCount(page) == 7);
}

void test7(){
    Pager pager;
    std::byte* page = pager.getPage(0);
    page[100] = std::byte{0x42};
    std::byte* newPage = pager.getPage(0);
    assert(newPage[100] == std::byte{0x42});
}

int main(){
    test1();
    std::cout<<"Test1 Passed\n";
    test2();
    std::cout<<"Test2 Passed\n";
    test3();
    std::cout<<"Test3 Passed\n";
    test4();
    std::cout<<"Test4 Passed\n";
    test5();
    std::cout<<"Test5 Passed\n";
    test6();
    std::cout<<"Test6 Passed\n";
    test7();
    std::cout<<"Test7 Passed\n";
    std::cout<<"All Tests Passed\n";
    return 0;
}

