///
/// Created by cxk_zjq on 25-5-30.
///

#ifndef GOCOROUTINE_THREAD_SAFE_QUEUE_H
#define GOCOROUTINE_THREAD_SAFE_QUEUE_H
#pragma once
#include <utils/utils.h>
#include <cassert>
#include <concurrence/spinlock.h>
#include <mutex>
#include "smart_ptr.h"

namespace cxk
{

/*
 * @brief 侵入式数据结构Hook基类，提供双向链表所需的指针和安全链接操作
 * */
struct TSQueueHook
{
    TSQueueHook* prev = nullptr; /*@brief 指向前驱节点的指针 */
    TSQueueHook* next = nullptr; /*@brief 指向后继节点的指针 */
    void *check_ = nullptr; /*@brief 用于验证节点归属的标记，防止跨队列操作 */

    /*@brief 安全地将当前节点与下一个节点链接
     * @param theNext 待链接的下一个节点
     * @pre next为空且theNext的prev为空
     */
    ALWAYS_INLINE void link(TSQueueHook* theNext) {
        assert(next == nullptr);
        assert(theNext->prev == nullptr);
        next = theNext;
        theNext->prev = this;
    }

    /* @brief 安全地断开当前节点与下一个节点的链接
     * @param theNext 当前节点的下一个节点
     * @pre next为theNext且theNext的prev为当前节点
     */
    ALWAYS_INLINE void unlink(TSQueueHook* theNext) {
        assert(next == theNext);
        assert(theNext->prev == this);
        next = nullptr;
        theNext->prev = nullptr;
    }
};

/*@brief 侵入式双向链表，支持移动语义、遍历和O(1)删除操作  --> 此处必须注意无虚拟头结点
 * @tparam T 链表节点类型，必须继承自TSQueueHook
 */
template <typename T>
class SList
{
    static_assert((std::is_base_of<TSQueueHook, T>::value), "T must inherit TSQueueHook");

public:
    /*@brief 支持边遍历边删除的迭代器 */
    struct iterator
    {
        T* ptr; /*@brief 当前节点指针 */
        T* prev; /*@brief 前驱节点指针，用于安全删除 */
        T* next; /*@brief 后继节点指针，用于安全删除 */

        iterator() : ptr(nullptr), prev(nullptr), next(nullptr) {}
        iterator(T* p) { reset(p); }

        /*@brief 重置迭代器指向新节点，并更新前后指针 */
        void reset(T* p) {
            ptr = p;
            next = ptr ? (T*)ptr->next : nullptr;
            prev = ptr ? (T*)ptr->prev : nullptr;
        }

        friend bool operator==(iterator const& lhs, iterator const& rhs)
        /*@brief 迭代器相等比较 */
        { return lhs.ptr == rhs.ptr; }
        friend bool operator!=(iterator const& lhs, iterator const& rhs)
        /*@brief 迭代器不等比较 */
        { return !(lhs.ptr == rhs.ptr); }

        iterator& operator++() { reset(next); return *this; } /*@brief 前置自增 */
        iterator operator++(int) { iterator ret = *this; ++(*this); return ret; } /*@brief 后置自增 */
        iterator& operator--() { reset(prev); return *this; } /*@brief 前置自减 */
        iterator operator--(int) { iterator ret = *this; --(*this); return ret; } /*@brief 后置自减 */
        T& operator*() { return *(T*)ptr; } /*@brief 解引用 */
        T* operator->() { return (T*)ptr; } /*@brief 指针操作 */
    };

    T* head_; /*@brief 链表头节点（哑元节点，不存储数据） */
    T* tail_; /*@brief 链表尾节点（哑元节点，不存储数据） */
    std::size_t count_; /*@brief 链表元素数量 */

public:
    /*@brief 默认构造函数，创建空链表 */
    SList() : head_(nullptr), tail_(nullptr), count_(0) {}

    /*@brief 构造函数，用现有节点初始化链表（用于所有权转移）
     * @param h 头节点指针
     * @param t 尾节点指针
     * @param count 元素数量
     */
    SList(TSQueueHook* h, TSQueueHook* t, std::size_t count)
            : head_((T*)h), tail_((T*)t), count_(count) {}

    /*@brief 禁止拷贝构造函数 */
    SList(SList const&) = delete;
    /*@brief 禁止拷贝赋值运算符 */
    SList& operator=(SList const&) = delete;

    /*@brief 移动构造函数，转移链表所有权 */
    SList(SList<T> && other)
    {
        head_ = other.head_;
        tail_ = other.tail_;
        count_ = other.count_;
        other.stealed(); // 将源链表置空
    }

