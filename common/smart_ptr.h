/**
 * @file smart_ptr.h
 * @brief 智能指针类，提供半侵入式的引用计数管理
 * @author cxk_zjq
 * @date 2025-05-30
 */

#ifndef GOCOROUTINE_SMART_PTR_H
#define GOCOROUTINE_SMART_PTR_H

#include <string>
#include <atomic>
#include <utils/utils.h>
#include <cstring>
#include <memory>
#include <functional>

namespace cxk
{

/**
 * @brief 可空锁保护（无实际锁功能，用于优化空指针场景）
 * 模板构造函数接受任意互斥体类型，实际不执行锁定操作
 */
struct fake_lock_guard
{
    /**
     * @brief 模板构造函数
     * @tparam Mutex 互斥体类型（未实际使用）
     * @param m 互斥体引用（未实际使用）
     */
    template<typename Mutex>
    explicit fake_lock_guard(Mutex &)
    {}
};

/**
 * @brief 核心模块：半侵入式引用计数体系
 * 设计目标：结合侵入式高性能与共享指针易用性，支持对象复用与弱引用
 */
class RefObject;

/**
 * @brief 自定义删除器结构体
 * 支持用户传入自定义释放函数和参数，实现资源的灵活释放，避免使用delete导致类释放不完全
 */
struct Deleter
{
    // using func_t = void (*)(RefObject *ptr, void *arg);
    /** 删除函数类型，使用std::function支持更灵活的函数签名 */
    using func_t = std::function<void(RefObject *ptr, void *arg)>;
    func_t deleter_;        /**< 删除函数指针 */
    void *arg_;             /**< 删除函数参数 */
    bool empty_;            /**< 是否为空删除器 */

    /**
     * @brief 默认构造函数：空删除器
     * 初始化为空删除器
     */
    explicit Deleter() noexcept : deleter_(nullptr), arg_(nullptr), empty_(true)
    {}

    /**
     * @brief 带参构造函数：绑定释放函数和参数
     * @param func 自定义释放函数
     * @param arg 传递给释放函数的参数
     * 初始化为非空删除器
     */
    Deleter(func_t func, void *arg) : deleter_(func), arg_(arg), empty_(false)
    {}

    /**
     * @brief 调用删除函数释放资源，重载()运算符
     * @param ptr 指向要释放的RefObject对象
     */
    void operator()(RefObject *ptr) const
    {
        if (deleter_) {
            deleter_(ptr, arg_); // 调用自定义删除函数
        } else {
            delete ptr; // 默认删除
            ptr = nullptr; // 避免悬空指针
        }
    }

    // 移动构造
    Deleter(Deleter &&other) noexcept
            : deleter_(std::move(other.deleter_)), arg_(other.arg_), empty_(other.empty_)
    {
        other.deleter_ = nullptr; // 清空源对象的删除器，避免重复释放
        other.arg_ = nullptr;
        other.empty_ = true;
    }

    // 移动赋值
    Deleter &operator=(Deleter &&other) noexcept
    {
        if (this != &other) {
            deleter_ = std::move(other.deleter_);
            arg_ = other.arg_;
            empty_ = other.empty_;
            other.deleter_ = nullptr; // 清空源对象的删除器，避免重复释放
            other.arg_ = nullptr;
            other.empty_ = true;
        }
        return *this;
    }

    // 拷贝构造
    Deleter(const Deleter& other)
    {
        if (this == &other) return; // 防止自赋值
        deleter_ = other.deleter_;
        arg_ = other.arg_;
        empty_ = other.empty_;
    }

    // 拷贝赋值
    Deleter& operator=(const Deleter& other)
    {
        if (this == &other) return *this; // 防止自赋值
        deleter_ = other.deleter_;
        arg_ = other.arg_;
        empty_ = other.empty_;
        return *this;
    }
};

/**
 * @brief 此类的子类必须由智能指针管理
 */
class RefObject
{

public:
    /**
     * @brief 构造函数
     * @param refCount 引用计数指针（可选，默认为nullptr）
     * 默认使用共享模式，引用计数指针指向自身
     */
    explicit RefObject()
            : refCount_(&refCountValue_), refCountValue_(0)
    {
    }

