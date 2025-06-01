//
// Created by cxk_zjq on 25-6-1.
//

#ifndef GOCOROUTINE_CHANNEL_H
#define GOCOROUTINE_CHANNEL_H

#include <deque>
#include "debug.h"

namespace cxk
{

/* * @brief 通道（Channel）类
 * 用于协程间通信和同步，支持多生产者多消费者模式。
 * 提供阻塞和非阻塞的发送和接收操作。
 */

/*
 * ChannelImpl是Channel的实现类，提供了具体的发送和接收逻辑。
 */
template<
        typename T,
        typename QueueT
>
class ChannelImpl;

template<
        typename T,
        typename QueueT = std::deque<T>
>
class Channel;

/*
 * @brief 基于信号的Channel实现类，兼容stl风格的api实现
 * 继承自DebuggerId以支持调试和日志记录。
 */
template<typename T>
class ChannelImplWithSignal : public DebuggerId<ChannelImplWithSignal<int>>
{

};

}

#endif //GOCOROUTINE_CHANNEL_H
