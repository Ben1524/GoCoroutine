///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef SPINLOCK_H
#define SPINLOCK_H
#include <atomic>
#include <utils/utils.h>

namespace cxk
{
/**
 * @brief 自旋锁类
 *
 * 该类实现了一个简单的自旋锁，用于多线程环境下的互斥访问。
 * 自旋锁在获取锁时会忙等待，适用于锁持有时间较短的场景。
 */
struct LFLock
{
    /// 禁止拷贝
    LFLock(const LFLock&) = delete;
    LFLock& operator=(const LFLock&) = delete;
    std::atomic_flag flag;

    LFLock() : flag{false}
    {
    }

    /**
     *@brief 写线程的 release-store 之前的所有操作（包括原子和非原子写），对 读线程的 acquire-load 之后的所有操作可见。
     * acquire和 release内存序，特点是 release 之前的写操作必须在 release 之前完成，确保这些写操作对其他线程可见。
     * acquire 之后的读操作必须在 acquire 之后执行，确保在 acquire 之前的写操作对后续读可见。
     *
     */
    ALWAYS_INLINE void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire));
    }

    ALWAYS_INLINE bool try_lock()
    {
        return !flag.test_and_set(std::memory_order_acquire);
    }

    ALWAYS_INLINE void unlock()
    {
        flag.clear(std::memory_order_release); /// 释放之后确保对下一个acquire之后的读可见，故而使用release
    }
};

struct FakeLock {
    void lock() {}
    bool is_lock() { return false; }
    bool try_lock() { return true; }
    void unlock() {}
};
}

#endif ///SPINLOCK_H
