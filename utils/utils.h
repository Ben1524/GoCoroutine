///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef UTILS_H
#define UTILS_H
#include <atomic>

# define ALWAYS_INLINE __attribute__ ((always_inline)) inline  /// 强制内联宏定义
template <typename T>
using atomic_t = std::atomic<T>;

#endif ///UTILS_H
