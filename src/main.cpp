#include "backend/codegen/function_registry.hpp"
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x86_64_codegen.hpp"
#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/parser.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#ifdef __linux
#include "backend/linker/linker.hpp"
#include <sys/stat.h>
#elif _WIN64
#include "backend/linker/pe_header.hpp"
#endif

#include <cstdlib>

void display_help() {
  std::cout
      << "Usage: occultc [options] <source.occ>\n"
      << "Options:\n"
      << "  -t,   --time              Show compilation time per stage\n"
      << "  -d,   --debug             Enable debug mode (implies --time)\n"
      << "  -o,   --output <file>     Output native binary\n"
      << "  -j,   --jit               JIT compile (in memory)\n"
      << "  -h,   --help              Show this message\n";
}

// Minimal bump allocator backed by a static arena; avoids libc malloc/free
OCCULT_FUNC_DECL(std::int64_t, alloc, (std::int64_t sz), std::int64_t) {
  if (sz <= 0) {
    sz = 8;
  }
  static std::uint8_t arena[1 << 20] = {}; // 1MB arena
  static std::size_t offset = 0;
  const std::size_t align = 16;
  const std::size_t aligned =
      static_cast<std::size_t>((sz + align - 1) & ~(align - 1));
  if (aligned > sizeof(arena) - offset) {
    return 0; // out of space
  }
  void *ptr = arena + offset;
  offset += aligned;
  return reinterpret_cast<std::int64_t>(ptr);
}

OCCULT_FUNC_DECL(std::int64_t, del, (std::int64_t), std::int64_t) {
  // bump allocator is non-freeing; no-op
  return 0;
}

OCCULT_FUNC_DECL(std::int64_t, print_string, (std::int64_t str), std::int64_t) {
  if (str == 0) {
    return 0;
  }
  const char *ptr = reinterpret_cast<const char *>(str);
  std::size_t len = 0;
  while (ptr[len] != '\0') {
    ++len;
  }
  std::cout.write(ptr, static_cast<std::streamsize>(len));
  std::cout.flush();
  return static_cast<std::int64_t>(len);
}

OCCULT_FUNC_DECL(std::int64_t, print_integer, (std::int64_t num),
                 std::int64_t) {
  std::string s = std::to_string(num);
  std::cout << s;
  std::cout.flush();
  return 0;
}

OCCULT_FUNC_DECL(std::int64_t, print_newline, (), std::int64_t) {
  std::cout.put('\n');
  std::cout.flush();
  return 0;
}

int main(int argc, char *argv[]) {
  std::string input_file;
  std::string source_original;

  bool debug = false;
  bool verbose = false;
  bool showtime = false;
  bool jit = true; // we will default to JIT but still have the arg if anyone
                   // wants to use it /shrug

  std::string filenameout;

  for (int i = 1; i < argc; ++i) {
    if (std::string arg = argv[i]; arg == "-d" || arg == "--debug") {
      debug = true;
      verbose = true;
      showtime = true;
    } else if (arg == "-t" || arg == "--time") {
      showtime = true;
    } else if (arg == "-o" || arg == "--output") {
      jit = false;

      if (i + 1 < argc) {
        jit = false;
        filenameout = argv[++i];
      } else {
        filenameout = "a.out";
      }
    } else if (arg == "-j" || arg == "--jit") {
      jit = true;
    } else if (arg == "-h" || arg == "--help") {
      display_help();

      return 0;
    } else {
      input_file = arg;
    }
  }

  std::ifstream file(input_file);
  std::stringstream buffer;
  buffer << file.rdbuf();
  source_original = buffer.str();

  if (input_file.empty()) {
    std::cout << RED << "[-] No input file specified" << RESET << std::endl;
    display_help();

    return 0;
  }

  auto start = std::chrono::high_resolution_clock::now();
  occult::lexer lexer(source_original);
  auto tokens = lexer.analyze();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  if (showtime) {
    std::cout << GREEN << "[OCCULTC] Completed lexical analysis \033[0m"
              << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    lexer.visualize();
  }

  start = std::chrono::high_resolution_clock::now();
  occult::parser parser(tokens, input_file);
  auto cst = parser.parse();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (parser.get_state() == occult::parser::state::failed) {
    std::cout << RED << "[OCCULTC] Parsing failed" << RESET << std::endl;
    return 1;
  }

  if (showtime) {
    std::cout << GREEN << "[OCCULTC] Completed parsing \033[0m"
              << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    cst->visualize();
  }

  start = std::chrono::high_resolution_clock::now();
  occult::ir_gen ir_gen(cst.get(), parser.get_custom_type_map(), debug);
  auto ir_structs = ir_gen.lower_structs();
  auto ir = ir_gen.lower_functions();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << GREEN << "[OCCULTC] Completed generating IR \033[0m"
              << duration.count() << "ms\n";
  }
  if (debug) {
    occult::ir_gen::visualize_structs(ir_structs);
    occult::ir_gen::visualize_stack_ir(ir);
  }

  // Register alloc/del and override stdio-style functions with native
  // implementations
  occult::function_registry::register_function_to_ir<&alloc>(ir);
  occult::function_registry::register_function_to_ir<&del>(ir);
  occult::function_registry::register_function_to_ir<&print_string>(ir);
  occult::function_registry::register_function_to_ir<&print_integer>(ir);
  occult::function_registry::register_function_to_ir<&print_newline>(ir);

  for (auto &f : ir) {
    if (f.name == "print_string" || f.name == "print_integer" ||
        f.name == "print_newline") {
      f.is_external = true;
    }
  }

  start = std::chrono::high_resolution_clock::now();
  occult::x86_64::codegen jit_runtime(ir, ir_structs, debug);

  occult::function_registry::register_function_to_codegen<&alloc>(jit_runtime);
  occult::function_registry::register_function_to_codegen<&del>(jit_runtime);
  occult::function_registry::register_function_to_codegen<&print_string>(
      jit_runtime);
  occult::function_registry::register_function_to_codegen<&print_integer>(
      jit_runtime);
  occult::function_registry::register_function_to_codegen<&print_newline>(
      jit_runtime);

  jit_runtime.compile(jit);
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << GREEN
              << "[OCCULTC] Completed converting IR to machine code \033[0m"
              << duration.count() << "ms\n";
  }
  /*if (debug && jit) {
    for (const auto& pair : jit_runtime.function_map) {
      std::cout << pair.first << std::endl;
      std::cout << "0x" << std::hex <<
  reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
    }
  }*/

  if (jit) {
    if (auto it = jit_runtime.function_map.find("main");
        it != jit_runtime.function_map.end()) {
      start = std::chrono::high_resolution_clock::now();

      auto res = reinterpret_cast<std::int64_t (*)()>(it->second)();

      end = std::chrono::high_resolution_clock::now();
      duration = end - start;

      if (debug) {
        std::cout << "Main returned: " << res << std::endl;
      }

      if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed executing jit code " << RESET
                  << duration.count() << "ms\n";
      }
    } else {
      std::cerr << "Main function not found!" << std::endl;
    }
  }

#ifdef __linux
  else if (!jit) {
    occult::linker::link_and_create_binary(
        filenameout, jit_runtime.function_map,
        jit_runtime.function_raw_code_map, debug, showtime);

    chmod(filenameout.c_str(), S_IRWXU);
  }
#endif

  return 0;
}