    /*@brief 移动赋值运算符，转移链表所有权 */
    SList& operator=(SList<T> && other)
    {
        clear(); // 清空当前链表
        head_ = other.head_;
        tail_ = other.tail_;
        count_ = other.count_;
        other.stealed(); // 将源链表置空
        return *this;
    }

    /*@brief 将另一个链表的所有元素追加到当前链表尾部（移动语义）
     * @param other 待追加的链表（操作后other变为空链表）
     */
    void append(SList<T> && other) {
        if (other.empty())
            return;

        if (empty()) {
            *this = std::move(other); // 当前链表为空时直接接管
            return;
        }

        tail_->link(other.head_); // 链接到当前链表尾部
        tail_ = other.tail_; // 更新尾节点
        count_ += other.count_;
        other.stealed(); // 清空源链表
    }

    /*@brief 从链表头部切割出前n个元素
     * @param n 切割数量
     * @return 包含前n个元素的新链表
     * @note 时间复杂度O(n)，慎用
     */
    SList<T> cut(std::size_t n) {
        if (empty()) return SList<T>();

        if (n >= size()) {
            SList<T> o(std::move(*this)); // 全量切割
            return o;
        }

        if (n == 0) {
            return SList<T>();
        }

        SList<T> o;
        auto pos = head_;
        for (std::size_t i = 1; i < n; ++i)
            pos = (T*)pos->next; // 定位切割点

        o.head_ = head_;
        o.tail_ = pos;
        o.count_ = n;

        count_ -= n;
        head_ = (T*)pos->next; // 重置当前链表头
        pos->unlink(head_); // 断开链接
        return o;
    }

    /*@brief 析构函数，断言链表为空 */
    ~SList()
    {
        assert(count_ == 0);
    }

    /*@brief 返回迭代器指向头节点 */
    iterator begin() const{ return iterator{head_}; }
    /*@brief 返回迭代器指向尾后节点 */
    iterator end() const{ return iterator(); }
    /*@brief 判断链表是否为空 */
    ALWAYS_INLINE bool empty() const { return head_ == nullptr; }

    /*@brief 删除迭代器指向的节点，并返回下一个迭代器
     * @param it 待删除的迭代器
     * @return 下一个迭代器
     */
    iterator erase(iterator it)
    {
        T* ptr = (it++).ptr;
        erase(ptr);
        return it;
    }

    /*@brief 带校验的删除操作
     * @param ptr 待删除的节点
     * @param check 校验标记
     * @return 删除成功与否
     */
    bool erase(T* ptr, void *check)
    {
        if (ptr->check_ != check) return false; // 校验节点归属
        erase(ptr);
        return true;
    }

    /*@brief 从链表中删除指定节点
     * @param ptr 待删除的节点
     */
    void erase(T* ptr)
    {
        if (ptr->prev) ptr->prev->next = ptr->next; // 更新前驱节点的后继指针
        else head_ = (T*)head_->next; // 删除头节点时更新头指针

        if (ptr->next) ptr->next->prev = ptr->prev; // 更新后继节点的前驱指针
        else tail_ = (T*)tail_->prev; // 删除尾节点时更新尾指针

        ptr->prev = ptr->next = nullptr; // 清空节点指针
        --count_; // 元素数量减一
        DecRef(ptr); // 减少引用计数（与智能指针配合）
    }

    /*@brief 返回链表元素数量 */
    std::size_t size() const
    {
        return count_;
    }

    /*@brief 清空链表所有元素 */
    void clear()
    {
        auto it = begin();
        while (it != end())
            it = erase(it); // 逐个删除元素
        stealed(); // 重置链表状态
    }

    /*@brief 将链表置为空状态（移动语义辅助函数） */
    void stealed()
    {
        head_ = tail_ = nullptr;
        count_ = 0;
    }

    /*@brief 返回头节点指针（哑元节点） */
    ALWAYS_INLINE TSQueueHook* head() { return head_; }
    /*@brief 返回尾节点指针（哑元节点） */
    ALWAYS_INLINE TSQueueHook* tail() { return tail_; }
};

/*@brief 线程安全的队列，支持随机删除和批量操作
 * @tparam T 队列元素类型，必须继承自TSQueueHook
 * @tparam ThreadSafe 是否启用线程安全（默认true）
 */
template <typename T, bool ThreadSafe = true>
class TSQueue
{
   static_assert((std::is_base_of<TSQueueHook, T>::value), "T must inherit TSQueueHook");

public:
    /*@brief 锁类型（根据ThreadSafe选择自旋锁或空锁） */
    typedef typename std::conditional<ThreadSafe,
            LFLock, FakeLock>::type lock_t;
    /*@brief 锁保护类型（根据ThreadSafe选择std::lock_guard或空保护） */
    typedef typename std::conditional<ThreadSafe,
            std::lock_guard<LFLock>,
            fake_lock_guard>::type LockGuard;

