cmake_minimum_required(VERSION 3.0)

project(server)

set(CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(LIB_PATH /lib)

add_subdirectory(src)