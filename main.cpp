#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include "bytecode_generator.hpp"

// TODO organize files into directories

void display_help() {
    std::println("Usage: occultc [options] <source.occ>");
    
    std::println("Options:");
    std::println("  -t, --time                     Shows the compilation time for each stage.");
    std::println("  -d, --debug [verbose_option]   Enable debugging options (shows time as well -t is not needed):\n"
    "                                 verbose_lexer | verbose_parser | verbose_codegen | verbose");
    
    std::println("  -h, --help                     Display this help message.");
}

int main(int argc, char* argv[]) {
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  bool verbose_lexer = false;
  verbose_parser = false;
  bool showtime = false;
  
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "-d" || arg == "--debug") {
      debug = true;
      
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        ++i;
        
        std::string debug_option = argv[i];
        
        if (debug_option == "verbose") {
          verbose_lexer = true;
          verbose_parser = true;
          showtime = true;
        }
      }
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
  
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted lexical analysis \033[0m" << duration.count() << "ms" << std::endl;
  
  std::vector<occult::token_t> stream = lexer.analyze();
  
  if (debug && verbose_lexer) {
    lexer.visualize();
  }
  
  occult::parser parser(stream);
  
  start = std::chrono::high_resolution_clock::now();
  
  auto root = parser.parse();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime)
    std::cout << "[occultc] \033[1;36mcompleted parsing \033[0m" << duration.count() << "ms" << std::endl;
  
  if (debug && verbose_parser) {
    root->visualize();
  }
  
  occult::bytecode_generator bg;
  
  start = std::chrono::high_resolution_clock::now();
  
  bg.generate_bytecode(std::move(root));
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  if (showtime) {
    std::cout << "[occultc] \033[1;36mcompleted bytecode generation \033[0m" << duration.count() << "ms" << std::endl;
    std::cout << "[occultc] \033[1;36mbytecode: \033[0m";
    bg.visualize();
    std::cout << "[occultc] \033[1;36mbytecode visualization: \033[0m\n";
    bg.visualize_code();
  }
  
  return 0;
}
  
