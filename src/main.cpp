#include "lexer/lexer.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "backend/jit.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#ifdef __linux
#include <sys/stat.h>
#include "backend/elf_header.hpp"
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
  
  /*occult::x64writer writer(1024);
  writer.emit_push_reg_64("rbp");               // function prologue
  writer.emit_mov_reg_reg("rbp", "rsp");
  
  writer.emit_sub_reg8_64_imm8_32("rsp", 8);    // stack space for 1 64-bit integer
  
  writer.emit_mov_mem_imm("rbp", -8, 26);       // create new integer (8 bytes) and set it to 25
  
  writer.emit_sub_reg8_64_imm8_32("rsp", 8); 
  writer.emit_mov_mem_imm("rbp", -16, 25);
  
  writer.emit_mov_reg_imm("rax", 25);           // add 25 to it
  
  writer.emit_cmp_mem_reg("rbp", -16, "rax");
  writer.emit_jz_short(111);
  
  writer.emit_mov_reg_imm("rax", 1);
  writer.emit_mov_reg_imm("rdi", 1);
  writer.emit_mov_reg_imm("rsi", reinterpret_cast<std::int64_t>("not same value\n"));
  writer.emit_mov_reg_imm("rdx", 15);
  writer.emit_syscall();
  writer.emit_short_jmp(153);
  
  std::cout << writer.get_code().size() << std::endl;
  
  writer.emit_mov_reg_imm("rax", 1);
  writer.emit_mov_reg_imm("rdi", 1);
  writer.emit_mov_reg_imm("rsi", reinterpret_cast<std::int64_t>("same value\n"));
  writer.emit_mov_reg_imm("rdx", 11);
  writer.emit_syscall();
  
  std::cout << writer.get_code().size() << std::endl;
  
  writer.emit_mov_reg_reg("rsp", "rbp");         // epilogue
  writer.emit_pop_reg_64("rbp");
  writer.emit_ret();
  
  auto func = writer.setup_function();
  func();*/
  
  start = std::chrono::high_resolution_clock::now();
  
  occult::jit jit(std::move(root));
  
  auto func = jit.generate();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted jit compilation \033[0m" << duration.count() << "ms\n";
  
  if (debug) {
    std::cout << "machine code: ";
    for (auto byte : jit.w.get_code()) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<std::intptr_t>(byte) << std::dec << " ";
    }
    std::cout << "\n\n";
  }
  
  std::cout << "return value of jit function: " << func() << std::endl;
/*#ifdef __linux
  occult::elf::generate_binary("a.out", writer.get_code());
  
  if (chmod("a.out", S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
    std::cerr << "failed to change permissions to binary" << std::endl;
    return 1;
  }
#endif*/
  
  return 0;
}
