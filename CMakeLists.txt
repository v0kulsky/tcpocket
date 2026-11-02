cmake_minimum_required(VERSION 3.20)

project(tcpocket
    VERSION 0.1.0
    DESCRIPTION "A C++20 library for handling TCP connections"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(tcpocket
    src/tcpocket.cpp
)

target_include_directories(tcpocket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_executable(tcpocket_app
    src/main.cpp
)

target_link_libraries(tcpocket_app
    PRIVATE
        tcpocket
)

option(BUILD_TESTS "Build test suite" ON)

if(BUILD_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    include(GoogleTest)

    add_executable(unit_tests
        tests/test_tcpocket.cpp
    )

    target_link_libraries(unit_tests
        PRIVATE
            tcpocket
            GTest::gtest_main
    )

    gtest_discover_tests(unit_tests)
endif()
