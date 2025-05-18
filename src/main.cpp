#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/parser.hpp"
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x64writer.hpp"
#include "backend/codegen/jit.hpp"
#include "backend/codegen/x86_64_writer.hpp"

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
  std::cout << "  -o, --output <filename>        Output a native binary\n";
  std::cout << "  -j, --jit                      Compile code as just-in-time (in memory)\n";
  std::cout << "  -h, --help                     Display this help message.\n" ;
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
  auto ir = ir_gen.lower();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << GREEN << "[OCCULTC] Completed generating IR \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    ir_gen.visualize(ir);
  }

  start = std::chrono::high_resolution_clock::now();
  occult::jit_runtime jit_runtime(ir, debug, jit);
  jit_runtime.convert_ir();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << GREEN << "[OCCULTC] Completed converting IR to machine code \033[0m" << duration.count() << "ms\n";
  }
  /*if (debug) {
    for (const auto& pair : jit_runtime.function_map) {
      std::cout << pair.first << std::endl;
      std::cout << "0x" << std::hex << reinterpret_ccst<std::int64_t>(&pair.second) << std::dec << std::endl;
    }
  }*/
  
  using namespace occult::x86_64;

  x86_64_writer writer;
  /*writer.emit_add(rax, mem{rip, 0x1000}); // add rax, [rip + 0x1000]
  writer.emit_add(mem{rsp, rdx, 0}, rcx); // add [rsp + rdx * 1], rcx
  writer.emit_add(rcx, mem{rsp, rdx, 0, 0x1000}); // add rcx, [rsp + rdx * 1 + 0x1000]
  writer.emit_add(mem{rsp}, 10); // add [rsp], 10
  writer.emit_add(eax, INT_MAX); // add eax, INT_MAX 
  writer.emit_mov(r8d, INT_MAX); // mov r8d, INT_MAX 
  writer.emit_add(rax, rcx); // add rax, rcx
  writer.emit_add(rbx, r9); // add rbx, r9
  writer.emit_push(INT8_MAX); // push INT8_MAX
  writer.emit_push(INT32_MAX); // push INT32_MAX
  writer.emit_push(INT16_MAX); // push INT16_MAX
  writer.emit_imul(rax, rbx, 0x2000); // imul rax, rbx, 0x2000
  writer.emit_or(rcx, 0x10); // or rcx, 0x10*/
  writer.print_bytes();
  
  if (jit) {
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
  }
  else if (!jit) {
    occult::linker::link_and_create_binary(filenameout, jit_runtime.function_map, jit_runtime.function_raw_code_map, debug, showtime);
#ifdef __linux
    chmod(filenameout.c_str(), S_IRWXU);
#endif
  }
  
  return 0;
}
  
