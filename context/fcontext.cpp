//
// Created by cxk_zjq on 25-6-1.
//

#include "fcontext.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <spdlog/spdlog.h>

#define PAGE_SIZE 0x1000 // 4096 bytes, typical page size on Linux

stack_malloc_fn_t &cxk::StackTraits::MallocFunc()
{
    static stack_malloc_fn_t fn = &::std::malloc;
    return fn;
}

stack_free_fn_t &cxk::StackTraits::FreeFunc()
{
    static stack_free_fn_t fn = &::std::free;
    return fn;
}

int &cxk::StackTraits::GetProtectStackPageSize()
{
    static int size = 0;
    return size;
}

/*
 * 在 Linux x86_64 架构下，栈默认是向下增长的（栈顶地址 < 栈底地址，地址从高到低延伸）
 */

bool cxk::StackTraits::ProtectStack(void *stack, std::size_t size, int pageNum)
{
    if (pageNum==0){
        return false; // 如果页大小为0，直接返回false，不进行保护
    }
    if ((int)size <= (pageNum+1)*getpagesize()) {   // +1确保栈分配合理，至少有一页
        return true; // 如果栈大小小于等于页大小，直接返回true，不进行保护
    }

//    // 计算栈顶地址（低地址端）
//    void* stack_top = (char*)stack - size; // 假设栈向下增长，栈顶 = 栈底 - size
//
//    // 计算保护页起始地址（栈顶下方，页对齐）
//    void* protect_start = (void*)(((uintptr_t)stack_top & ~(uintptr_t)(pageSize - 1)) - pageSize * pageNum);
//    if (protect_start < nullptr) {
//        return false;
//    }

    // 计算保护页的起始地址（页对齐）
    // Linux 要求mprotect的地址必须是页对齐的
    void *protect_page_addr = ((std::size_t)stack & (~PAGE_SIZE)) ?  // 0xfff是4096的掩码，目标是向上取整对齐，此处用于判断低位是否有大小
                              (void*)(((std::size_t)stack & ~(std::size_t)(~PAGE_SIZE)) + PAGE_SIZE) : // 非对齐地址：调整到下一页边界，0x1000是一页大小
                              stack; // 对齐地址：直接使用

/*#define PROT_READ	0x1		*//* Page can be read.  *//*
#define PROT_WRITE	0x2		*//* Page can be written.  *//*
#define PROT_EXEC	0x4		*//* Page can be executed.  *//*
#define PROT_NONE	0x0		*//* Page can not be accessed.  */
    if (mprotect(protect_page_addr, pageNum * PAGE_SIZE, PROT_NONE) == -1) {
        spdlog::error("Failed to protect stack at {}: {}", protect_page_addr, strerror(errno));
        return false; // 如果保护失败，返回false
    }else {
        spdlog::info("Protected stack at {} with {} pages", protect_page_addr, pageNum);
        return true; // 如果保护成功，返回true
    }
    return false;
}

void cxk::StackTraits::UnprotectStack(void *stack, int pageNum)
{
    if (pageNum == 0) {
        return; // 如果页大小为0，直接返回，不进行解除保护
    }

    // 计算保护页的起始地址（页对齐）
    void *protect_page_addr = ((std::size_t)stack & (~PAGE_SIZE)) ?
                              (void*)(((std::size_t)stack & ~(std::size_t)(~PAGE_SIZE)) + PAGE_SIZE) :
                              stack; // 对齐地址：直接使用

    if (mprotect(protect_page_addr, pageNum * PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
        spdlog::error("Failed to unprotect stack at {}: {}", protect_page_addr, strerror(errno));
    } else {
        spdlog::info("Unprotected stack at {} with {} pages", protect_page_addr, pageNum);
    }

}
