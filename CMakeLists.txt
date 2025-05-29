cmake_minimum_required(VERSION 3.30)
project(GoCoroutine)



# 调试配置
set(CMAKE_CXX_FLAGS_DEBUG "-rdynamic -O0 -ggdb -Wall -Wno-deprecated -Wno-unused-function -Wno-builtin-macro-redefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wno-deprecated -Wno-unused-function -Wno-builtin-macro-redefined")

option(TEST "ON for complile test" ON)
enable_testing()
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/utils)
include_directories(${CMAKE_SOURCE_DIR}/concurrence)
include_directories(${CMAKE_SOURCE_DIR})


add_executable(GoCoroutine main.cpp
        common/anys.h
        utils/utils.h
        concurrence/spinlock.h
        common/clock.h
        utils/macro.h
        test/test_clock.cpp)


# 定义一个函数，自动帮test添加lib
function(add_test target_name target_source)
    message("Adding test: ${target_name} with source: ${target_source}")
    add_executable(${target_name} ${target_source})
    target_link_libraries(${target_name} GTest::gtest GTest::gtest_main)
endfunction()

if (TEST)
    add_test(test_anys test/test_anys.cpp)
    add_test(test_clock test/test_clock.cpp)
endif (TEST)

target_link_libraries(GoCoroutine GTest::gtest GTest::gtest_main)
