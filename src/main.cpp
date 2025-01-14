#include "lexer/lexer.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "backend/x64writer.hpp"
#include "backend/elf_header.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#ifdef __linux
#include <sys/stat.h>
#endif

void display_help() {
  std::cout << "Usage: occultc [options] <source.occ>\n";
  std::cout << "Options:\n";
  std::cout << "  -t, --time                     Shows the compilation time for each stage.\n";
  std::cout << "  -d, --debug                    Enable debugging options (shows time as well -t is not needed)\n";
  std::cout << "  -h, --help                     Display this help message.\n";
}

int main(int argc, char* argv[]) {
  /*std::string input_file;
  std::string source_original;
  
  bool debug = false;
  verbose = false;
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
      std::cout << "No input file specified\n";
      display_help();
      
      return 0;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  occult::lexer lexer(source_original);
  
  std::vector<occult::token_t> stream = lexer.analyze();
  
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted lexical analysis \033[0m" << duration.count() << "ms\n";
  
  if (debug && verbose) {
    lexer.visualize();
  }

  occult::parser parser(stream);
  
  start = std::chrono::high_resolution_clock::now();
  
  auto root = parser.parse();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted parsing \033[0m" << duration.count() << "ms\n";
  
  if (debug && verbose) {
    root->visualize();
  }*/
  
  std::int64_t x = 123456;
  
  occult::x64writer writer(1024);
  writer.emit_mov_r_imm("rax", reinterpret_cast<std::int64_t>(&x)); // mov rax, &x
  writer.emit_mov_r_m("rbx", "rax", 0); // mov rbx, [rax]
  writer.emit_ret();
  
  auto func = writer.setup_function();
  func();
  
  std::int64_t rax_value = 0;
  std::int64_t rbx_value = 0;
  
  __asm__ volatile(
      "mov %%rax, %0\n"  
      "mov %%rbx, %1\n"  
      : "=r"(rax_value), "=r"(rbx_value)
      : 
      : "rax", "rbx" 
  );
  
  std::cout << "address of x (rax): " << rax_value << std::endl;
  std::cout << "value of x (rbx): " << rbx_value << std::endl;
  
/*#ifdef __linux
  occult::elf::generate_binary("a.out", writer.get_code());
  
  if (chmod("a.out", S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
    std::cerr << "failed to change permissions to binary" << std::endl;
    return 1;
  }
#endif*/
  
  return 0;
}
