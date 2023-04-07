cmake_minimum_required(VERSION 3.0)

# Project configuration

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
set(CMAKE_C_CPPCHECK "cppcheck")
set(CMAKE_C_CLANG_TIDY "clang-tidy;-format-style=file;--config-file=../.clang-tidy")
set(ALL_FLAGS "-O2 -std=gnu99 -Wall -Wextra -Werror -Wpedantic")

# Executable

set(ZEJFCSEIS_VERSION "1.5.0")
set(EXECUTABLE zejfcseis_${ZEJFCSEIS_VERSION})

project(zejfcseis)
file(GLOB SOURCES "src/*.c")

add_executable(${EXECUTABLE} ${SOURCES})

