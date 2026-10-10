cmake_minimum_required(VERSION 3.23)

project(AevoraWorks)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

enable_testing()

# Code for every program, then each program
add_subdirectory(shared)
add_subdirectory(valuma)
