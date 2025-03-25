cmake_minimum_required(VERSION 3.5)

project(occult)
# -O3 normally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++23 -O3 -Wall -Wextra")

add_executable(occultc
./src/lexer/lexer.hpp
./src/lexer/lexer_maps.hpp
./src/lexer/lexer.cpp
./src/parser/ast.hpp
./src/parser/error.hpp
./src/parser/error.cpp
./src/parser/parser.hpp
./src/parser/parser.cpp
./src/backend/writer.cpp
./src/backend/writer.hpp
./src/backend/elf_header.cpp
./src/backend/elf_header.hpp
./src/backend/ir_gen.cpp
./src/backend/ir_gen.hpp
./src/backend/jit.cpp
./src/backend/jit.hpp
./src/backend/linker/linker.cpp
./src/backend/linker/linker.hpp
./src/main.cpp)
