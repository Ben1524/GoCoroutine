///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef CLOCK_H
#define CLOCK_H

#pragma once
#include <ctime>
#include <chrono>
#include <thread>
#include <limits>
#include <mutex>
#include <utils/utils.h>
#include <utils/macro.h>

#include "spinlock.h"

namespace cxk
{
/**
 * @brief 高性能稳定时钟类（基于x86_64 TSC硬件计数器）
 *
 * 实现高精度时间测量，在x86_64 Unix系统上利用时间戳计数器（TSC）提升性能，
 * 并通过后台线程校准TSC与系统时钟的偏差，确保测量精度。
 * 非x86_64平台自动回退到标准库的std::chrono::steady_clock。
 */

    /// 预处理阶段确定平台特性
#if defined(LIBGO_SYS_Unix) && __x86_64__ == 1
class FastSteadyClock
{
public:
    /// 类型定义（兼容std::chrono接口）
    typedef std::chrono::steady_clock base_clock_t;        ///< 基类（标准稳定时钟）
    typedef base_clock_t::duration duration;              ///< 时间间隔类型
    typedef base_clock_t::rep rep;                        ///< 时间表示类型（通常为long long）
    typedef base_clock_t::period period;                  ///< 时间周期类型（通常为std::nano）
    typedef base_clock_t::time_point steady_time_point;          ///< 时间点类型
    static constexpr bool is_steady = true;               ///< 确保是稳定时钟（不随系统时间调整跳跃）

    /**
     * @brief 获取当前时间点
     * @return 时间点对象（time_point）
     * @note 在x86_64平台使用校准后的TSC计算，其他平台调用标准时钟
     */
    static steady_time_point now() noexcept {
        auto& data = self();
        if (!data.fast_) { /// 未校准或校准失败时使用标准时钟
            return base_clock_t::now(); /// 会由于cpu的频率发生变化以及切换导致 误差
        }
        /// 获取当前检查点（双缓冲机制）
        auto& checkPoint = data.checkPoint_[data.switchIdx_];
        uint64_t tsc_current = rdtsc();
        uint64_t dtsc = tsc_current - checkPoint.tsc_; /// TSC差值
        /// 计算时间差：校准时间点 + TSC差值 / 校准周期
        long dur = checkPoint.tp_.time_since_epoch().count() +
                   static_cast<long>(dtsc / data.cycle_);
        return steady_time_point(duration(dur));
    }

    /**
     * @brief 后台校准线程函数（x86_64平台专用）
     * @note 定期校准TSC与系统时钟的偏差，确保测量精度
     */
    static void ThreadRun() {
        auto& data = self();
        std::unique_lock<LFLock> lock(data.threadInit_, std::defer_lock);

        /// 初始化互斥锁，确保线程安全
        if (!lock.try_lock()) {
            return; /// 初始化失败（可能已存在其他线程）
        }

        /// 校准周期（20ms，可根据场景调整），校准的间隔越短越精准
        const auto calibration_interval = std::chrono::milliseconds(20);

        /// 无限循环校准
        while (true) {
            std::this_thread::sleep_for(calibration_interval);

            /// 切换检查点索引（双缓冲机制：0 <-> 1）
            int next_idx = !data.switchIdx_;
            auto& current_checkpoint = data.checkPoint_[next_idx];

            /// 记录标准时钟时间点和对应的TSC值
            current_checkpoint.tp_ = base_clock_t::now();
            current_checkpoint.tsc_ = rdtsc();

            /// 获取上一次检查点
            auto& last_checkpoint = data.checkPoint_[data.switchIdx_];

            /// 跳过首次校准（无历史数据）
            if (last_checkpoint.tsc_ == 0) {
                data.switchIdx_ = next_idx;
                continue;
            }

            /// 计算时间差（标准时钟）
            auto dur = current_checkpoint.tp_ - last_checkpoint.tp_;
            uint64_t dtsc = current_checkpoint.tsc_ - last_checkpoint.tsc_;

            /// 防止除零和数值下溢
            if (dur.count() == 0) {
                data.switchIdx_ = next_idx;
                continue;
            }

            /// 计算TSC周期（单位：周期/秒）
            float cycle = static_cast<float>(dtsc) / static_cast<float>(dur.count());
            cycle = std::max(cycle, std::numeric_limits<float>::min()); /// 最小周期保护

            /// 更新全局校准参数
            data.cycle_ = cycle;
            data.fast_ = true; /// 标记校准完成
            data.switchIdx_ = next_idx; /// 切换索引
        }
    }

private:
    struct CheckPoint {
        steady_time_point tp_;         ///< 标准时钟时间点（用于校准）
        uint64_t tsc_ = 0;      ///< 对应时刻的TSC值（时间戳计数器）TSC是用于记录CPU时钟周期数
    };

    struct Data {
        LFLock threadInit_;     ///< 自旋锁（保护校准线程初始化）
        bool fast_ = false;      ///< 校准状态（是否可用TSC快速计算）
        float cycle_ = 1.0f;     ///< TSC周期（周期数/秒，用于换算时间）
        CheckPoint checkPoint_[2]; ///< 双缓冲检查点（交替记录校准数据）
        volatile int switchIdx_ = 0; ///< 检查点索引（volatile确保内存可见性）
    };

    /// 获取单例数据实例（线程安全的静态初始化）
    static Data& self() {
        static Data instance;
        return instance;
    }

    /// 内联汇编获取TSC值（x86_64专用）
    static uint64_t rdtsc() {
        uint32_t high, low;
        __asm__ __volatile__(
                "rdtsc" : "=a" (low), "=d" (high)
                );
        return ((uint64_t)high << 32) | low;
    }
};
/// 非x86_64平台的默认实现（直接使用标准库）
#else
class FastSteadyClock : public std::chrono::steady_clock {
public:
    static void ThreadRun() {} /// 空实现（非x86_64平台无需校准）
};
#endif

} /// namespace co

#endif ///CLOCK_H
