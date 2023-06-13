cmake_minimum_required(VERSION 3.0)

# Project configuration

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
set(CMAKE_C_CPPCHECK "cppcheck")
set(CMAKE_C_CLANG_TIDY "clang-tidy;-format-style=file;--config-file=../.clang-tidy")
set(ALL_FLAGS "-O2 -std=gnu99 -Wall -Wextra -Werror -Wpedantic")

# Executable

set(ZEJF_VERSION "1.5.1")
set(EXECUTABLE zejfseis_server_${ZEJF_VERSION})

project(zejfseis_server)
file(GLOB SOURCES "src/*.c")

add_executable(${EXECUTABLE} ${SOURCES})

