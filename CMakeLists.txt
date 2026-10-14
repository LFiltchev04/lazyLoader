cmake_minimum_required(VERSION 3.10)
project(LazyFetcher VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NGHTTP2 REQUIRED libnghttp2)

pkg_check_modules(LIBURING REQUIRED liburing)

find_library(LMDB_LIBRARY NAMES lmdb REQUIRED)
find_path(LMDB_INCLUDE_DIR NAMES lmdb.h REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(LazyFetcherLib STATIC ${SOURCES})
target_include_directories(LazyFetcherLib PUBLIC include src ${LMDB_INCLUDE_DIR} ${NGHTTP2_INCLUDE_DIRS} ${LIBURING_INCLUDE_DIRS})
target_link_libraries(LazyFetcherLib PUBLIC ${LMDB_LIBRARY} ${NGHTTP2_LIBRARIES} ${LIBURING_LIBRARIES})

add_executable(LazyFetcher main.cpp)
target_link_libraries(LazyFetcher PRIVATE LazyFetcherLib)

# Tests
include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)
file(GLOB TEST_SOURCES "tests/*.cpp")
add_executable(LazyFetcherTests ${TEST_SOURCES})
target_link_libraries(LazyFetcherTests PRIVATE LazyFetcherLib GTest::gtest_main)
gtest_discover_tests(LazyFetcherTests)