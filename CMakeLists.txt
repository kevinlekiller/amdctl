set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -std=c99 -O2 -lm")
cmake_minimum_required (VERSION 2.6)
project(amdctl)
add_executable(amdctl amdctl.c)