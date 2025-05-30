///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef ANYS_H
#define ANYS_H
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>
#include <../concurrence/spinlock.h>
namespace cxk {
/**
 * @brief 类型容器管理类（用于协程/线程本地存储）
 * @tparam Group 类型分组标识（用于区分不同存储组）
 *
 * 该类实现了一个类型安全的容器，用于存储不同类型的对象实例，
 * 支持在内存中连续分配空间并管理对象的生命周期，适用于需要高效存储多种类型的场景。
 */
template <typename Group>
class Anys
{
public:
    /// 通过类型擦除绑定通用的构造函数和析构函数 将具体类型的信息封装在一个通用接口后面，只暴露统一的操作方式。
    /// 定义函数指针类型，参数为void*，返回void*类型
    using Constructor = void (*)(void* object);
    using Destructor = void (*)(void* object);

    /**
     * @brief 默认构造/析构函数（针对类型T）
     * 提供默认的placement new构造和显式析构实现
     */
    template <typename T>
    struct DefaultConstructorDestructor
    {
        /// 构造函数：使用placement new在指定内存位置创建对象
        static void Constructor(void *ptr) {
            new (reinterpret_cast<T*>(ptr)) T();  /// 在指定内存位置构造对象 new (内存地址) 构造函数
        }
        /// 析构函数：显式调用对象析构函数
        static void Destructor(void *ptr) {
            reinterpret_cast<T*>(ptr)->~T();
        }
    };

    /**
     * @brief 注册类型T（使用默认构造/析构函数）
     * @return 类型对应的索引（用于后续访问）
     * @note 必须在创建第一个实例前完成注册，建议在全局初始化阶段调用
     */
    template <typename T>
    static std::size_t Register()
    {
        return Register<T>(&DefaultConstructorDestructor<T>::Constructor,
                          &DefaultConstructorDestructor<T>::Destructor);
    }

    /**
     * @brief 注册自定义构造/析构函数的类型T
     * @param constructor 自定义构造函数
     * @param destructor 自定义析构函数
     * @return 类型对应的索引
     * @throw logic_error 若注册发生在实例创建之后
     */
    template <typename T>  /// 由于是静态函数，会出现构造发生在类初始化完成前,使用自旋锁保证注册在构造之前
    static std::size_t Register(Constructor constructor, Destructor destructor)
    {
        std::unique_lock<std::mutex> lock(GetMutex()); /// 线程安全的注册操作
        std::unique_lock<LFLock> inited(GetInitGuard(), std::defer_lock);
        if (!inited.try_lock())
        {
            throw std::logic_error("Anys::Register must be called before any instance is created");
        }
        /// 获取全局类型元数据列表
        /// 记录类型元数据
        KeyInfo info;
        info.align = std::alignment_of<T>::value;       /// 类型对齐要求
        info.size = sizeof(T);                         /// 类型大小
        info.offset = StorageLen();                    /// 内存偏移量，理论上的偏移
        info.constructor = constructor;                /// 自定义构造函数
        info.destructor = destructor;                  /// 自定义析构函数
        GetKeys().push_back(info);

        /// 更新全局存储总长度
        /// 类型至少占用一个字节，确保加上填充可以使得向下区中为align的倍数
        StorageLen() += (info.size + info.align - 1) & ~(info.align - 1);
        Size()++;
        return GetKeys().size() - 1; /// 返回新注册类型的索引
    }

    /**
     * @brief 获取指定索引的类型实例（安全访问）
     * @tparam T 目标类型
     * @param index 类型索引
     * @return 类型T的引用
     * @throw logic_error 索引越界或类型不匹配
     */
    template <typename T>
    ALWAYS_INLINE T& get(std::size_t index)
    {
        if (index >= *(std::size_t*)hold_) /// 检查索引有效性
            throw std::logic_error("Anys::get overflow");
        /// 开始校验
        const auto& keyInfo = GetKeys()[index];
        if (keyInfo.size != sizeof(T)) /// 检查类型大小是否匹配
            throw std::logic_error("Anys::get type mismatch");
        if (keyInfo.align != std::alignment_of<T>::value) /// 检查对齐要求是否匹配
            throw std::logic_error("Anys::get alignment mismatch");
        if (index >= Size()) /// 检查索引是否越界
            throw std::logic_error("Anys::get index out of range");

        /// 计算内存地址并返回引用
        char *p = storage_ + offsets_[index];
        return *reinterpret_cast<T*>(p);
    }

private:
    /// 类型元数据结构体（存储类型大小、对齐、构造/析构函数等）
    struct KeyInfo
    {
        int align;          /// 内存对齐字节数,数据的起始地址是该变量值的整数倍
        int size;           /// 类型大小（字节）
        std::size_t offset; /// 在storage_中的偏移量
        Constructor constructor; /// 构造函数指针
        Destructor destructor; /// 析构函数指针
    };

