//
// Created by cxk_zjq on 25-5-31.
//
#include <common/thread_safe_queue.h>
#include <gtest/gtest.h>
#include <common/smart_ptr.h>
#include <thread>
#include <concurrentqueue/concurrentqueue.h>

using namespace cxk;
// 测试类：同时继承TSQueueHook和SharedRefWrapper
class TestElement : public TSQueueHook, public SharedRefWrapper {
public:
    int value;
    explicit TestElement(int v) : value(v) {}
    ~TestElement() override {
        // 析构函数可以用于验证对象的生命周期
//        std::cout << "TestElement with value " << value << " destroyed." << std::endl;
    }
};
TEST(TSQueue, SingleElement) {
    using Queue = TSQueue<TestElement>;
    Queue queue;

    // 入队
    auto obj = SharedWrapper<TestElement>(new TestElement(42));
    queue.push(obj.get());
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_FALSE(queue.empty());

    // 出队
    RefObject* ptr = queue.pop();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(queue.size(), 0u);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(ptr, obj.get()); // 验证指针一致性
}



// 辅助函数：验证链表内容
template<typename T>
std::vector<int> collectValues(const cxk::SList<T>& list) {
    std::vector<int> values;
    for (auto it = list.begin(); it != list.end(); ++it) {
        values.push_back(it->value);
    }
    return values;
}

// 测试用例1：基础插入与遍历
TEST(SList, InsertAndTraverse) {
    cxk::SList<TestElement> list;



    // 插入元素
    auto e1 = new TestElement(1);
    // 创建哑元节点（实际代码中由SList构造函数创建）
    list.head_ = e1;
    list.tail_ = list.head_;
    auto e2 = new TestElement(2);
    list.tail_->link(e2);
    list.tail_ = e2;
    list.count_ = 2;

    // 验证遍历
    EXPECT_EQ(collectValues(list), (std::vector<int>{1, 2}));

    // 清理内存
    list.clear();
    delete list.head_; // 删除哑元节点
}

// 测试用例2：迭代器删除
TEST(SList, IteratorErase) {
    cxk::SList<TestElement> list;


    auto e1 = new TestElement(1);
    auto e2 = new TestElement(2);
    auto e3 = new TestElement(3);
    list.head_ = e1;
    list.tail_ = list.head_;
    list.tail_->link(e2);
    list.tail_ = e2;
    list.tail_->link(e3);
    list.tail_ = e3;
    list.count_ = 3;

    // 删除中间节点
    for (auto it = list.begin(); it != list.end(); ) {
        if (it->value == 2) {
            it = list.erase(it);
            break;
        } else {
            ++it;
        }
    }

    EXPECT_EQ(collectValues(list), (std::vector<int>{1, 3}));

    list.clear();
    delete list.head_;
}

// 测试用例3：移动语义
TEST(SList, MoveSemantics) {
    cxk::SList<TestElement> list1;


    auto e1 = new TestElement(1);
    list1.head_ = e1;
    list1.tail_ = list1.head_;
    list1.count_ = 1;

    // 移动构造
    cxk::SList<TestElement> list2(std::move(list1));

    EXPECT_TRUE(list1.empty());
    EXPECT_EQ(list2.size(), 1u);
    EXPECT_EQ(collectValues(list2), (std::vector<int>{1}));

    list2.clear();
    delete list2.head_;
}

// 测试用例4：切割操作
TEST(SList, CutOperation) {
    cxk::SList<TestElement> list;

    auto e1 = new TestElement(1);
    auto e2 = new TestElement(2);
    auto e3 = new TestElement(3);
    list.head_ = e1;
    list.tail_ = list.head_;
    list.tail_->link(e2);
    list.tail_ = e2;
    list.tail_->link(e3);
    list.tail_ = e3;
    list.count_ = 3;

    // 切割前两个元素
    auto front2 = list.cut(2);

    EXPECT_EQ(front2.size(), 2u);
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(collectValues(front2), (std::vector<int>{1, 2}));
    EXPECT_EQ(collectValues(list), (std::vector<int>{3}));

    front2.clear();
    list.clear();
    delete front2.head_;
    delete list.head_;
}

