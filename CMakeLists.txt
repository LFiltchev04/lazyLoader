cmake_minimum_required(VERSION 3.10)
project(LazyFetcher VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NGHTTP2 REQUIRED libnghttp2)

find_library(LMDB_LIBRARY NAMES lmdb REQUIRED)
find_path(LMDB_INCLUDE_DIR NAMES lmdb.h REQUIRED)

file(GLOB SOURCES "src/*.cpp")
add_executable(LazyFetcher main.cpp ${SOURCES})

target_include_directories(LazyFetcher PRIVATE include ${LMDB_INCLUDE_DIR})
target_link_libraries(LazyFetcher PRIVATE ${LMDB_LIBRARY})

target_include_directories(LazyFetcher PRIVATE ${NGHTTP2_INCLUDE_DIRS})
target_link_libraries(LazyFetcher PRIVATE ${NGHTTP2_LIBRARIES})