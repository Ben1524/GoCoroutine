//
// Created by cxk_zjq on 25-5-29.
//

#include <gtest/gtest.h>
#include <stdexcept>
#include <common/error.h>

using namespace cxk;

// 测试错误类别名称
TEST(CoErrorCategory, Name) {
    const auto& category = GetCoErrorCategory();
    EXPECT_STREQ(category.name(), "coroutine_error");
}

// 测试错误码到消息的映射
TEST(CoErrorCategory, MessageMapping) {
    const auto& category = GetCoErrorCategory();

    EXPECT_EQ(category.message((int)eCoErrorCode::ec_ok), "ok");
    EXPECT_EQ(category.message((int)eCoErrorCode::ec_mutex_double_unlock),
              "co_mutex double unlock");
    EXPECT_EQ(category.message((int)eCoErrorCode::ec_yield_failed),
              "yield failed");
    EXPECT_EQ(category.message((int)eCoErrorCode::ec_std_thread_link_error),
              "std thread link error.\n"
              "if static-link use flags: '-Wl,--whole-archive -lpthread -Wl,--no-whole-archive -static' on link step;\n"
              "if dynamic-link use flags: '-pthread' on compile step and link step;\n");

    // 测试未知错误码返回空字符串
    EXPECT_EQ(category.message(9999), "");
}

// 测试错误码创建
TEST(CoErrorCategory, MakeErrorCode) {
    auto ec = MakeCoErrorCode(eCoErrorCode::ec_swapcontext_failed);

    EXPECT_EQ(ec.value(), (int)eCoErrorCode::ec_swapcontext_failed);
    EXPECT_STREQ(ec.category().name(), "coroutine_error");
//    EXPECT_STREQ(ec.message(), "swapcontext failed");
}

// 测试ThrowError抛出正确的异常
TEST(CoErrorCategory, ThrowError) {
    EXPECT_THROW({
        ThrowError(eCoErrorCode::ec_block_object_locked);
    }, std::system_error);

    try {
        ThrowError(eCoErrorCode::ec_iocpinit_failed);
    } catch (const std::system_error& e) {
        EXPECT_EQ(e.code().value(), (int)eCoErrorCode::ec_iocpinit_failed);
        EXPECT_STREQ(e.code().category().name(), "coroutine_error");
        EXPECT_STREQ(e.what(), "iocp init failed");
    }
}

// 测试嵌套异常时不抛出新异常
TEST(CoErrorCategory, NestedException) {
    try {
        try {
            throw std::runtime_error("inner exception");
        } catch (...) {
            // 在已有异常的情况下调用ThrowError
            ThrowError(eCoErrorCode::ec_protect_stack_failed);
            FAIL() << "Expected no exception to be thrown";
        }
    } catch (...) {
        // 应该捕获到原始的runtime_error，而不是system_error
        SUCCEED();
    }
}

// 测试co_exception异常
TEST(CoErrorCategory, CoException) {
    EXPECT_THROW({
        ThrowException("test exception message");
    }, co_exception);

    try {
        ThrowException("custom exception");
    } catch (const co_exception& e) {
        EXPECT_STREQ(e.what(), "custom exception");
    }
}
