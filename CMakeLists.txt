cmake_minimum_required(VERSION 3.5)

project(occult)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23 -O3 -Wall -Wextra")

add_executable(occultc
./src/lexer/lexer.hpp
./src/lexer/lexer_maps.hpp
./src/lexer/lexer.cpp
./src/parser/ast.hpp
./src/parser/error.hpp
./src/parser/error.cpp
./src/parser/parser.hpp
./src/parser/parser.cpp
./src/sigil/vm/opcode_maps.hpp
./src/bytecode_gen/bytecode_gen.hpp
./src/bytecode_gen/bytecode_gen.cpp
./src/main.cpp)
