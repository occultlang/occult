cmake_minimum_required(VERSION 3.16)

project(occult CXX)

# One source list for every host. The platform differences live in the
# sources behind __linux__ / _WIN64, not here: the object writers are
# portable byte emission, and the JIT's mapping and signal handling are
# the only parts that really differ.
add_executable(occultc
	./src/lexer/lexer.hpp
	./src/lexer/lexer_maps.hpp
	./src/lexer/lexer.cpp
	./src/parser/cst.hpp
	./src/parser/error.hpp
	./src/parser/error.cpp
	./src/parser/parser.hpp
	./src/parser/parser.cpp
	./src/parser/error_builtin.hpp
	./src/parser/error_builtin.cpp
	./src/code_analysis/linter.hpp
	./src/code_analysis/linter.cpp
	./src/backend/codegen/writer.cpp
	./src/backend/codegen/writer.hpp
	./src/backend/codegen/ir_gen.cpp
	./src/backend/codegen/ir_gen.hpp
	./src/backend/linker/elf_defs.hpp
	./src/backend/linker/elf_header.cpp
	./src/backend/linker/elf_header.hpp
	./src/backend/linker/elf_object.cpp
	./src/backend/linker/elf_object.hpp
	./src/backend/linker/coff_object.cpp
	./src/backend/linker/coff_object.hpp
	./src/backend/linker/pe_header.cpp
	./src/backend/linker/pe_header.hpp
	./src/backend/linker/linker.cpp
	./src/backend/linker/linker.hpp
	./src/main.cpp)

set_target_properties(occultc PROPERTIES
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)

# build.bat drives this through clang-cl, which takes MSVC's flags rather
# than gcc's, so neither set can be written into CMAKE_CXX_FLAGS.
if (MSVC)
	target_compile_options(occultc PRIVATE /W3 /permissive- $<$<CONFIG:Release>:/O2>)
	# windows.h defines min and max as macros, and the code generator calls
	# std::max; WIN32_LEAN_AND_MEAN keeps the rest of the platform out.
	target_compile_definitions(occultc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(occultc PRIVATE -Wall -Wextra -g $<$<NOT:$<CONFIG:Debug>>:-O3>)
	if (WIN32)
		target_compile_definitions(occultc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
	endif()
	if (MINGW)
		# a self-contained occultc.exe, rather than one that wants
		# libstdc++-6.dll and libgcc_s_seh-1.dll beside it
		target_link_options(occultc PRIVATE -static -static-libgcc -static-libstdc++)
	endif()
endif()

# resolving an extern fn under the JIT: dlopen on Linux, which is empty on a
# libc that folded libdl in, and EnumProcessModules on Windows.
target_link_libraries(occultc PRIVATE ${CMAKE_DL_LIBS})
if (WIN32)
	target_link_libraries(occultc PRIVATE psapi)
endif()
