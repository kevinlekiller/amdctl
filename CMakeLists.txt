cmake_minimum_required(VERSION 2.8.12)
project(amdctl)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra -std=c99")
add_executable(amdctl amdctl.c)
