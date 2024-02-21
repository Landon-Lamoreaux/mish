cmake_minimum_required(VERSION 3.27)
project(mish)

set(CMAKE_CXX_STANDARD 23)

include_directories(/usr/include/readline)

add_executable(mish mish.cpp
        cmake-build-debug/mish.h)

target_link_libraries(mish readline)