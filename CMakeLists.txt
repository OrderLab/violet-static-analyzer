cmake_minimum_required(VERSION 3.5)
project(static_analyzer)

set(CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
SET(CMAKE_CXX_FLAGS "-Wall -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "The CMAKE_CXX_FLAGS : ${CMAKE_CXX_FLAGS}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Find the libraries that correspond to the LLVM components
# that we wish to use
#llvm_map_components_to_libnames(llvm_libs support core irreader)
#list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(AddLLVM)
include_directories(dependencyAnalysis)
add_subdirectory(dependencyAnalysis)

#add_executable(static_analyzer main.cpp)
