cmake_minimum_required(VERSION 3.5)

project(occult)
# -O3 normally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++23 -O0 -Wall -Wextra")

add_executable(occultc
./src/lexer/lexer.hpp
./src/lexer/lexer_maps.hpp
./src/lexer/lexer.cpp
./src/parser/ast.hpp
./src/parser/error.hpp
./src/parser/error.cpp
./src/parser/parser.hpp
./src/parser/parser.cpp
./src/backend/x86_writer.cpp
./src/backend/x86_writer.hpp
./src/backend/elf_header.cpp
./src/backend/elf_header.hpp
./src/main.cpp)
