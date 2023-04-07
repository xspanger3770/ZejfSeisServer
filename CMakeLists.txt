cmake_minimum_required(VERSION 3.0)

# Project configuration
project(zejfcseis)
file(GLOB SOURCES "src/*.c")
set(ZEJFCSEIS_VERSION "1.5.0")
set(EXECUTABLE zejfcseis_${ZEJFCSEIS_VERSION})

set(ALL_FLAGS "-O2 -s -std=gnu99 -Wall -Wextra -Werror -Wpedantic")

# Executable
add_executable(${EXECUTABLE} ${SOURCES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
