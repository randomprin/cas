cmake_minimum_required(VERSION 3.6)
project(demo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp
        Tes_LockFree_Obj_Fifo.h)
add_executable(demo ${SOURCE_FILES})