cmake_minimum_required(VERSION 3.10)
project(test_cnanolog C)

set(CMAKE_C_STANDARD 11)

find_package(CNanoLog REQUIRED CONFIG)

add_executable(test_cnanolog test_cnanolog.c)
target_link_libraries(test_cnanolog PRIVATE CNanoLog::cnanolog)