    lock_t ownerLock_; /*@brief 内置锁（仅在ThreadSafe=true时使用） */
    lock_t *lock_; /*@brief 锁指针（支持外部锁注入） */
    TSQueueHook* head_; /*@brief 队列头节点（虚拟节点） */
    TSQueueHook* tail_; /*@brief 队列尾节点（虚拟节点） */
    volatile std::size_t count_; /*@brief 队列元素数量（volatile保证多线程可见性） */
    void *check_; /*@brief 用于验证删除操作的归属标记 */

public:
    /*@brief 构造函数，初始化空队列 */
    TSQueue()
    {
        head_ = tail_ = new TSQueueHook; // 创建虚拟头节点和尾节点
        count_ = 0;
        check_ = this; // 校验标记指向当前队列
        lock_ = &ownerLock_; // 默认使用内置锁
    }

    /*@brief 析构函数，释放所有元素和虚拟节点 */
    ~TSQueue()
    {
        LockGuard lock(*lock_); // 加锁保证线程安全
        while (head_ != tail_) { // 逐个释放元素
            TSQueueHook *prev = tail_->prev;
//            DecRef((T *)tail_); // 减少引用计数
            tail_ = prev; // 移动尾指针到前驱节点
        }
        delete head_; // 释放虚拟头节点
        head_ = tail_ = 0;
    }

    /*@brief 设置外部锁（允许使用自定义锁类型）
     * @param lock 外部锁指针
     */
    void setLock(lock_t *lock) {
        lock_ = lock;
    }

    /*@brief 返回锁的引用（用于外部锁控制） */
    ALWAYS_INLINE lock_t& LockRef() {
        return *lock_;
    }

    /*@brief 获取队列头部元素（线程安全）
     * @param out 用于接收头部元素的指针
     */
    ALWAYS_INLINE void front(T*& out)
    {
        LockGuard lock(*lock_); // 加锁
        out = (T*)head_->next; // 头节点的下一个元素为实际头部
        if (out) out->check_ = check_; // 设置校验标记
    }

    /*@brief 获取当前元素的下一个元素（线程安全）
     * @param ptr 当前元素
     * @param out 用于接收下一个元素的指针
     */
    ALWAYS_INLINE void next(T* ptr, T*& out)
    {
        LockGuard lock(*lock_); // 加锁
        nextWithoutLock(ptr, out); // 非安全版本获取下一个元素
    }

    /*@brief 获取当前元素的下一个元素（非线程安全）
     * @param ptr 当前元素
     * @param out 用于接收下一个元素的指针
     */
    ALWAYS_INLINE void nextWithoutLock(T* ptr, T*& out)
    {
        out = (T*)ptr->next; // 直接获取下一个元素
        if (out) out->check_ = check_; // 设置校验标记
    }

    /*@brief 判断队列是否为空（线程安全） */
    ALWAYS_INLINE bool empty()
    {
        LockGuard lock(*lock_); // 加锁
        return !count_; // 通过元素数量判断空
    }

    /*@brief 判断队列是否为空（非线程安全，慎用） */
    ALWAYS_INLINE bool emptyUnsafe()
    {
        return !count_; // 直接读取数量，可能读到脏数据
    }

    /*@brief 获取队列元素数量（线程安全） */
    ALWAYS_INLINE std::size_t size()
    {
        LockGuard lock(*lock_); // 加锁
        return count_;
    }

    /*@brief 从队列头部弹出元素（线程安全）
     * @return 弹出的元素指针，队列为空时返回nullptr
     */
    ALWAYS_INLINE T* pop()
    {
        if (head_ == tail_) return nullptr; // 空队列直接返回
        LockGuard lock(*lock_); // 加锁
        if (head_ == tail_) return nullptr; // 二次检查（应对锁竞争）

        TSQueueHook* ptr = head_->next; // 实际头部元素
        if (ptr == tail_) tail_ = head_; // 弹出后队列空时重置尾节点

        head_->next = ptr->next; // 更新头节点的后继指针
        if (ptr->next) ptr->next->prev = head_; // 更新后继节点的前驱指针

        ptr->prev = ptr->next = nullptr; // 清空节点指针
        ptr->check_ = nullptr; // 清除校验标记
        --count_; // 元素数量减一
        DecRef((T*)ptr); // 减少引用计数
        return (T*)ptr; // 返回弹出的元素
    }

