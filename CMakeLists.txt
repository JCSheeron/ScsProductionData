# specify the minimum version for CMake
cmake_minimum_required(VERSION 3.9)

# project name
project(ScsProductionData)

# set the output folder where the program will be created
set (CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set (LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# The following folder will be included
include_directories(
    "${PROJECT_SOURCE_DIR}"
    "/usr/include/c++/5"
    "/home/jcsheeron/virtualMachines/vmShare/software/boost_1_63_0"
    )

# generate a compile commands database
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set which c++ standard to use. Valid values are 98, 11, 14
set (CMAKE_XXX_STANDARD 98)
#
# compile the source
add_executable(ScsProductionData ${PROJECT_SOURCE_DIR}/main.cpp)