    /** @brief 虚析构函数：允许子类自定义析构逻辑 */
    virtual ~RefObject()
    {}

    /**
     * @brief 判断是否为共享模式
     * @return true 如果引用计数指针指向自身（共享模式），否则 false
     */
    bool IsShared() const
    {
        return refCount_ != &refCountValue_;
    }

    /** @brief 增加引用计数 */
    void AddRef()
    {
        refCount_->fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 减少引用计数并检查是否需要释放对象（原子操作）
     * @return true：计数减为0并释放对象；false：计数未减为0
     */
    virtual bool DecRef()
    {
        if (*(this->refCount_) == 0) {
            return true;  /**< 如果引用计数为0，直接返回false */
        }
        if (--(*refCount_) == 0) {
            if (deleter_.empty_) {
                delete this;  /**< 如果没有自定义删除器，直接删除对象 */
            } else {
                deleter_(this);  /**< 调用删除器释放资源 */
            }
            return true;
        }
        return false;
    }

    /** @brief 返回共享模式下的引用计数 */
    long UseCount() const
    {
        return *refCount_;
    }

    /** @brief 设置自定义删除器 */
    void SetDeleter(const Deleter& d) {
        deleter_ = d;
        deleter_.empty_ = false;  /**< 设置自定义删除器 */
    }

    RefObject(RefObject const&) = delete;
    RefObject& operator=(RefObject const&) = delete;

public:
    atomic_t<long> *refCount_; /**< 引用计数指针(指向自身或者其他对象的引用计数) 共享模式则指向自身 */
    atomic_t<long> refCountValue_; /**< 非共享模式下的本地引用计数（默认初始化1） */
    Deleter deleter_; /**< 自定义删除器（用于资源释放） */
};

/**
 * @brief 共享模式实现类（支持弱引用）
 * 分离强引用和弱引用计数，用于SharedRefObject和WeakPtr
 * 此处使用组合而非继承，避免侵入式设计，无实际作用，仅用于区分
 * 强引用计数通过RefObject的refCount_指针管理，弱引用计数通过RefObjectImpl的weak_成员管理
 */
class RefObjectImpl
{

public:
    friend class SharedRefWrapper; /**< 允许SharedRefObject访问私有成员 */

    /**
     * @brief 构造函数：初始化强/弱计数均为1（初始时被一个SharedRefObject持有）
     */
    explicit RefObjectImpl() noexcept : reference_(0), weak_(0)
    {}

    /** @brief 增加弱引用计数 */
    void AddWeakRef()
    {
        ++weak_;
    }

    /** @brief 返回强引用计数 */
    long GetUseCount() const
    {
        return reference_;
    }

    /**
     * @brief 减少弱引用计数
     * @return true：计数减为0；false：计数未减为0
     */
    bool DecWeakRef()
    {
        if (weak_ == 0) {
            return true; /**< 如果弱引用计数已经为0，直接返回false */
        }
        if (--weak_ == 0) { // fetch_s
            delete this; /**< 弱引用计数减为0时删除对象 */
            return true; /**< 弱引用计数减为0 */
        }
        return false; /**< 弱引用计数未减为0 */
    }

    /**
     * @brief 尝试获取强引用（用于弱指针升级）
     * @return true：成功获取强引用；false：对象已销毁
     */
    bool Lock()
    {
        long count = reference_.load(std::memory_order_relaxed);
        do {
            if (count == 0)
                return false;
        } while (!reference_.compare_exchange_weak(count, count + 1,
                                                   std::memory_order_acq_rel, std::memory_order_relaxed));
        return true;
    }

    RefObjectImpl(RefObjectImpl const &) = delete;
    RefObjectImpl &operator=(RefObjectImpl const &) = delete;

