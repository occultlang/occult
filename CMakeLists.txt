cmake_minimum_required(VERSION 3.5)

project(occult)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -Wall -Wextra")

add_executable(occultc lexer.hpp lexer_maps.hpp lexer.cpp ast.hpp error.hpp error.cpp parser.hpp parser.cpp main.cpp)
