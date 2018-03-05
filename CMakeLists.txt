cmake_minimum_required(VERSION 2.8.4)
project(HUST_fs)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(Kernel_INCLUDE_DIR /lib/modules/4.14.21-1-MANJARO/build/include)
include_directories(${Kernel_INCLUDE_DIR})
add_custom_target(HUST_fs.ko ALL COMMAND make WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(mkfs-HUST_fs mkfs-HUST_fs.c)
add_executable(HUST_fs HUST_fs.c)