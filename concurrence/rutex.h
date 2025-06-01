//
// Created by cxk_zjq on 25-6-1.
//

#ifndef GOCOROUTINE_RUTEX_H
#define GOCOROUTINE_RUTEX_H
#include <atomic>
#include "linked_list.h"
#include <mutex>
#include "debug.h"
/*
 * @brief POSIX Futex（快速用户空间互斥锁）
 * Futex 的核心思想是：先在用户空间尝试操作，只有当锁确实被占用时才进入内核态等待。
 * 这种设计大幅减少了不必要的系统调用，提高了性能。
 */

namespace cxk
{
/// @brief 基础整数值类型模板，根据 Reference 参数选择是否使用引用外部
template<typename IntValueType, bool Reference>
struct IntValue;

/// @brief 引用类型的模板偏特化
template<typename IntValueType>
struct IntValue<IntValueType, true>
{
public:
    inline std::atomic<IntValueType> *value()
    { return ptr_; }

    inline void ref(std::atomic<IntValueType> *ptr)
    { ptr_ = ptr; }

protected:
    std::atomic<IntValueType> *ptr_{nullptr};
};

template <typename IntValueType>
struct IntValue<IntValueType, false>
{
public:
    inline std::atomic<IntValueType>* value() { return &value_; }

protected:
    std::atomic<IntValueType> value_ {0};
};


struct RutexBase
{
public:
    enum rutex_wait_return {
        rutex_wait_return_success = 0,
        rutex_wait_return_etimeout = 1,
        rutex_wait_return_ewouldblock = 2,
        rutex_wait_return_eintr = 3,
    };

    inline static const char* etos(rutex_wait_return v) {
#define __SWITCH_CASE_ETOS(x) case x: return #x
        switch (v) {
            __SWITCH_CASE_ETOS(rutex_wait_return_success);
            __SWITCH_CASE_ETOS(rutex_wait_return_etimeout);
            __SWITCH_CASE_ETOS(rutex_wait_return_ewouldblock);
            __SWITCH_CASE_ETOS(rutex_wait_return_eintr);
            default: return "Unknown rutex_wait_return";
        }
#undef __SWITCH_CASE_ETOS
    }

protected:
    friend struct RutexWaiter;
    LinkedList waiters_; ///< 等待者链表，存储等待此锁的协程
    std::mutex mtx_; ///< 互斥锁，用于保护等待者链表的访问
};

/*
 * @brief 等待者结构体
 */
struct RutexWaiter : public LinkedNode, public DebuggerId<RutexWaiter>
{

};


}

#endif //GOCOROUTINE_RUTEX_H
