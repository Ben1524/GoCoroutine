//
// Created by cxk_zjq on 25-6-1.
//

#ifndef GOCOROUTINE_CONTEXT_H
#define GOCOROUTINE_CONTEXT_H
#include <utils/utils.h>
#include "fcontext.h"

namespace cxk
{

class Context
{
public:
    explicit Context(fn_t fn, intptr_t vp, uint32_t stackSize = 0)
        : fn_(fn), vp_(vp), stackSize_(stackSize)
    {
        stack_ = static_cast<char*>(StackTraits::MallocFunc()(stackSize_));
        // 创建协程上下文（栈顶位于高地址，栈向下增长）返回低地址内容
        ctx_ = libgo_make_fcontext(stack_ + stackSize_, stackSize_, fn_);
        int protectPage = StackTraits::GetProtectStackPageSize();
        if (protectPage>0) {  // 如果保护页大小大于0，则保护栈
            StackTraits::ProtectStack(stack_, stackSize_, protectPage); // 使用时从高地址向低地址增长，保护栈的前protectPage个字节
            protectPage_ = protectPage;
        }
    }
    ~Context() {
        if (stack_) {
            // 1. 取消栈保护
            if (protectPage_) {
                StackTraits::UnprotectStack(stack_, protectPage_);
            }
            // 2. 释放栈内存
            StackTraits::FreeFunc()(stack_);
            stack_ = nullptr;
        }
    }

    ALWAYS_INLINE void SwapIn() {
        libgo_jump_fcontext(&GetTlsContext(), ctx_, vp_);
    }

    ALWAYS_INLINE void SwapTo(Context & other) {
        libgo_jump_fcontext(&ctx_, other.ctx_, other.vp_);
    }

    ALWAYS_INLINE void SwapOut() {
        libgo_jump_fcontext(&ctx_, GetTlsContext(), 0);
    }

    fcontext_t& GetTlsContext() {
        static thread_local fcontext_t tls_context;
        return tls_context;
    }

private:
    fcontext_t ctx_;
    fn_t fn_;
    intptr_t vp_;
    char *stack_ = nullptr;
    uint32_t stackSize_ = 0;
    int protectPage_ = 0;
};

}

#endif //GOCOROUTINE_CONTEXT_H
