//
// Created by cxk_zjq on 25-6-1.
//

#ifndef GOCOROUTINE_SWITCHER_H
#define GOCOROUTINE_SWITCHER_H
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
namespace cxk
{
    extern void routine_sync_init_callback();  /// 注册默认的协程切换器

    /**
     * @brief 协程切换器接口
     * 提供协程上下文切换的基本操作
     */
    struct RoutineSwitcherI
    {
    public:
        virtual ~RoutineSwitcherI() {
            valid_ = false;
        }

        // 把当前routine标记为休眠状态, 但不立即休眠routine
        // 如下两种执行顺序都必须支持:
        //  mark -> sleep -> wake(其他线程执行)
        //  mark -> wake(其他线程执行) -> sleep
        virtual void mark() = 0;

        // 在routine中调用的接口，用于休眠当前routine
        // sleep函数中切换routine执行权，当routine被重新唤醒时函数才返回
        virtual void sleep() = 0;

        // 在其他routine中调用，用于唤醒休眠的routine
        // @return: 返回唤醒成功or失败
        // @要求: 一次sleep多次wake，只有其中一次wake成功，并且其他wake不会产生副作用
        virtual bool wake() = 0;

        // 判断是否在协程中 (子类switcher必须实现这个接口)
        //static bool isInRoutine();

        // 返回协程私有变量的switcher (子类switcher必须实现这个接口)
        //static RoutineSwitcherI & clsRef()

    private:
        bool valid_ = true;

    public:
        inline bool valid() const { return valid_; }
    };

class PThreadSwitcher : public RoutineSwitcherI
{
public:
    PThreadSwitcher() {
        // printf("PThreadSwitcher threadid=%d\n", (int)syscall(SYS_gettid));
    }

    virtual ~PThreadSwitcher() {
        // printf("~PThreadSwitcher threadid=%d\n", (int)syscall(SYS_gettid));
    }

    virtual void mark()override
    {
        std::unique_lock<std::mutex> lock(mtx_);
        waiting_ = true; // 标记为等待状态
    }
    virtual void sleep() override
    {
        std::unique_lock<std::mutex> lock(mtx_);
        waiting_ = true; // 标记为等待状态
        // 如果回调函数返回值为tru取消等待
        cv_.wait(lock, [this] { return !waiting_; }); // 等待被唤醒
    }
    virtual bool wake() override
    {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!waiting_) {
            return false; // 如果没有处于等待状态，返回false
        }
        waiting_ = false; // 重置等待状态
        cv_.notify_one(); // 唤醒一个等待的线程
        return true; // 返回唤醒成功
    }

    static bool isInRoutine() { return true; }  // 判断是否在协程中
    static RoutineSwitcherI & clsRef()
    {
        // 懒汉式单例模式，确保每个线程都有独立的PThreadSwitcher实例并且可以修改
        static thread_local PThreadSwitcher pts;  // 使用thread_local确保每个线程有独立的实例
        return static_cast<RoutineSwitcherI &>(pts); // 每个线程都有自己的PThreadSwitcher的引用实例
    }


private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool waiting_{false}; // 是否处于等待状态
};

/*
 * @brief RoutineSyncPolicy 类是一个策略模式的实现，用于在不同的协程 / 线程环境中选择合适的上下文切换器
 */

class RoutineSyncPolicy
{
public:
    template<typename ...Switchers>
    static bool RegisterSwitcher(int overlappedLevel=-1)  // 注册当前协程的切换器，参数为优先级
    {
        if (overlappedLevel <= refOverlappedLevel())
            return false;

        refOverlappedLevel() = overlappedLevel; // 设置当前协程切换器的优先级
        clsRefFunction() = &RoutineSyncPolicy::clsRef_T<Switchers...>;
        isInPThreadFunction() = &RoutineSyncPolicy::isInPThread_T<Switchers...>;
        return true;
    }
    static RoutineSwitcherI& ClsRef()
    {
        static bool dummy = (routine_sync_init_callback(), true);
        (void)dummy;
        return clsRefFunction()();
    }
    static bool IsInPThread()
    {
        static bool dummy = (routine_sync_init_callback(), true);  // 确保初始化回调函数被调用并且只执行一次
        (void)dummy;
        return isInPThreadFunction()();
    }

private:
    typedef std::function<RoutineSwitcherI& ()> ClsRefFunction;  // 获取当前协程的切换器引用函数类型
    typedef std::function<bool()> IsInPThreadFunction; // 判断当前是否在协程中的函数类型

    static int& refOverlappedLevel() {
        static int lv = -1;
        return lv;
    }

    static ClsRefFunction & clsRefFunction() {
        static ClsRefFunction fn;
        return fn;
    }

    static IsInPThreadFunction & isInPThreadFunction() {
        static IsInPThreadFunction fn;
        return fn;
    }

    template <typename S1, typename S2, typename ... Switchers>
    inline static RoutineSwitcherI& clsRef_T() {
        if (S1::isInRoutine()) {  // 如果S1的切换器在协程中
            return S1::clsRef(); // 使用S1的切换器
        }

        return clsRef_T<S2, Switchers...>();
    }

    template <typename S1>
    inline static RoutineSwitcherI& clsRef_T() {
        if (S1::isInRoutine()) {
            return S1::clsRef();
        }

        return PThreadSwitcher::clsRef();
    }

    template <typename S1, typename S2, typename ... Switchers>
    inline static bool isInPThread_T() {
        if (S1::isInRoutine()) {
            return false;
        }

        return isInPThread_T<S2, Switchers...>();
    }

    template <typename S1>
    inline static bool isInPThread_T() {
        if (S1::isInRoutine()) {
            return false;
        }

        return true;
    }

};

}

#endif //GOCOROUTINE_SWITCHER_H
