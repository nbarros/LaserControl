cmake_minimum_required(VERSION 3.1)
include(CMakePrintHelpers)
project(
  controlLibrary
  VERSION 1.0
  LANGUAGES CXX)
 
# We need c++11
set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)


## load in pkg-config support
find_package(PkgConfig)
## use pkg-config to get hints for 0mq locations
pkg_check_modules(PC_ZeroMQPP cppzmq)

cmake_print_variables(PC_ZeroMQPP_INCLUDE_DIRS)

## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
        NAMES zmq.hpp
        PATHS ${PC_ZeroMQ_INCLUDE_DIRS}
        )
cmake_print_variables(ZeroMQ_INCLUDE_DIR)

## use the hint from above to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
        )
cmake_print_variables(ZeroMQ_LIBRARY)
        
# get all *.cpp files recursively
#file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
include_directories(${PROJECT_SOURCE_DIR}/include)
#add_subdirectory(${PROJECT_SOURCE_DIR}/src)

# -- this would be to install separate applications
set(LIB_SRC_FILES ${PROJECT_SOURCE_DIR}/src/Attenuator.cpp ${PROJECT_SOURCE_DIR}/src/Device.cpp ${PROJECT_SOURCE_DIR}/src/Laser.cpp ${PROJECT_SOURCE_DIR}/src/PowerMeter.cpp ${PROJECT_SOURCE_DIR}/src/serial.cc ${PROJECT_SOURCE_DIR}/src/utilities.cpp)
#add_executable(test2 ${SOURCE_FILES})

add_library(LaserControl STATIC ${LIB_SRC_FILES})
target_compile_features(LaserControl PUBLIC cxx_std_11)

# Now add a server target
add_executable(control_server ${PROJECT_SOURCE_DIR}/src/control_server.cpp)
## add the include directory to our compile directives
target_include_directories(control_server PUBLIC ${ZeroMQ_INCLUDE_DIR})
## at the 0mq library to our link directive
target_link_libraries(control_server PUBLIC ${ZeroMQ_LIBRARY})