    /*@brief 批量入队（线程安全）
     * @param elements 待入队的链表（移动语义，入队后elements为空）
     */
    ALWAYS_INLINE void push(SList<T> && elements)
    {
        if (elements.empty()) return; // 空链表直接返回
        LockGuard lock(*lock_); // 加锁
        pushWithoutLock(std::move(elements)); // 非安全版本入队
    }

    /*@brief 批量入队（非线程安全）
     * @param elements 待入队的链表（移动语义，入队后elements为空）
     */
    ALWAYS_INLINE void pushWithoutLock(SList<T> && elements)
    {
        if (elements.empty()) return; // 空链表直接返回
        assert(elements.head_->prev == nullptr); // 校验链表头无前驱
        assert(elements.tail_->next == nullptr); // 校验链表尾无后继

        TSQueueHook* listHead = elements.head_; // 获取链表头
        count_ += elements.size(); // 更新元素数量
        tail_->link(listHead); // 链接到队列尾部
        tail_ = elements.tail_; // 更新队列尾节点
        elements.stealed(); // 清空源链表

#if LIBGO_DEBUG
        // 调试模式下设置校验标记
        for (TSQueueHook* pos = listHead; pos; pos = pos->next)
            pos->check_ = check_;
#endif
    }

    /*@brief 从头部弹出前n个元素（线程安全）
     * @param n 弹出数量
     * @return 包含前n个元素的链表
     * @note 时间复杂度O(n)，慎用
     */
    ALWAYS_INLINE SList<T> pop_front(uint32_t n)
    {
        if (head_ == tail_) return SList<T>(); // 空队列返回空链表
        LockGuard lock(*lock_); // 加锁
        if (head_ == tail_) return SList<T>(); // 二次检查

        TSQueueHook* first = head_->next; // 实际头部元素
        TSQueueHook* last = first;
        uint32_t c = 1; // 已弹出数量

#if LIBGO_DEBUG
        first->check_ = nullptr; // 清除校验标记
#endif

        // 定位第n个元素
        for (; c < n && last->next; ++c) {
            last = last->next;
#if LIBGO_DEBUG
            last->check_ = nullptr; // 清除校验标记
#endif
        }

        if (last == tail_) tail_ = head_; // 弹出后队列空时重置尾节点
        head_->next = last->next; // 更新头节点后继指针
        if (last->next) last->next->prev = head_; // 更新后继节点前驱指针

        first->prev = last->next = nullptr; // 断开链表
        count_ -= c; // 更新元素数量
        return SList<T>(first, last, c); // 返回弹出的链表
    }

    /*@brief 从尾部弹出后n个元素（线程安全）
     * @param n 弹出数量
     * @return 包含后n个元素的链表
     * @note 时间复杂度O(n)，慎用
     */
    ALWAYS_INLINE SList<T> pop_back(uint32_t n)
    {
        if (head_ == tail_) return SList<T>(); // 空队列返回空链表
        LockGuard lock(*lock_); // 加锁
        return pop_backWithoutLock(n); // 调用非安全版本
    }

    /*@brief 从尾部弹出后n个元素（非线程安全）
     * @param n 弹出数量
     * @return 包含后n个元素的链表
     * @note 时间复杂度O(n)，慎用
     */
    ALWAYS_INLINE SList<T> pop_backWithoutLock(uint32_t n)
    {
        if (head_ == tail_) return SList<T>(); // 空队列返回空链表
        TSQueueHook* last = tail_; // 实际尾节点（虚拟节点的前一个元素）
        TSQueueHook* first = last;
        uint32_t c = 1; // 已弹出数量

#if LIBGO_DEBUG
        first->check_ = nullptr; // 清除校验标记
#endif

        // 定位倒数第n个元素
        for (; c < n && first->prev != head_; ++c) {
            assert(first->prev != nullptr); // 校验前驱存在
            first = first->prev;
#if LIBGO_DEBUG
            first->check_ = nullptr; // 清除校验标记
#endif
        }

        tail_ = first->prev; // 更新尾节点到倒数第n+1个元素
        first->prev = tail_->next = nullptr; // 断开链表
        count_ -= c; // 更新元素数量
        return SList<T>(first, last, c); // 返回弹出的链表
    }

