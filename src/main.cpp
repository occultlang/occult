#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/parser.hpp"
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x86_64_codegen.hpp"
#include <source_location>
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
  std::cout << "Usage: occultc [options] <source.occ>\n"
            << "Options:\n"
            << "  -t,   --time              Show compilation time per stage\n"
            << "  -d,   --debug             Enable debug mode (implies --time)\n"
            << "  -o,   --output <file>     Output native binary\n"
            << "  -j,   --jit               JIT compile (in memory)\n"
            << "  -h,   --help              Show this message\n";
}

enum class codegenmode : std::uint8_t {
  none,
  stack,
  _register
};

int equals_aot(std::int64_t a, std::int64_t b) {
  return a == b;
}

int main(int argc, char* argv[]) {  
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  bool verbose = false;
  bool showtime = false;
  bool jit = true; // we will default to JIT but still have the arg if anyone wants to use it /shrug

  std::string filenameout;
  
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
    else if (arg == "-o" || arg == "--output") {
      jit = false;
      
      if (i + 1 < argc) {  
        jit = false;
        filenameout = argv[++i]; 
      }
      else {
        filenameout = "a.out";
      }
    }
    else if (arg == "-j" || arg == "--jit") {
      jit = true;
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
    std::cout << GREEN << "[OCCULTC] Completed lexical analysis \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    lexer.visualize();
  }
  
  start = std::chrono::high_resolution_clock::now();
  occult::parser parser(tokens);
  auto cst = parser.parse();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (parser.get_state() == occult::parser::state::failed) {
    std::cout << RED << "[OCCULTC] Parsing failed" << RESET << std::endl;
    return 1;
  }
  else if (showtime) {
     std::cout << GREEN << "[OCCULTC] Completed parsing \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    cst->visualize();
  }

  start = std::chrono::high_resolution_clock::now();
  occult::ir_gen ir_gen(cst.get());
  auto ir = ir_gen.lower_to_stack();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << GREEN << "[OCCULTC] Completed generating IR \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    ir_gen.visualize_stack_ir(ir);
  }
  
  start = std::chrono::high_resolution_clock::now();
  occult::x86_64::codegen jit_runtime(ir, debug);
  jit_runtime.compile(jit);
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << GREEN << "[OCCULTC] Completed converting IR to machine code \033[0m" << duration.count() << "ms\n";
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
      
    auto res = reinterpret_cast<std::int64_t(*)()>(it->second)();
      
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
      
    if (debug) {
      std::cout << "Main returned: " << res << std::endl;
    }
      
    if (showtime) {
      std::cout << GREEN << "[OCCULTC] Completed executing jit code " << RESET << duration.count() << "ms\n";
    }
  }
  else {
    std::cerr << "Main function not found!" << std::endl;
  }
  
  /*else if (!jit) {
    occult::linker::link_and_create_binary(filenameout, jit_runtime.function_map, jit_runtime.function_raw_code_map, debug, showtime);
#ifdef __linux
    chmod(filenameout.c_str(), S_IRWXU);
#endif
  }*/
  
  return 0;
}
  
