cmake_minimum_required(VERSION 3.20)
project(tiny-bdd-cec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TBC_WERROR "Treat warnings as errors" OFF)
option(BUILD_TESTING "Build test suite" ON)

# Compiler warning flags
add_library(tbc_warnings INTERFACE)
if(MSVC)
    target_compile_options(tbc_warnings INTERFACE /W4 /permissive-)
    if(TBC_WERROR)
        target_compile_options(tbc_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(tbc_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )
    if(TBC_WERROR)
        target_compile_options(tbc_warnings INTERFACE -Werror)
    endif()
endif()

# Core static library
add_library(tbc STATIC
    src/bdd.cpp
    src/netlist.cpp
    src/netlist_to_bdd.cpp
    src/blif_parser.cpp
    src/verilog_lexer.cpp
    src/verilog_parser.cpp
    src/elaborate.cpp
    src/cec.cpp
    src/dot.cpp
)

target_include_directories(tbc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tbc PUBLIC tbc_warnings)

# CLI Application
add_executable(tiny-bdd-cec apps/tiny-bdd-cec/main.cpp)
target_link_libraries(tiny-bdd-cec PRIVATE tbc)

# Tests
if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found locally, fetching via FetchContent...")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(tbc_tests
        tests/test_reduction.cpp
        tests/test_axioms.cpp
        tests/test_canonicity.cpp
        tests/test_ite.cpp
        tests/test_cache.cpp
        tests/test_verilog.cpp
        tests/test_arith_reference.cpp
        tests/test_cec.cpp
        tests/test_order.cpp
        tests/test_blif.cpp
    )

    target_include_directories(tbc_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(tbc_tests PRIVATE tbc Catch2::Catch2WithMain)

    include(CTest)
    include(Catch)
    catch_discover_tests(tbc_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
