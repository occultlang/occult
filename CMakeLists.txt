cmake_minimum_required(VERSION 3.5)

project(occult)
# -O3 normally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++23 -O0 -Wall -Wextra")

add_executable(occultc
./src/lexer/lexer.hpp
./src/lexer/lexer_maps.hpp
./src/lexer/lexer.cpp
./src/parser/cst.hpp
./src/parser/error.hpp
./src/parser/error.cpp
./src/parser/parser.hpp
./src/parser/parser.cpp
./src/backend/codegen/writer.cpp
./src/backend/codegen/writer.hpp
./src/backend/linker/elf_header.cpp
./src/backend/linker/elf_header.hpp
./src/backend/linker/linker.cpp
./src/backend/linker/linker.hpp
./src/backend/codegen/ir_gen.cpp
./src/backend/codegen/ir_gen.hpp
./src/backend/codegen/jit.cpp
./src/backend/codegen/jit.hpp
./src/main.cpp)
