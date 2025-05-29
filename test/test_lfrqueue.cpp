//
// Created by cxk_zjq on 25-5-29.
//
#include <gtest/gtest.h>
#include "common/lock_free_ring_queue.h"
#include <thread>
#include <vector>
#include <string>
#include <atomic>

using namespace cxk;

// 测试基础功能：单生产者单消费者
TEST(LockFreeRingQueueTest, BasicSingleThread) {
    LockFreeRingQueue<int> queue(5);

    EXPECT_EQ(queue.capacity(),8-1); // 检查实际容量是否正确，因为容量是2的幂次方，扣除一个多余元素

    // 测试Push/Pop单个元素
    {
        LockFreeResult res = queue.Push(42);
        EXPECT_TRUE(res.success);
        EXPECT_TRUE(res.notify); // 从空变为非空，应通知

        // 继续放
        for (int i = 0; i < 6; ++i) {
            res = queue.Push(i);
            EXPECT_TRUE(res.success);
            EXPECT_FALSE(res.notify); // 仍然非空，不应通知
        }
        int val;
        res = queue.Pop(val);
        EXPECT_TRUE(res.success);
        EXPECT_EQ(val, 42); // 弹出第一个元素
        EXPECT_TRUE(res.notify); // 从非空变为空，应通知
    }

}

// 测试边界条件：满队列和空队列
TEST(LockFreeRingQueueTest, BoundaryConditions) {
    LockFreeRingQueue<int> queue(3); // 实际容量4（2^2）

    // 填满队列
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(queue.Push(i).success);
    }

    // 尝试Push满队列，应失败
    EXPECT_FALSE(queue.Push(4).success);

    // 弹出所有元素
    int val;
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(queue.Pop(val).success);
        EXPECT_EQ(val, i);
    }

    // 尝试Pop空队列，应失败
    EXPECT_FALSE(queue.Pop(val).success);
}

// 测试自定义类型：带构造/析构函数的类
class TestClass {
public:
    TestClass()=default;
    TestClass(int val) : value(val) {}
    TestClass(const TestClass&) = default;
    TestClass(TestClass&&) = default;
    TestClass& operator=(const TestClass&) = default;
    TestClass& operator=(TestClass&&) = default;
    ~TestClass() = default;

    bool operator==(const TestClass& other) const { return value == other.value; }

    int value;
};

TEST(LockFreeRingQueueTest, CustomTypeSupport) {
    LockFreeRingQueue<TestClass> queue(3);

    TestClass obj1(10), obj2(20);

    EXPECT_TRUE(queue.Push(obj1).success);
    EXPECT_TRUE(queue.Push(std::move(obj2)).success);

    TestClass result(10);

    EXPECT_TRUE(queue.Pop(result).success);
    EXPECT_EQ(result.value, 10);
    EXPECT_TRUE(queue.Pop(result).success);
    EXPECT_EQ(result.value, 20);
}

// 测试多线程并发：多生产者多消费者
TEST(LockFreeRingQueueTest, MultiThreadedConcurrency) {
    const int kCapacity = 80;
    const int kElements = 100;
    LockFreeRingQueue<int> queue(kCapacity);

    std::atomic<int> producer_count(0);
    std::atomic<int> consumer_count(0);
    std::vector<std::thread> producers, consumers;

    // 生产者线程：向队列中添加元素
    auto producer = [&]() {
        for (int i = 0; i < kElements; ++i) {
            while (!queue.Push(i).success) {
                std::this_thread::yield(); // 队列满时让步
            }
        }
    };

    // 消费者线程：从队列中取出元素
    auto consumer = [&]() {
        int val;
        while (consumer_count < kElements * 2) { // 确保处理所有元素
            if (queue.Pop(val).success) {
                EXPECT_LT(val, kElements); // 验证数据有效性
                consumer_count++;
            } else {
                std::this_thread::yield(); // 队列空时让步
            }
        }
    };

    // 创建2个生产者和2个消费者
    producers.emplace_back(producer);
    producers.emplace_back(producer);
    consumers.emplace_back(consumer);
    consumers.emplace_back(consumer);

    // 等待所有线程完成
    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();

}

// 测试内存管理：验证析构函数正确释放资源
TEST(LockFreeRingQueueTest, DestructionTest) {
    { // 作用域内自动销毁队列
        LockFreeRingQueue<std::string> queue(5);
        for (int i = 0; i < 3; ++i) {
            queue.Push("test");
        }
        // 析构时应正确调用string的析构函数
    }
    // 此处可配合内存检测工具（如Valgrind）验证是否有泄漏
}

// 测试通知机制：验证notify标志是否正确设置
TEST(LockFreeRingQueueTest, NotifyMechanism) {
    LockFreeRingQueue<int> queue(2); // 容量2（1+1）

    // 空队列Push，notify应为true（从空变非空）
    auto res = queue.Push(10);
    EXPECT_TRUE(res.notify);

    // 满队列Push，notify应为false（未改变空状态）
    res = queue.Push(20);
    EXPECT_FALSE(res.notify);

    // 满队列Pop，notify应为true（从满变非满）
    int val;
    res = queue.Pop(val);
    EXPECT_TRUE(res.notify);

    // 空队列Pop，notify应为false（未改变满状态）
    res = queue.Pop(val);
    EXPECT_FALSE(res.notify);
}