    /** @brief 检查强引用计数是否大于0 */
    bool IsValid() const
    {
        return reference_ > 0;
    }

    /** @brief 返回弱引用计数（对象有效时返回实际值，否则返回0） */
    long GetWeakCount() const {
        if (IsValid()) {
            return weak_; /**< 如果对象有效，返回弱引用计数 */
        }
        return 0; /**< 如果对象无效，返回0 */
    }

private:
    atomic_t<long> reference_; /**< 强引用计数（关联IncursivePtr） */
    atomic_t<long> weak_; /**< 弱引用计数（关联WeakPtr） */
};

/**
 * @brief 共享模式对象包装类，并不是智能指针，而是一个共享对象的引用计数管理类
 * 继承RefObject，使用RefObjectImpl管理引用计数，支持弱引用
 */
class SharedRefWrapper : public RefObject
{
private:
    RefObjectImpl *impl_; /**< 引用计数实现对象指针 */

public:
    /**
     * @brief 构造函数：初始化共享对象引用计数
     * @param impl 引用计数实现对象指针
     */
    explicit SharedRefWrapper(RefObjectImpl *impl) : impl_(impl)
    {
        this->refCount_ = &impl_->reference_; /**< 设置引用计数指针为实现对象的强引用计数 */
    }

    /**
     * @brief 构造函数：创建新的RefObjectImpl实例，进入共享模式
     */
    explicit SharedRefWrapper() : impl_(new RefObjectImpl)
    {
        this->refCount_ = &impl_->reference_; /**< 指向外部强引用计数 */
    }

    /**
     * @brief 重写减少引用计数逻辑（处理弱引用计数释放）
     * @return true：强引用减为0并释放弱引用块；false：强引用未减为0
     */
    bool DecRef() override
    {
        if (!impl_) return false; /**< 如果实现对象为空，直接返回false */
        if (this->refCount_ == nullptr) {
            return false; /**< 如果引用计数为0，直接返回false */
        }
        if (RefObject::DecRef()) { /**< 先减少强引用计数，此处强引用绑定到了 impl_->reference_，源对象被消除 */
            return true;
        }
        return false;
    }

    /** @brief 返回引用计数实现对象指针 */
    RefObjectImpl *getImpl() const
    {
        return impl_;
    }

    /** @brief 析构函数：释放资源 */
    ~SharedRefWrapper() override
    {
        if (impl_) {
            if (impl_->reference_ == 0) {
                if (impl_->weak_ == 0) { // 没有弱引用计数
                    delete impl_; /**< 如果强引用计数和弱引用计数都为0，删除实现对象 */
                }
            }
            impl_ = nullptr; /**< 清空指针 */
        }
    }
};

/**
 * @brief 侵入式共享智能指针（RAII实现）
 * 管理继承自RefObject的对象，自动维护引用计数
 */
template<typename T>
class IncursivePtr
{
public:
    /** @brief 默认构造函数：空指针 */
    IncursivePtr() : ptr_(nullptr)
    {}

    /**
     * @brief 构造函数：从原始指针创建智能指针
     * @param ptr 原始指针（必须是RefObject派生类）
     */
    explicit IncursivePtr(T *ptr) : ptr_(ptr)
    {
        if (ptr_) {
            ptr_->AddRef(); /**< 增加引用计数 */
        }
    }

    /** @brief 析构函数：减少引用计数 */
    ~IncursivePtr()
    {
        if (ptr_) {
            ptr_->DecRef(); /**< 减少引用计数 */
        }
    }

    /**
     * @brief 拷贝构造函数：共享对象，增加引用计数
     * @param other 源智能指针
     */
    IncursivePtr(IncursivePtr const &other) : ptr_(other.ptr_)
    {
        ptr_->AddRef(); /**< 增加引用计数 */
    }

