//
// Created by cxk_zjq on 25-5-30.
//
#include <gtest/gtest.h>
#include "common/smart_ptr.h"
#include <thread>
#include <vector>
#include <string>
#include <atomic>

using namespace cxk;


int main()
{
    std::shared_ptr<int> ptr= std::make_shared<int>(10);
    std::weak_ptr<int> weakPtr = ptr;
    std::cout << "ptr use count: " << ptr.use_count() << std::endl;
    auto sharedPtr = weakPtr.lock();
//    if (sharedPtr) {
//        std::cout << "Weak pointer is valid, value: " << *sharedPtr << std::endl;
//    } else {
//        std::cout << "Weak pointer is expired." << std::endl;
//    }
//    ptr.reset();
    std::cout << "ptr use count after lock: " << ptr.use_count() << std::endl;
    std::cout << "Weak pointer use count: " << weakPtr.use_count() << std::endl;

}
