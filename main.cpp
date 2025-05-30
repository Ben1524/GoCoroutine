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

class SmartPtrTest : public RefObject {
public:
    SmartPtrTest() {
        p = new int(0);
        std::cout << "SmartPtrTest constructor called." << std::endl;
        deleter_.deleter_ = Deleter([](void* ptr) {
            if (ptr) {
                delete static_cast<int*>(ptr);
                std::cout << "Custom deleter called, memory freed." << std::endl;
            }
        });
    }
    virtual ~SmartPtrTest(){
        if (p) {
            delete p;
            p = nullptr;
        }
        std::cout << "SmartPtrTest destructor called." << std::endl;
    }

    // Example method to test shared_ptr
    void exampleMethod() {
        std::cout << "Example method called." << std::endl;
    }
    int *p= new int(42); // Initialize with a value for testing

};

int main()
{

}
