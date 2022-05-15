cmake_minimum_required(VERSION 3.16)
project(RedBase)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -pedantic-errors")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# GoogleTest
option(REDBASE_ENABLE_TEST "" ON)
if (REDBASE_ENABLE_TEST)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # Do not install GTest when `make install`
    set(INSTALL_GTEST OFF)
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)
endif ()

add_subdirectory(src)

# clang-format
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
add_custom_target(format COMMAND clang-format -i ${SOURCES})
