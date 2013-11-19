cmake_minimum_required(VERSION 2.6)
project(Diana)

IF(CMAKE_COMPILER_IS_GNUCC)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

add_library(DianaC diana.c)
add_library(DianaCPP diana.c cpp/diana.cpp)

add_executable(ExampleC example.c)
add_executable(ExampleCPP cpp/example.cpp)
add_executable(FuzzTest tests/fuzz.c)

target_link_libraries(ExampleC DianaC)
target_link_libraries(ExampleCPP DianaCPP)
