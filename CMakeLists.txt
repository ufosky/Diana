cmake_minimum_required(VERSION 2.6)
project(Diana)

add_library(DianaC diana.c)
add_library(DianaCPP diana.c diana.cpp)

add_executable(ExampleC example.c)
add_executable(ExampleCPP example.cpp)

target_link_libraries(ExampleC DianaC)
target_link_libraries(ExampleCPP DianaCPP)
