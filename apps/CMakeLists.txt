
#set(CMAKE_VERBOSE_MAKEFILE ON)



add_executable(serial_manager serial_manager.cpp)
target_compile_features(serial_manager PUBLIC cxx_std_11)
set_target_properties(serial_manager PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(serial_manager PROPERTIES CXX_EXTENSIONS OFF)
#target_include_directories(serial_manager PRIVATE ${PROJECT_SOURCE_DIR}/cib_debug)
#target_include_directories(test_lbls PRIVATE ${PROJECT_SOURCE_DIR}/common)
target_link_libraries(serial_manager LaserControl spdlog readline)