    /**
     * @brief 移动构造函数：转移所有权，不用减少引用计数
     * @param other 源智能指针
     */
    IncursivePtr(IncursivePtr &&other) noexcept : ptr_(other.ptr_)
    {
        other.ptr_ = nullptr; /**< 清空源指针 */
    }

    /**
     * @brief 拷贝赋值运算符：共享对象，增加引用计数
     * @param other 源智能指针
     * @return *this
     */
    IncursivePtr &operator=(IncursivePtr const &other)
    {
        if (this != &other) /**< 防止自赋值 */
        {
            reset(); /**< 先释放当前对象 */
            ptr_ = other.ptr_; /**< 复制源指针 */
            if (ptr_) {
                ptr_->AddRef(); /**< 增加引用计数 */
            }
        }
        return *this;
    }

    /**
     * @brief 移动赋值运算符：转移所有权
     * @param other 源智能指针
     * @return *this
     */
    IncursivePtr &operator=(IncursivePtr &&other) noexcept
    {
        if (this != &other) /**< 防止自赋值 */
        {
            reset(); /**< 先释放当前对象 */
            ptr_ = other.ptr_; /**< 转移所有权 */
            other.ptr_ = nullptr; /**< 清空源指针 */
        }
        return *this;
    }

    /** @brief 释放当前对象并清空指针 */
    void reset()
    {
        if (ptr_) {
            ptr_->DecRef(); /**< 减少引用计数 */
            ptr_ = nullptr; /**< 清空指针 */
        }
    }

    /** @brief 解引用操作：返回对象引用 */
    T &operator*() const
    { return *ptr_; }

    /** @brief 指针操作：返回对象指针 */
    T *operator->() const
    { return ptr_; }

    /** @brief 布尔转换：判断指针是否非空 */
    explicit operator bool() const
    { return !!ptr_; }

    /** @brief 获取裸指针（谨慎使用） */
    T *get() const
    { return ptr_; }

    /** @brief 获取引用计数 */
    [[nodiscard]] long UseCount() const
    {
        if (!ptr_) return 0;
        if (!ptr_->refCount_) return 0;
        return ptr_->refCount_->load();
    }

    /** @brief 判断是否唯一持有对象 */
    bool unique()
    {
        return UseCount() == 1;
    }

private:
    /**
     * @brief 交换两个智能指针的对象
     * @param other 目标智能指针
     */
    void swap(IncursivePtr &other)
    {
        std::swap(ptr_, other.ptr_); /**< 交换底层指针 */
    }

    T *ptr_; /**< 指向管理的对象指针，要求集成自RefObject */
};

/**
 * @brief 自动释放指针（类似unique_ptr）
 * 持有对象但不共享，手动调用release()释放
 */
template<typename T>
class AutoRelease
{
    T *ptr_; /**< 底层裸指针 */

public:
    /**
     * @brief 构造函数：接收裸指针，不增加引用计数（手动管理）
     * @param ptr 待管理的对象指针
     */
    explicit AutoRelease(T *ptr) : ptr_(ptr)
    {}

    /** @brief 释放对象：清空指针，不影响计数 */
    void release()
    {
        ptr_ = nullptr; /**< 放弃所有权 */
    }

    /** @brief 析构函数：释放对象（减少引用计数） */
    ~AutoRelease()
    {
        if (ptr_) {
            ptr_->DecRef(); /**< 离开作用域时释放 */
        }
    }
};

/**
 * @brief 弱指针（观察对象存活状态，不影响生命周期）
 * 支持从弱引用升级为强引用
 */
template<typename T>
class WeakPtr
{
public:
    /** @brief 默认构造函数：空弱指针 */
    WeakPtr() : impl_(nullptr), ptr_(nullptr) {}

    /**
     * @brief 构造函数：从裸指针创建弱指针（仅当对象为共享模式时有效）
     * @param ptr 待观察的对象指针
     */
    explicit WeakPtr(T * ptr) : impl_(nullptr), ptr_(nullptr) {
        reset(ptr); /**< 尝试关联对象 */
    }

