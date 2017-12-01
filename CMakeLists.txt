# specify the minimum version for CMake
cmake_minimum_required(VERSION 2.8)

# project name
project(HelloWorld)

# set the output folder where the program will be created
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# The following folder will be included
include_directories("${PROJECT_SOURCE_DIR}")

# generate a compile commands database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# compile the source
add_executable(jimmy ${PROJECT_SOURCE_DIR}/helloWorld.cpp)

