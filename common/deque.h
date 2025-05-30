///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef DEQUE_H
#define DEQUE_H

#pragma once

#include <deque>

namespace cxk
{
    template <typename T, typename Alloc = std::allocator<T>>
    using Deque = std::deque<T, Alloc>;

    /// TODO: 实现多读一写线程安全的deque

}


#endif ///DEQUE_H