    /**
     * @brief 构造函数：从IncursivePtr创建弱指针
     * @param iptr 强智能指针
     */
    explicit WeakPtr(IncursivePtr<T> const& iptr) : impl_(nullptr), ptr_(nullptr) {
        T* ptr = iptr.get();
        reset(ptr); /**< 关联强指针持有的对象 */
    }

    /**
     * @brief 赋值函数：从IncursivePtr创建弱指针
     * @param iptr 强智能指针
     * @return *this
     */
    WeakPtr& operator=(IncursivePtr<T> const& iptr) {
        reset(iptr.get()); /**< 关联强指针持有的对象 */
        return *this;
    }

    /**
     * @brief 拷贝构造函数：共享弱引用计数
     * @param other 源弱指针
     */
    WeakPtr(WeakPtr const& other) : impl_(other.impl_), ptr_(other.ptr_) {
        if (impl_) impl_->AddWeakRef(); /**< 增加弱计数 */
    }

    /**
     * @brief 移动构造函数：转移弱引用所有权
     * @param other 源弱指针（将被移动）
     */
    WeakPtr(WeakPtr && other) : impl_(nullptr), ptr_(nullptr) {
        swap(other); /**< 交换内部状态 */
    }

    /**
     * @brief 移动赋值运算符：转移弱引用所有权
     * @param other 源弱指针（将被移动）
     * @return *this
     */
    WeakPtr& operator=(WeakPtr && other) {
        swap(other); /**< 交换内部状态 */
        return *this;
    }

    /**
     * @brief 拷贝赋值运算符：释放原有引用，获取新引用
     * @param other 源弱指针
     * @return *this
     */
    WeakPtr& operator=(WeakPtr const& other) {
        if (this == &other) return *this;
        reset(); /**< 释放当前弱引用 */
        if (other.impl_) {
            impl_ = other.impl_;
            ptr_ = other.ptr_;
            impl_->AddWeakRef(); /**< 增加弱引用计数 */
        }
        return *this;
    }

    /** @brief 获取弱引用计数 */
    long UseCount() const {
        if (!impl_) return 0; /**< 如果没有Impl，返回0 */
        if (impl_->GetUseCount() == 0) {
            return 0; /**< 如果强引用计数为0，返回0 */
        }
        return impl_->GetWeakCount(); /**< 返回强引用计数 */
    }

    /** @brief 析构函数：释放弱引用计数 */
    ~WeakPtr() {
        reset(); /**< 减少弱计数并释放Impl（若需要） */
    }

    /**
     * @brief 交换两个弱指针的状态
     * @param other 目标弱指针
     */
    void swap(WeakPtr & other) {
        std::swap(impl_, other.impl_);
        std::swap(ptr_, other.ptr_);
    }

    /** @brief 清空弱指针引用 */
    void reset() {
        if (impl_) {
            impl_->DecWeakRef();
            impl_ = nullptr;
            ptr_ = nullptr;
        }
    }

    /**
     * @brief 关联新的对象指针
     * @param ptr 新的对象指针
     */
    void reset(T*ptr)
    {
        if (impl_)
        {
            impl_->DecWeakRef(); /**< 减少弱引用计数 */
            impl_ = nullptr;
            ptr_ = nullptr;
        }

        if (!ptr) {
            return;
        }
        if (!ptr->IsShared()) {
            return;
        }

        ptr_ = ptr; /**< 更新指针 */
        impl_ = reinterpret_cast<SharedRefWrapper*>(ptr_)->getImpl();
        impl_->AddWeakRef(); /**< 增加弱引用计数 */
    }

    /**
     * @brief 尝试将弱引用升级为强引用
     * @return 有效的IncursivePtr（对象存活时）或空指针（对象已销毁）
     */
    IncursivePtr<T> Lock() const {
        if (!impl_) return IncursivePtr<T>(); /**< 无Impl说明对象非共享或已销毁 */
        RefObjectImpl* localImpl = impl_;
        if (!localImpl->Lock())  return IncursivePtr<T>(); /**< 如果强引用计数为1，说明对象已销毁 */
        IncursivePtr<T> iptr(ptr_); /**< 创建强指针，注意，会导致强引用+1 */
        ptr_->DecRef(); /**< 平衡强指针构造时的计数增加，impl_->Lock()成功后，强引用计数增加了1 */
        return iptr;
    }

