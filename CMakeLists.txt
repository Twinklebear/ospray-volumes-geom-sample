cmake_minimum_required(VERSION 3.5)
project(ospray_volumes_geom_sample)
set(CMAKE_CXX_STANDARD 11)

find_package(ospray REQUIRED)

include_directories(${OSPRAY_INCLUDE_DIRS})
add_executable(sample_app main.cpp)
target_link_libraries(sample_app ${OSPRAY_LIBRARIES})
target_compile_definitions(sample_app PUBLIC NOMINMAX)

