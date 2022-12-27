cmake_minimum_required(VERSION 3.0)

# Project configuration
project(zejfcseis)
file(GLOB SOURCES "src/*.c")
set(ZEJFCSEIS_VERSION "1.4.1")
set(EXECUTABLE zejfcseis_${ZEJFCSEIS_VERSION})

#set(ALL_FLAGS "-O2 -g -std=gnu99 -Wall -Wextra -Werror")
set(ALL_FLAGS "-O2 -s -std=gnu99 -Wall -Wextra -Werror")

# Executable
add_executable(${EXECUTABLE} ${SOURCES})

# Configure compiler warnings
if (CMAKE_C_COMPILER_ID MATCHES Clang OR ${CMAKE_C_COMPILER_ID} STREQUAL GNU)
  # using regular Clang, AppleClang or GCC
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
  # using Visual Studio C++
  target_compile_definitions(${EXECUTABLE} PRIVATE _CRT_SECURE_NO_DEPRECATE)
  set(CMAKE_CXX_FLAGS "/permissive- /W4 /EHsc")
endif()

if(MINGW)
    target_compile_definitions(${EXECUTABLE} PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()

