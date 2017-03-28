cmake_minimum_required(VERSION 3.0)
project(k5-dmenu-desktop)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -std=c99 -pedantic -Wall -Wextra")
add_executable(${PROJECT_NAME} main.c)
