//
// Created by cxk_zjq on 25-5-29.
//
#include <common/clock.h>
#include <gtest/gtest.h>
#include <thread>
#include <vector>


TEST(FastSteadyClock, CalibrationMechanism) {
    using namespace cxk;
    using namespace std::chrono;

    // 启动校准线程
    std::thread calibThread(&FastSteadyClock::ThreadRun);
    calibThread.detach(); // 分离线程，测试结束后自动回收

    // 等待校准完成（至少两次校准周期）
    std::this_thread::sleep_for(milliseconds(50));

    // 测试校准后时间是否递增
    auto t1 = FastSteadyClock::now();
    // 纳秒
    std::this_thread::sleep_for(nanoseconds(1)); // 2纳秒延时
    auto t2 = FastSteadyClock::now();

    EXPECT_LE(t1.time_since_epoch().count(), t2.time_since_epoch().count());
    EXPECT_GE(duration_cast<nanoseconds>(t2 - t1).count(),1); // 允许一定误差
}

TEST(FastSteadyClock, CrossPlatformConsistency) {
    using namespace cxk;
    using namespace std::chrono;

    // 验证类型定义一致性
    static_assert(
        std::is_same_v<FastSteadyClock::duration, std::chrono::nanoseconds>,
        "Duration type must be nanoseconds"
    );

    auto tp = FastSteadyClock::now();
    auto rep = tp.time_since_epoch().count();
    static_cast<void>(rep); // 避免未使用变量警告
}