//
// Created by cxk_zjq on 25-5-29.
//

#include "error.h"
#include "error.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace cxk
{

// 定义协程专用日志器（建议在项目初始化时创建）
static std::shared_ptr<spdlog::logger> g_co_logger = []() {
    auto logger = spdlog::stdout_color_mt("co_logger");
    logger->set_level(spdlog::level::level_enum::debug); // 设置日志级别
    return logger;
}();


const char* co_error_category::name() const throw()
{
    return "coroutine_error";
}

std::string co_error_category::message(int v) const
{
    switch (v) {
        case (int)eCoErrorCode::ec_ok:
            return "ok";

        case (int)eCoErrorCode::ec_mutex_double_unlock:
            return "co_mutex double unlock";

        case (int)eCoErrorCode::ec_block_object_locked:
            return "block object locked when destructor";

        case (int)eCoErrorCode::ec_block_object_waiting:
            return "block object was waiting when destructor";

        case (int)eCoErrorCode::ec_yield_failed:
            return "yield failed";

        case (int)eCoErrorCode::ec_swapcontext_failed:
            return "swapcontext failed";

		case (int)eCoErrorCode::ec_makecontext_failed:
			return "makecontext failed";

        case (int)eCoErrorCode::ec_iocpinit_failed:
            return "iocp init failed";

        case (int)eCoErrorCode::ec_protect_stack_failed:
            return "protect stack failed";

        case (int)eCoErrorCode::ec_std_thread_link_error:
            return "std thread link error.\n"
                "if static-link use flags: '-Wl,--whole-archive -lpthread -Wl,--no-whole-archive -static' on link step;\n"
                "if dynamic-link use flags: '-pthread' on compile step and link step;\n";

        case (int)eCoErrorCode::ec_disabled_multi_thread:
            return "Unsupport multiply threads. If you want use multiply threads, please cmake libgo without DISABLE_MULTI_THREAD option.";
    }

    return "";
}

const std::error_category& GetCoErrorCategory()
{
    static co_error_category obj;
    return obj;
}
std::error_code MakeCoErrorCode(eCoErrorCode code)
{
    return std::error_code((int)code, GetCoErrorCategory());
}
void ThrowError(eCoErrorCode code)
{
    auto message = GetCoErrorCategory().message((int)code);
    g_co_logger->error("throw exception {}:{}", (int)code, message);
    if (std::uncaught_exception()) return ;
    throw std::system_error(MakeCoErrorCode(code));
}

co_exception::co_exception() {}
co_exception::co_exception(std::string const& errMsg)
    : errMsg_(errMsg)
{
}

void ThrowException(std::string const& errMsg)
{
    g_co_logger->error("throw co_exception: {}", errMsg);
    if (std::uncaught_exception()) return ;
    throw co_exception(errMsg);
}

} //namespace co