    /**
     * @brief 布尔转换：判断弱指针是否有效（关联到共享对象）
     * @return true：有效（关联到共享对象）；false：无效
     */
    explicit operator bool() const {
        return impl_ != nullptr && impl_->IsValid(); /**< 检查Impl是否有效且强引用计数大于0 */
    }

private:
    RefObjectImpl *impl_; /**< 引用计数实现对象指针 */
    T* ptr_; /**< 弱指针指向的对象指针 */
};

/**
 * @brief 辅助函数：从裸指针创建std::shared_ptr（桥接非侵入式接口）
 * @tparam T 对象类型（必须继承自RefObject）
 * @param ptr 裸对象指针
 * @return std::shared_ptr<T> 包装后的智能指针
 */
template <typename T>
typename std::enable_if<std::is_base_of<RefObject, T>::value, std::shared_ptr<T>>::type
SharedWrapper(T *ptr)
{
    if (!ptr) {
        return std::shared_ptr<T>(); /**< 空指针返回空shared_ptr */
    }
    ptr->AddRef(); /**< 增加引用计数，与shared_ptr共享 */
    auto deleter = [ptr](T *p) {
        if (p) {
            p->DecRef(); /**< 减少引用计数 */
        }
    };
    return std::shared_ptr<T>(ptr, deleter); /**< 返回std::shared_ptr，使用自定义删除器 */
}

/**
 * @brief 辅助函数：创建共享对象（类似make_shared）
 * @tparam T 对象类型（必须继承自RefObject）
 * @tparam Args 构造函数参数类型
 * @param args 构造函数参数
 * @return std::shared_ptr<T> 新创建的共享对象指针
 */
template <typename T, typename... Args>
typename std::enable_if<std::is_base_of<RefObject, T>::value, std::shared_ptr<T>>::type
MakeSharedWrapper(Args&&... args)
{
    auto ptr = new T(std::forward<Args>(args)...); /**< 创建对象 */
    ptr->AddRef(); /**< 增加引用计数 */
    auto deleter = [](T *p)
    {
        if (p) {
            p->DecRef(); /**< 减少引用计数 */
        }
    };
    return std::shared_ptr<T>(ptr, deleter); /**< 返回std::shared_ptr，使用自定义删除器 */
}

/**
 * @brief 辅助函数：统一增加引用计数接口（支持模板特化）
 * @tparam T 对象类型
 * @param ptr 对象指针（对于RefObject子类调用AddRef，否则不处理）
 */
template <typename T>
typename std::enable_if<std::is_base_of<RefObject, T>::value, void>::type
AddRef(T *ptr)
{
    if (ptr) {
        ptr->AddRef(); /**< 增加引用计数 */
    }
}
template <typename T>
typename std::enable_if<!std::is_base_of<RefObject, T>::value>::type
DecrementRef(T * ptr)
{
}

/**
 * @brief 辅助函数：统一减少引用计数接口（支持模板特化）
 * @tparam T 对象类型
 * @param ptr 对象指针（对于RefObject子类调用DecRef，否则不处理）
 */
template <typename T>
typename std::enable_if<std::is_base_of<RefObject, T>::value, void>::type
DecRef(T *ptr)
{
    if (ptr) {
        ptr->DecRef(); /**< 减少引用计数 */
    }
}

/**
 * @brief 引用计数作用域保护类
 * 在作用域内强制保持对象引用计数（防止被释放，增加一次引用计数）
 */
class ScopeRefGuard
{
public:
    /**
     * @brief 构造函数：增加引用计数
     * @param ptr 待保护的对象指针（必须是RefObject子类）
     */
    explicit ScopeRefGuard(RefObject *ptr) : ptr_(ptr)
    {
        if (ptr_) {
            ptr_->AddRef(); /**< 增加引用计数 */
        }
    }