    /// 获取全局类型元数据列表（静态成员，跨实例共享）
    static std::vector<KeyInfo> & GetKeys()
    {
        static std::vector<KeyInfo> obj;
        return obj;
    }

    /// 获取全局互斥锁（用于注册阶段的线程安全）
    inline static std::mutex & GetMutex()
    {
        static std::mutex obj;
        return obj;
    }

    /// 获取全局存储总长度（静态成员，记录内存需求）
    static std::size_t & StorageLen()
    {
        static std::size_t obj = 0;
        return obj;
    }

    /// 获取全局类型数量（静态成员，记录注册的类型总数）
    static std::size_t & Size()
    {
        static std::size_t obj = 0;
        return obj;
    }

    /// 获取全局初始化自旋锁（用于保证注册顺序）
    static LFLock & GetInitGuard()
    {
        static LFLock obj;
        return obj;
    }
    /**
      * @brief 初始化所有注册类型的对象
      * 调用注册时提供的构造函数
      */
    void Init()
    {
        if (!Size()) return;
        for (std::size_t i = 0; i < Size(); i++)
        {
            const auto& keyInfo = GetKeys()[i];
            if (!keyInfo.constructor) continue; /// 跳过无构造函数的类型

            char *p = storage_ + offsets_[i];
            keyInfo.constructor(p); /// 调用构造函数
        }
    }
    /**
     * @brief 销毁所有注册类型的对象
     * 调用注册时提供的析构函数
     */
    void Deinit()
    {
        if (!Size()) return;
        for (std::size_t i = 0; i < Size(); i++)
        {
            const auto& keyInfo = GetKeys()[i];
            if (!keyInfo.destructor) continue; /// 跳过无析构函数的类型

            char *p = storage_ + offsets_[i];
            keyInfo.destructor(p); /// 调用析构函数
        }
    }

public:


    Anys():hold_(nullptr),offsets_(nullptr),storage_(nullptr)
    {
        GetInitGuard().try_lock(); /// 确保初始时互斥(数量可能会变)
        if (!Size())
        {
            return; /// 如果没有注册类型，则不需要分配内存
        }
        const std::size_t header_size = sizeof(std::size_t) * (1+Size()); /// 计算头部大小类型数量头 + 偏移量数组
        try {
            hold_ = new char[header_size + StorageLen()]; /// 分配连续内存
        } catch (const std::bad_alloc&) {
            /// 再次尝试
            hold_ = new char[header_size + StorageLen()];
            if (!hold_) {
                throw std::bad_alloc(); /// 如果分配失败，抛出异常
            }
        }catch (const std::exception& e) {
            throw std::runtime_error(std::string("Anys allocation failed: ") + e.what());
        }
        storage_ = hold_ + header_size; /// 数据区起始位置，注意：并不强制要求连续存放，此处的指针只是一个相对位置，方便寻址
        *(std::size_t*)hold_ = Size();   /// 存储类型数量,占据一个size_t的大小
        offsets_ = (std::size_t*)(hold_ + sizeof(std::size_t)); /// 偏移量数组起始位置
        auto & keys = GetKeys();
        /// 计算各类型的内存偏移（考虑内存对齐）
        for (std::size_t i = 0; i < Size(); i++) {
            const auto& keyInfo = GetKeys()[i];
            std::size_t offset = keyInfo.offset;
            const std::size_t align = keyInfo.align;
            /// 执行内存对齐（模仿GCC的std::align实现）
            /// alignedOffset = ((offset + align - 1) / align) * align;
            std::size_t alignedOffset = (offset + align - 1) & ~(align - 1); /// 向上取整，清零余数(模运算)
            /// 更新实际偏移量并记录到offsets_数组
            offsets_[i] = alignedOffset;
        }
        Init(); /// 调用构造函数初始化所有对象
    }
    /**
    * @brief 析构函数（销毁所有对象并释放内存）
    */
    ~Anys()
    {
        Deinit(); /// 调用析构函数销毁所有对象
        if (hold_) {
            free(hold_);
            hold_ = nullptr;
            offsets_ = nullptr;
            storage_ = nullptr;
        }
    }
private:
    /// 内存布局：
    /// hold_ 指向一块连续内存，布局为：
    /// [size_t(类型数量)] [offsets_数组] [storage_数据区]
    char* hold_;           /// 内存块起始指针
    std::size_t* offsets_; /// 各类型数据的偏移量数组
    char* storage_;        /// 数据存储区
};

} /// cxk

#endif ///ANYS_H