    /*@brief 弹出所有元素（线程安全）
     * @return 包含所有元素的链表
     */
    ALWAYS_INLINE SList<T> pop_all()
    {
        if (head_ == tail_) return SList<T>(); // 空队列返回空链表
        LockGuard lock(*lock_); // 加锁
        return pop_allWithoutLock(); // 调用非安全版本
    }

    /*@brief 弹出所有元素（非线程安全）
     * @return 包含所有元素的链表
     */
    ALWAYS_INLINE SList<T> pop_allWithoutLock()
    {
        if (head_ == tail_) return SList<T>(); // 空队列返回空链表

        TSQueueHook* first = head_->next; // 实际头部元素
        TSQueueHook* last = tail_; // 实际尾节点（虚拟节点）

        tail_ = head_; // 重置尾节点到虚拟节点
        head_->next = nullptr; // 清空头节点后继

        first->prev = last->next = nullptr; // 断开链表
        std::size_t c = count_; // 保存元素数量
        count_ = 0; // 清空队列
        return SList<T>(first, last, c); // 返回所有元素
    }

    /*@brief 删除指定元素（线程安全）
     * @param hook 待删除的元素
     * @param check 是否启用校验（默认true）
     * @return 删除成功与否
     */
    ALWAYS_INLINE bool erase(T* hook, bool check = false)
    {
        LockGuard lock(*lock_); // 加锁
        return eraseWithoutLock(hook, check); // 调用非安全版本
    }

    /*@brief 删除指定元素（非线程安全）
     * @param hook 待删除的元素
     * @param check 是否启用校验（默认false）
     * @param refCount 是否减少引用计数（默认true）
     * @return 删除成功与否
     */
    ALWAYS_INLINE bool eraseWithoutLock(T* hook, bool check = false, bool refCount = true)
    {
        if (check && hook->check_ != check_) return false; // 校验节点归属

#if LIBGO_DEBUG
            assert(hook->check_ == check_); // 调试断言校验
#endif

        assert(hook->prev != nullptr); // 校验前驱存在
        assert(hook == tail_ || hook->next != nullptr); // 校验后继存在

        if (hook->prev) hook->prev->next = hook->next; // 更新前驱后继指针
        if (hook->next) hook->next->prev = hook->prev; // 更新后继前驱指针
        else if (hook == tail_) tail_ = tail_->prev; // 删除尾节点时更新尾指针

        hook->prev = hook->next = nullptr; // 清空节点指针
        hook->check_ = nullptr; // 清除校验标记
        assert(count_ > 0); // 断言元素数量合法
        --count_; // 元素数量减一

        if (refCount)
            DecRef((T*)hook); // 减少引用计数
        return true;
    }

    /*@brief 单元素入队（非线程安全）
     * @param element 待入队的元素
     * @param refCount 是否增加引用计数（默认true）
     * @return 入队后的元素数量
     */
    ALWAYS_INLINE size_t pushWithoutLock(T* element, bool refCount = true)
    {
        TSQueueHook *hook = static_cast<TSQueueHook*>(element);
        assert(hook->next == nullptr); // 校验元素未在其他链表中
        assert(hook->prev == nullptr); // 校验元素未在其他链表中

        tail_->link(hook); // 链接到队列尾部
        tail_ = hook; // 更新尾节点
        hook->next = nullptr; // 确保尾节点后继为空
        hook->check_ = check_; // 设置校验标记

        ++count_; // 元素数量加一

        if (refCount)
            AddRef(element); // 增加引用计数
        return count_;
    }

    /*@brief 单元素入队（线程安全）
     * @param element 待入队的元素
     * @return 入队后的元素数量
     */
    ALWAYS_INLINE size_t push(T* element)
    {
        LockGuard lock(*lock_); // 加锁
        return pushWithoutLock(element); // 调用非安全版本
    }

    /*@brief 调试用：断言链表结构正确性（线程安全） */
    ALWAYS_INLINE void AssertLink()
    {
#if LIBGO_DEBUG
        LockGuard lock(*lock_); // 加锁
        if (head_ == tail_) return; // 空队列直接返回

        assert(!!head_); // 断言头节点存在
        assert(!!tail_); // 断言尾节点存在

        TSQueueHook* pos = tail_;
        int n = 0;
        // 逆序遍历链表，校验指针正确性
        for (; pos != head_; pos = pos->prev, ++n) {
            assert(!!pos->prev); // 断言前驱存在
        }
        assert(pos == head_); // 断言遍历到头部
#endif
    }
};

} ///namespace cxk

#endif ///GOCOROUTINE_THREAD_SAFE_QUEUE_H
