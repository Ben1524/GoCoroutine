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
TEST(SmartPtrTest, BasicReferenceCount) {
    // 测试继承自RefObject的自定义类
    class MyClass : public cxk::RefObject {
    public:
        MyClass() {

        }
        ~MyClass() {
            std::cout << "MyClass destroyed" << std::endl;
        }
    };

    // 创建强指针
    cxk::IncursivePtr<MyClass> ptr1(new MyClass());
    std::size_t expected = 1;
    EXPECT_EQ(ptr1.UserCount(), expected);

    // 拷贝构造
    cxk::IncursivePtr<MyClass> ptr2(ptr1);
    expected++;
    EXPECT_EQ(ptr1.UserCount(), expected);
    EXPECT_EQ(ptr2.UserCount(), expected);

    // 移动构造
    cxk::IncursivePtr<MyClass> ptr3(std::move(ptr2));
    EXPECT_EQ(ptr1.UserCount(), expected);  // ptr2已释放，ptr1计数不变
    EXPECT_EQ(ptr3.UserCount(), expected);

    // 赋值操作
    ptr2 = ptr1;
    expected++;
    EXPECT_EQ(ptr1.UserCount(), expected);
    EXPECT_EQ(ptr2.UserCount(), expected);

    // 释放指针
    ptr1.reset();
    expected--;
    EXPECT_EQ(ptr2.UserCount(), expected);
    ptr2.reset();
    ptr3.reset();
    // 此时引用计数应为0，对象被销毁
}

TEST(SmartPtrTest, CustomDeleter) {
    class MyClass : public cxk::RefObject {
    public:
        bool deleted = false;
        ~MyClass() { deleted = true; }  // 默认析构函数
    };

    // 自定义删除器（不调用默认析构，标记状态）


    MyClass* rawPtr = new MyClass();
    // 使用lambda作为删除器
    rawPtr->SetDeleter(cxk::Deleter(
            [](cxk::RefObject* ptr, void* arg) {
                std::cout << "Lambda deleter called!" << std::endl;
            },
            nullptr
    ));

    cxk::IncursivePtr<MyClass> ptr(rawPtr);
    ptr.reset();  // 触发删除器
    EXPECT_EQ(rawPtr->UseCount(), 0);  // 计数应为0
}

TEST(SmartPtrTest, WeakPtrFunctionality) {
    class MyClass : public cxk::RefObject {};
    cxk::IncursivePtr<MyClass> strongPtr(new MyClass());
    cxk::WeakPtr<MyClass> weakPtr(strongPtr);
    EXPECT_EQ(strongPtr.UserCount(), 1);

    // 弱指针有效，可升级为强指针
    EXPECT_TRUE(weakPtr);
    cxk::IncursivePtr<MyClass> lockedPtr = weakPtr.Lock();
    EXPECT_TRUE(lockedPtr);
    EXPECT_EQ(lockedPtr.UserCount(), 2);

    // 释放强指针，对象仍存活（弱指针不影响计数）
    strongPtr.reset();
    EXPECT_TRUE(weakPtr);  // 弱指针仍有效（关联到SharedRefWrapper）
    lockedPtr = weakPtr.Lock();
    EXPECT_EQ(lockedPtr.UserCount(), 1);  // 升级成功，计数为1
    EXPECT_TRUE(lockedPtr);  // 仍可升级（强计数+1）

    // 释放最后一个强指针，对象销毁
    lockedPtr.reset();
    EXPECT_FALSE(weakPtr.Lock());  // 升级失败
    EXPECT_FALSE(weakPtr);         // 弱指针无效（Impl已销毁）
}