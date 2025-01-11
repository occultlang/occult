#include "lexer/lexer.hpp"
#include <iostream>
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "backend/ir_gen.hpp"
#include "backend/x86_writer.hpp"

#include "backend/elf_header.hpp"

#include <fstream>
#include <sstream>
#include <chrono>

void display_help() {
  std::println("Usage: occultc [options] <source.occ>");
  std::println("Options:");
  std::println("  -t, --time                     Shows the compilation time for each stage.");
  std::println("  -d, --debug Enable debugging options (shows time as well -t is not needed):\n");
  std::println("  -h, --help                     Display this help message.");
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
      std::println("No input file specified");
      display_help();
      
      return 0;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  occult::lexer lexer(source_original);
  
  std::vector<occult::token_t> stream = lexer.analyze();
  
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted lexical analysis \033[0m" << duration.count() << "ms" << std::endl;
  
  if (debug && verbose) {
    lexer.visualize();
  }

  occult::parser parser(stream);
  
  start = std::chrono::high_resolution_clock::now();
  
  auto root = parser.parse();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted parsing \033[0m" << duration.count() << "ms" << std::endl;
  
  if (debug && verbose) {
    root->visualize();
  }

  occult::ir_generator ir(root);
  
  start = std::chrono::high_resolution_clock::now();
  
  auto code = ir.generate();

  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted generating bytecode \033[0m" << duration.count() << "ms" << std::endl;
  
  if (debug && verbose) {
    ir.visualize();
    std::cout << std::endl;
  }
  
  occult::x86_writer writer(1024);
  writer.push_bytes({
      0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,                                            // mov rax, 0x01 (sys_write)
      0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,                                            // mov rdi, 0x01 (stdout)
      0x48, 0x8D, 0x35, 0x14, 0x00, 0x00, 0x00,                                            // lea rsi, [rip + offset] (string address)
      0x48, 0xC7, 0xC2, 19, 0x00, 0x00, 0x00,                                            // mov rdx, 0x0E (string length)
      0x0F, 0x05,                                                                          // syscall
      0x31, 0xFF,                                                                          // xor edi, edi
      0xB8, 0x3C, 0x00, 0x00, 0x00,                                                        // mov eax, 60
      0x0F, 0x05                                                                           // syscall                                                
  });
  
  writer.push_bytes(writer.string_to_bytes("im gay as fuck bro\n"));
  
  auto jit_function = writer.setup_function(); 
  
  //jit_function();
 
    elf::generate_binary("program_something", writer.code);

  return 0;
}
