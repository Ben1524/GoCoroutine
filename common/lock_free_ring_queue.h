///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef GOCOROUTINE_LOCK_FREE_RING_QUEUE_H
#define GOCOROUTINE_LOCK_FREE_RING_QUEUE_H
#pragma once

#include <atomic>
#include <limits>
#include <stdexcept>

namespace cxk
{

struct LockFreeResult {
    bool success = false;
    bool notify = false;
};

/// 无锁环形队列
template<typename T, typename SizeType = std::size_t>
class LockFreeRingQueue {
public:
    typedef SizeType uint_t;
    typedef std::atomic<uint_t> atomic_t;

    /// 多申请一个typename T的空间, 便于判断full和empty.
    explicit LockFreeRingQueue(uint_t capacity)
            : capacity_(reCapacity(capacity)), readable_{0}, write_{0}, read_{0}, writable_{uint_t(capacity_ - 1)}
    {
        buffer_ = (T*) malloc(sizeof (T)*capacity_); /// malloc不会自动调用构造函数
    }

    ~LockFreeRingQueue() {
        /// destory elements.
        uint_t read = consume(read_);
        uint_t readable = consume(readable_);
        for (; read < readable; ++read) {
            buffer_[read].~T();
        }

        free(buffer_);
    }

    template<typename U>
    LockFreeResult Push(U &&t)
    {
        LockFreeResult result;

        /// 1.write_步进1.
        uint_t write, writable;
        do {
            write = relaxed(write_);
            writable = consume(writable_);
            if (write == writable) /// 队列已满
                return result;

        } while (!write_.compare_exchange_weak(write, mod(write + 1),
                                               std::memory_order_acq_rel, std::memory_order_relaxed));

        /// 2.数据写入
        new(buffer_ + write) T(std::forward<U>(t));

        /// 3.更新readable
        uint_t readable;
        do {
            readable = relaxed(readable_);
        } while (!readable_.compare_exchange_weak(write, mod(readable + 1),
                                                  std::memory_order_acq_rel, std::memory_order_relaxed));

        /// 4.检查写入时是否empty
        result.notify = (write == mod(writable + 1)); /// 如果写入时队列为空，则需要通知消费者，此时可读
        result.success = true;
        return result;
    }

    LockFreeResult Pop(T &t)
    {
        LockFreeResult result;

        /// 1.read_步进1.
        uint_t read, readable;
        do {
            read = relaxed(read_);
            readable = consume(readable_);
            if (read == readable)
                return result;

        } while (!read_.compare_exchange_weak(read, mod(read + 1),
                                              std::memory_order_acq_rel, std::memory_order_relaxed));

        /// 2.读数据
        t = std::move(buffer_[read]);
        buffer_[read].~T();


        uint_t check = mod(read + capacity_ - 1);
        while (!writable_.compare_exchange_weak(check, read,
                                                std::memory_order_acq_rel, std::memory_order_relaxed));

        /// 4.检查读取时是否full
        result.notify = (read == mod(readable_ + 1));  /// 如果读取时队列已满，则需要通知生产者，此时可写
        result.success = true;
        return result;
    }
    inline std::size_t capacity() const {
        return capacity_-1; /// 环形队列多申请了一个空间, 便于判断full和empty.
    }
private:
    inline uint_t relaxed(atomic_t &val)
    {
        return val.load(std::memory_order_relaxed);
    }

    inline uint_t acquire(atomic_t &val)
    {
        return val.load(std::memory_order_acquire);
    }

    inline uint_t consume(atomic_t &val) /// consume 内存序
    {
        return val.load(std::memory_order_consume);
    }

    /// 优化后的位运算（仅适用于capacity_是2的幂）
    inline uint_t mod(uint_t val) {
        return val & (capacity_ - 1);
    }

    /// 将容量向上取整为2的幂（例如：7→8, 8→8, 9→16）
    inline std::size_t reCapacity(uint_t capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("Capacity must be positive");
        }

        /// 防止溢出
        if (capacity >= (std::numeric_limits<uint_t>::max() / 2)) {
            return std::numeric_limits<uint_t>::max() / 2;
        }

        /// 向上取整为2的幂
        --capacity;
        capacity |= capacity >> 1; /// 将最高位设置为1
        capacity |= capacity >> 2; /// 将次高位设置为1
        capacity |= capacity >> 4; /// 将更低位设置为1
        capacity |= capacity >> 8;
        capacity |= capacity >> 16;
        return capacity + 1; /// 返回下一个2的幂
    }


private:
    std::size_t capacity_;
    T *buffer_;

    /// [write_, writable_] 可写区间, write_ == writable_ is full.
    /// read后更新writable
    atomic_t write_;
    atomic_t writable_;

    /// [read_, readable_) 可读区间, read_ == readable_ is empty.
    /// write后更新readable
    atomic_t read_;
    atomic_t readable_;
};

} /// namespace cxk

#endif ///GOCOROUTINE_LOCK_FREE_RING_QUEUE_H
