//
// Created by cxk_zjq on 25-5-31.
//

#ifndef GOCOROUTINE_TYPES_H
#define GOCOROUTINE_TYPES_H
#include <atomic>

template <typename T>
using atomic_t = std::atomic<T>;

// 性能低于直接使用函数指针的方式
//#include <functional>
//using stack_malloc_fn_t = std::function<void*(size_t)>;
//using stack_free_fn_t = std::function<void(void*)>;

typedef void*(*stack_malloc_fn_t)(std::size_t size);
typedef void(*stack_free_fn_t)(void *ptr);

#endif //GOCOROUTINE_TYPES_H
