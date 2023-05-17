cmake_minimum_required(VERSION 3.1)

project(
  controlLibrary
  VERSION 1.0
  LANGUAGES CXX)
 
# We need c++11
set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# get all *.cpp files recursively
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
include_directories(${PROJECT_SOURCE_DIR}/include)
add_library(LaserControl STATIC ${SRC_FILES})
target_compile_features(LaserControl PUBLIC cxx_std_11)
