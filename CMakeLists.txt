cmake_minimum_required(VERSION 3.21)
project(xi C)

set(CMAKE_C_STANDARD 99)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(xi src/main.c)

target_link_libraries(xi ${CURSES_LIBRARIES})
