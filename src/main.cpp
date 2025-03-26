#include "lexer/lexer.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x64writer.hpp"
#include "backend/codegen/jit.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#ifdef __linux
#include <sys/stat.h>
#include "backend/linker/linker.hpp"
#elif _WIN64
#include "backend/pe_header.hpp"
#endif

void display_help() {
  std::cout << "Usage: occultc [options] <source.occ>\n";
  std::cout << "Options:\n";
  std::cout << "  -t, --time                     Shows the compilation time for each stage.\n";
  std::cout << "  -d, --debug                    Enable debugging options (shows time as well -t is not needed)\n";
  std::cout << "  -h, --help                     Display this help message." << std::endl;
}

int main(int argc, char* argv[]) {
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  bool verbose = false;
  bool showtime = false;
  
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "-d" || arg == "--debug") {
      debug = true;
      verbose = true;
      showtime = true;
    }
    else if (arg == "-t" || arg == "--time") {
      showtime = true;
    }
    else if (arg == "-h" || arg == "--help") {
      display_help();
      
      return 0;
    }
    else {
      input_file = arg;
    }
  }
  
  std::ifstream file(input_file);
  std::stringstream buffer;
  buffer << file.rdbuf();
  source_original = buffer.str();

  if (input_file.empty()) {
    std::cout << "No input file specified" << std::endl;
    display_help();
    
    return 0;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  occult::lexer lexer(source_original);
  auto tokens = lexer.analyze();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  if (showtime) {
    std::cout << "[occultc] \033[1;35mcompleted lexical analysis \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    lexer.visualize();
  }
  
  start = std::chrono::high_resolution_clock::now();
  occult::parser parser(tokens);
  auto ast = parser.parse();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << "[occultc] \033[1;35mcompleted parsing \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    ast->visualize();
  }

  start = std::chrono::high_resolution_clock::now();
  occult::ir_gen ir_gen(ast.get());
  auto ir = ir_gen.lower();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << "[occultc] \033[1;35mcompleted generating ir \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    ir_gen.visualize(ir);
  }

  start = std::chrono::high_resolution_clock::now();
  occult::jit_runtime jit_runtime(ir, debug);
  jit_runtime.convert_ir();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << "[occultc] \033[1;35mcompleted converting ir to machine code \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    for (const auto& pair : jit_runtime.function_map) {
      std::cout << pair.first << std::endl;
      std::cout << "0x" << std::hex << reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
    }
  }
  
  auto it = jit_runtime.function_map.find("main");
  if (it != jit_runtime.function_map.end()) {
    start = std::chrono::high_resolution_clock::now();
    
    auto res = it->second();
    
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    
    if (debug) {
      std::cout << "JIT main returned: " << res << std::endl;
    }
    
    if (showtime) {
      std::cout << "[occultc] \033[1;35mcompleted executing jit code \033[0m" << duration.count() << "ms\n";
    }
    
    occult::linker::link_and_create_binary("a.out", jit_runtime.function_map, jit_runtime.function_raw_code_map);
  }
  else {
    std::cerr << "main function not found!" << std::endl;
  }
  
  return 0;
}
  