    /** @brief 析构函数：减少引用计数 */
    ~ScopeRefGuard()
    {
        if (ptr_) {
            ptr_->DecRef(); /**< 减少引用计数 */
        }
    }

    /** @brief 获取受保护的对象指针 */
    RefObject *get() const
    {
        return ptr_; /**< 返回受保护的对象指针 */
    }

private:
    RefObject *ptr_; /**< 待保护的对象指针（必须是RefObject子类） */
};

/**
 * @brief 对象计数器模板类（用于统计对象实例数量，调试用）
 */
template <typename T>
struct ObjectCounter
{
    /** @brief 构造函数：增加计数器 */
    ObjectCounter() { ++counter(); }

    /** @brief 拷贝构造函数：增加计数器（模拟对象复制时的计数变化） */
    ObjectCounter(ObjectCounter const&) { ++counter(); }

    /** @brief 移动构造函数：不增加计数器（对象所有权转移） */
    ObjectCounter(ObjectCounter &&) { ++counter(); }

    /** @brief 析构函数：减少计数器 */
    ~ObjectCounter() { --counter(); }

    /** @brief 获取当前对象数量 */
    static long getCount() {
        return counter();
    }

private:
    /** @brief 静态原子计数器（每个模板特化类型独立） */
    static atomic_t<long>& counter() {
        static atomic_t<long> c;
        return c;
    }
};

/**
 * @brief ID生成器模板类（为每个对象分配唯一ID，调试用）
 */
template <typename T>
struct IdCounter
{
    /** @brief 构造函数：分配唯一ID（原子递增） */
    IdCounter() { id_ = ++counter(); }

    /** @brief 拷贝构造函数：分配新ID（模拟对象复制时的新实例） */
    IdCounter(IdCounter const&) { id_ = ++counter(); }

    /** @brief 移动构造函数：分配新ID（模拟对象移动时的新实例） */
    IdCounter(IdCounter &&) { id_ = ++counter(); }

    /** @brief 获取对象ID */
    long getId() const {
        return id_;
    }

private:
    /** @brief 静态原子计数器（每个模板特化类型独立） */
    static atomic_t<long>& counter() {
        static atomic_t<long> c;
        return c;
    }

    long id_; /**< 对象唯一ID */
};

/**
 * @brief 辅助结构体：记录代码位置（用于日志或调试）
 */
struct SourceLocation
{
    const char* file_ = nullptr; /**< 文件名 */
    int lineno_ = 0; /**< 行号 */

    /**
     * @brief 初始化位置信息
     * @param file 文件名
     * @param lineno 行号
     */
    void Init(const char* file, int lineno)
    {
        file_ = file;
        lineno_ = lineno;
    }

    /**
     * @brief 比较运算符（用于排序或哈希）
     * @param lhs 左操作数
     * @param rhs 右操作数
     * @return true：lhs小于rhs；false：否则
     */
    friend bool operator<(SourceLocation const& lhs, SourceLocation const& rhs)
    {
        if (lhs.lineno_ != rhs.lineno_)
            return lhs.lineno_ < rhs.lineno_;

        if (lhs.file_ == rhs.file_) return false;

        if (lhs.file_ == nullptr)
            return true;

        if (rhs.file_ == nullptr)
            return false;

        return strcmp(lhs.file_, rhs.file_) == -1;
    }

    /**
     * @brief 转换为字符串（用于日志输出）
     * @return 格式化后的位置字符串
     */
    std::string ToString() const
    {
        std::string s("{file:");
        if (file_) s += file_;
        s += ", line:";
        s += std::to_string(lineno_) + "}";
        return s;
    }
};

} // namespace cxk

#endif // GOCOROUTINE_SMART_PTR_H