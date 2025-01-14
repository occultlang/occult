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
  std::string input_file;
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
  }
  
  occult::x64writer writer(1024);
  writer.emit_mov_r_imm("rax", static_cast<std::uint64_t>(0xDEADBEEF), 64); // mov rax, 0x1000
  writer.emit_mov_r_m("rbx", "rax", 0, 64); // mov rbx, [rax]

  auto func = writer.setup_function();
  //func();
  
  /*uint64_t rbx_value;
  
  __asm__ volatile(
      "mov %%rax, %0"  
      : "=r"(rbx_value)
      :
      : "%rax"      
  );
  
  std::cout << "value in rax: " << rbx_value << std::endl;*/
  
/*#ifdef __linux
  occult::elf::generate_binary("a.out", writer.get_code());
  
  if (chmod("a.out", S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
    std::cerr << "failed to change permissions to binary" << std::endl;
    return 1;
  }
#endif*/
  
  return 0;
}
