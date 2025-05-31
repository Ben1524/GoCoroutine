cmake_minimum_required(VERSION 3.15)  # 使用FetchContent需要3.11+，建议3.15+
project(GoCoroutine LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 优化编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-rdynamic -O0 -ggdb -Wall -Wextra -Wpedantic -Wno-deprecated -Wno-unused-function -Wno-builtin-macro-redefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wno-deprecated -Wno-unused-function -Wno-builtin-macro-redefined")

# 选项控制
option(BUILD_TESTS "Build tests" ON)
option(USE_EXTERNAL_GTEST "Use external GTest instead of FetchContent" OFF)
option(USE_SANITIZERS "Enable sanitizers for debugging" OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 配置全局头文件路径（使用INTERFACE目标）
add_library(project_headers INTERFACE)
target_include_directories(project_headers
        INTERFACE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/utils
        ${CMAKE_SOURCE_DIR}/concurrence
)

find_package(unofficial-concurrentqueue CONFIG REQUIRED)
# 依赖项配置
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)


# GTest配置
if(USE_EXTERNAL_GTEST)
    find_package(GTest REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https:///github.com/google/googletest.git
            GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# 启用测试
if(BUILD_TESTS)
    enable_testing()
    include(GoogleTest)
endif()

# 将核心功能作为库构建
add_library(gocoroutine_lib STATIC
        common/anys.h
        utils/utils.h
        concurrence/spinlock.h
        common/clock.h
        utils/macro.h
        common/deque.h
        common/error.cpp
        common/error.h
        common/lock_free_ring_queue.h
        common/thread_safe_queue.h
        common/smart_ptr.h
        context/context.h
        utils/types.h
        context/fcontext.cpp
        context/fcontext.h
)

target_link_libraries(gocoroutine_lib
        PUBLIC
        project_headers
        Threads::Threads
        spdlog::spdlog
        unofficial::concurrentqueue::concurrentqueue
)

# 主可执行文件配置
add_executable(GoCoroutine
        main.cpp
)

target_link_libraries(GoCoroutine
        PRIVATE
        gocoroutine_lib
)

# 测试配置
if(BUILD_TESTS)
    # 定义测试源文件列表
    set(TEST_SOURCES
            test/test_anys.cpp
            test/test_clock.cpp
            test/test_error.cpp
            test/test_lfrqueue.cpp
            test/test_smartptr.cpp
            test/test_tsqueue.cpp
    )

    # 为每个测试文件创建单独的测试目标
    foreach(test_source ${TEST_SOURCES})
        # 获取不带路径和扩展名的文件名
        get_filename_component(test_name ${test_source} NAME_WE)

        # 创建测试可执行文件
        add_executable(${test_name} ${test_source}

        )

        # 链接必要的库
        target_link_libraries(${test_name}
                PRIVATE
                gocoroutine_lib
                GTest::gtest
                GTest::gtest_main
                GTest::gmock
                GTest::gmock_main
                unofficial::concurrentqueue::concurrentqueue
        )

        # 将测试添加到CTest
        gtest_discover_tests(${test_name})

        # 添加编译选项
        if(USE_SANITIZERS)
            target_compile_options(${test_name} PRIVATE -fsanitize=address -fsanitize=leak -fsanitize=undefined)
            target_link_options(${test_name} PRIVATE -fsanitize=address -fsanitize=leak -fsanitize=undefined)
        endif()
    endforeach()
endif()