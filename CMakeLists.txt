cmake_minimum_required(VERSION 3.10)

project(occult)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++23 -O3 -Wall -Wextra -g") # -fsanitize=address,undefined -fno-omit-frame-pointer

if (WIN32)
	add_executable(occultc
	./src/lexer/lexer.hpp
	./src/lexer/lexer_maps.hpp
	./src/lexer/lexer.cpp
	./src/parser/cst.hpp
	./src/parser/error.hpp
	./src/parser/error.cpp
	./src/parser/parser.hpp
	./src/parser/parser.cpp
	./src/code_analysis/linter.hpp
	./src/code_analysis/linter.cpp
	./src/backend/codegen/writer.cpp
	./src/backend/codegen/writer.hpp
	./src/backend/codegen/ir_gen.cpp
	./src/backend/codegen/ir_gen.hpp 
	./src/third-party/xxhash/xxhash.h
	./src/third-party/xxhash/xxhash.c
	./src/main.cpp)
	elseif (UNIX)
	add_executable(occultc
	./src/lexer/lexer.hpp
	./src/lexer/lexer_maps.hpp
	./src/lexer/lexer.cpp
	./src/parser/cst.hpp
	./src/parser/error.hpp
	./src/parser/error.cpp
	./src/parser/parser.hpp
	./src/parser/parser.cpp
	./src/code_analysis/linter.hpp
	./src/code_analysis/linter.cpp
	./src/backend/codegen/writer.cpp
	./src/backend/codegen/writer.hpp
	./src/backend/linker/elf_header.cpp
	./src/backend/linker/elf_header.hpp
	./src/backend/linker/linker.cpp
	./src/backend/linker/linker.hpp
	./src/backend/codegen/ir_gen.cpp
	./src/backend/codegen/ir_gen.hpp 
	./src/third-party/xxhash/xxhash.h
	./src/third-party/xxhash/xxhash.c
	./src/main.cpp)
endif()