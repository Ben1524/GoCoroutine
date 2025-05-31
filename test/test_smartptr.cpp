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
    EXPECT_EQ(ptr1.UseCount(), expected);

    // 拷贝构造
    cxk::IncursivePtr<MyClass> ptr2(ptr1);
    expected++;
    EXPECT_EQ(ptr1.UseCount(), expected);
    EXPECT_EQ(ptr2.UseCount(), expected);

    // 移动构造
    cxk::IncursivePtr<MyClass> ptr3(std::move(ptr2));
    EXPECT_EQ(ptr1.UseCount(), expected);  // ptr2已释放，ptr1计数不变
    EXPECT_EQ(ptr3.UseCount(), expected);

    // 赋值操作
    ptr2 = ptr1;
    expected++;
    EXPECT_EQ(ptr1.UseCount(), expected);
    EXPECT_EQ(ptr2.UseCount(), expected);

    // 释放指针
    ptr1.reset();
    expected--;
    EXPECT_EQ(ptr2.UseCount(), expected);
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
    class MyClass : public cxk::SharedRefWrapper{
        ~MyClass() {
            std::cout << "MyClass destroyed" << std::endl;
        }
    };
    cxk::IncursivePtr<MyClass> strongPtr(new MyClass());
    cxk::WeakPtr<MyClass> weakPtr(strongPtr);
    EXPECT_EQ(strongPtr.UseCount(), 1);

    // 弱指针有效，可升级为强指针
    EXPECT_TRUE(weakPtr);
    cxk::IncursivePtr<MyClass> lockedPtr = weakPtr.Lock();
    EXPECT_TRUE(lockedPtr);
    EXPECT_EQ(lockedPtr.UseCount(), 2);

    // 释放强指针，对象仍存活（弱指针不影响计数）
    strongPtr.reset();
    EXPECT_TRUE(weakPtr);  // 弱指针仍有效（关联到SharedRefWrapper）
    lockedPtr = weakPtr.Lock();
    EXPECT_EQ(lockedPtr.UseCount(), 1);  // 升级成功，计数为1
    EXPECT_TRUE(lockedPtr);  // 仍可升级（强计数+1）

    EXPECT_EQ(weakPtr.UseCount(),1);  // 弱指针计数为1（指向的对象仍然存活）
    // 释放最后一个强指针，对象销毁
    lockedPtr.reset();
    EXPECT_EQ(lockedPtr.UseCount(), 0);  // 弱指针计数为0
    EXPECT_EQ(weakPtr.UseCount(), 0);  // 弱指针计数为0
    EXPECT_FALSE(weakPtr);         // 弱指针无效（Impl已销毁）
}

TEST(SmartPtrTest, CycleReferenceWithWeakPtr) {
    class B;
    class A : public cxk::RefObject {
    public:
        cxk::WeakPtr<B> weakB;  // 用弱指针打破循环
        ~A() {
            std::cout << "A destroyed" << std::endl;
        }
    };

    class B : public cxk::RefObject {
    public:
        cxk::IncursivePtr<A> strongA;  // 强指针
        ~B() {
            std::cout << "B destroyed" << std::endl;
        }
    };

    cxk::IncursivePtr<A> a(new A());
    cxk::IncursivePtr<B> b(new B());

    a->weakB = b;  // A持有B的弱引用
    b->strongA = a;  // B持有A的强引用

    EXPECT_EQ(a.UseCount(), 2);  // A的强引用计数为2
    EXPECT_EQ(b.UseCount(), 1);  // B的强引用计数为1

    // 释放强指针，应触发销毁
    a.reset();
    b.reset();

    // 验证A和B的引用计数均为0
    // （弱指针不影响生命周期，循环被打破）
    EXPECT_EQ(a.UseCount(), 0);
    EXPECT_EQ(b.UseCount(), 0);
}

TEST(SmartPtrTest, MoveSemantics) {
    class MyClass : public cxk::RefObject {};

    // 移动构造
    cxk::IncursivePtr<MyClass> ptr1(new MyClass());
    cxk::IncursivePtr<MyClass> ptr2(std::move(ptr1));
    EXPECT_TRUE(ptr2);
    EXPECT_FALSE(ptr1);  // ptr1为空
    EXPECT_EQ(ptr2.UseCount(), 1);

    // 移动赋值
    cxk::IncursivePtr<MyClass> ptr3(new MyClass());
    ptr2 = std::move(ptr3);
    EXPECT_TRUE(ptr2);
    EXPECT_FALSE(ptr3);
    EXPECT_EQ(ptr2.UseCount(), 1);
}

#include <thread>

TEST(SmartPtrTest, MultiThreadReferenceCount) {
    class MyClass : public cxk::RefObject {};
    cxk::IncursivePtr<MyClass> ptr(new MyClass());

    // 多线程并发增加/减少计数
    auto increment = [&ptr]() {
        for (int i = 0; i < 1000; ++i) {
            ptr->AddRef();
            ptr->DecRef();
        }
    };

    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();

    EXPECT_EQ(ptr.UseCount(), 1);  // 最终计数应回到初始值1
}