TEST(TSQueue, ThreadSafety) {
    using Queue = cxk::TSQueue<TestElement>;
    Queue queue;
    const int kThreadCount = 4;
    const int kElementsPerThread = 1000;

    moodycamel::ConcurrentQueue<TestElement*> moodyQueue;

    // 生产者线程：并发入队
    std::vector<std::thread> producers;
    for (int i = 0; i < kThreadCount; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < kElementsPerThread; ++j) {
                auto elem = SharedWrapper<TestElement>(new TestElement(i * kElementsPerThread + j));
                moodyQueue.enqueue(elem.get());
                queue.push(elem.get());
            }
        });
    }

    // 消费者线程：并发出队
    std::atomic<int> count1(0), count2(0);
    std::vector<std::thread> consumers1;
    for (int i = 0; i < kThreadCount; ++i) {
        consumers1.emplace_back([&]() {
            while (count1 < kThreadCount * kElementsPerThread) {
                TestElement* elem;
                elem = (TestElement*)queue.pop();
                if (elem) {
                    count1++;
                }
            }
        });
    }
    // 消费者线程：使用moodycamel队列
    std::vector<std::thread> consumers2;
    for (int i = 0; i < kThreadCount; ++i) {
        consumers2.emplace_back([&]() {
            TestElement* elem;
            while (count2 < kThreadCount * kElementsPerThread) {
                if (moodyQueue.try_dequeue(elem)) {
                    count2++;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers1) t.join();
    for (auto& t : consumers2) t.join();

    EXPECT_EQ(moodyQueue.size_approx(), 0u); // 验证moodycamel队列为空

    std::cout << "Final queue size: " << queue.size() << std::endl;
    EXPECT_TRUE(queue.empty()); // 验证所有元素都被消费
}

// 模板化性能测试函数
template <typename QueueType>
struct QueuePerformanceTester {
    QueueType queue;
    std::atomic<int> produced = 0;
    std::atomic<int> consumed = 0;
    const int kThreadCount;
    const int kElementsPerThread;
    const int kTotalElements;

    QueuePerformanceTester(int threadCount, int elementsPerThread)
            : kThreadCount(threadCount),
              kElementsPerThread(elementsPerThread),
              kTotalElements(threadCount * elementsPerThread) {}

    // 生产者线程：入队
    void Producer() {
        for (int i = 0; i < kElementsPerThread; ++i) {
            auto elem = MakeSharedWrapper<TestElement>(produced.fetch_add(1));
            queue.push(elem.get()); // 假设QueueType支持push(TestElement*)
        }
    }

    // 消费者线程：出队
    void Consumer() {
        while (consumed < kTotalElements) {
            TestElement* elem = queue.pop(); // 假设QueueType支持pop()返回TestElement*
            if (elem) {
                consumed.fetch_add(1);
            }
        }
    }

    // 运行测试并返回耗时（毫秒）
    template <typename Func>
    double RunTest(Func func, const std::string& operation) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads(kThreadCount);
        for (int i = 0; i < kThreadCount; ++i) {
            threads[i] = std::thread(func, this);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << operation << " time: " << duration << " ms, Throughput: "
                  << kTotalElements / duration * 1000 << " ops/s" << std::endl;
        return duration;
    }

    // 执行完整测试流程
    void Run() {
        std::cout << "\nTesting " << typeid(QueueType).name() << "..." << std::endl;

        // 重置状态
        produced = 0;
        consumed = 0;

        // 测试入队性能
        std::cout << "Enqueue performance:" << std::endl;
        RunTest(&QueuePerformanceTester::Producer, "  Enqueue");

        // 测试出队性能
        produced = kTotalElements; // 确保所有元素已入队
        consumed = 0;
        std::cout << "Dequeue performance:" << std::endl;
        RunTest(&QueuePerformanceTester::Consumer, "  Dequeue");

        // 验证队列空
        EXPECT_TRUE(queue.empty());
        std::cout << "Queue empty: " << (queue.empty() ? "OK" : "FAIL") << std::endl;
    }
};

// 特化moodycamel::ConcurrentQueue的生产者/消费者
template <>
struct QueuePerformanceTester<moodycamel::ConcurrentQueue<TestElement*>> {
    moodycamel::ConcurrentQueue<TestElement*> queue;
    std::atomic<int> produced = 0;
    std::atomic<int> consumed = 0;
    const int kThreadCount;
    const int kElementsPerThread;
    const int kTotalElements;

    QueuePerformanceTester(int threadCount, int elementsPerThread)
            : kThreadCount(threadCount),
              kElementsPerThread(elementsPerThread),
              kTotalElements(threadCount * elementsPerThread) {}

    void Producer() {
        for (int i = 0; i < kElementsPerThread; ++i) {
            auto elem = MakeSharedWrapper<TestElement>(produced.fetch_add(1));
            queue.enqueue(elem.get());
        }
    }

    void Consumer() {
        TestElement* elem;
        while (consumed < kTotalElements) {
            if (queue.try_dequeue(elem)) {
                consumed.fetch_add(1);
            }
        }
    }

    // 复用基类的RunTest和Run逻辑（需调整模板特化）
    template <typename Func>
    double RunTest(Func func, const std::string& operation) {
        // 与基类实现相同，无需重复编写
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads(kThreadCount);
        for (int i = 0; i < kThreadCount; ++i) {
            threads[i] = std::thread(func, this);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << operation << " time: " << duration << " ms, Throughput: "
                  << kTotalElements / duration * 1000 << " ops/s===" << std::endl;
        return duration;
    }
};

// 性能对比测试
TEST(QueuePerformance, CompareTSQueueAndMoodyCamel) {
    const int kThreadCount = 8;
    const int kElementsPerThread = 100000; // 每个线程生产10万元素，共80万
    const int kTotalElements = kThreadCount * kElementsPerThread;

    // 测试cxk::TSQueue
    {
        QueuePerformanceTester<TSQueue<TestElement>> tester(kThreadCount, kElementsPerThread);
        tester.Run();
    }

    // 测试moodycamel::ConcurrentQueue
    {
        QueuePerformanceTester<moodycamel::ConcurrentQueue<TestElement*>> tester(kThreadCount, kElementsPerThread);
        tester.RunTest(&QueuePerformanceTester<moodycamel::ConcurrentQueue<TestElement*>>::Producer, "  Enqueue");
    }
}