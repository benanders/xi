cmake_minimum_required(VERSION 3.21)
project(xi C)

set(CMAKE_C_STANDARD 99)

include_directories(deps/termbox2)
add_executable(xi
        src/main.c
        src/editor.c src/editor.h)
