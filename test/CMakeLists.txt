cmake_minimum_required(VERSION 3.30)
project(Test)

enable_testing()


set(CMAKE_CXX_STANDARD 17)

find_package(spdlog REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/utils)
include_directories(${CMAKE_SOURCE_DIR}/concurrence)
include_directories(${CMAKE_SOURCE_DIR})


add_executable(GoCoroutine main.cpp
        common/anys.cpp
        common/anys.h
        utils/utils.h
        concurrence/spinlock.h)


# 定义一个函数，自动帮test添加lib
function(add_test target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} GoCoroutine)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

if (TEST)

endif (TEST)

target_link_libraries(GoCoroutine GTest::gtest GTest::gtest_main
        spdlog::spdlog)
