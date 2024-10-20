cmake_minimum_required(VERSION 3.27)
project(OS_Lab1 C)

set(CMAKE_C_STANDARD 11)


add_executable(OS_Lab1 Server.c)
target_link_libraries(OS_Lab1 PRIVATE ws2_32)
