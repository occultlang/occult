#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include <fstream>
#include <sstream>
#include <chrono>

// TODO organize files into directories

void display_help() {
    std::println("Usage: occultc [options] <source.occ>");
    
    std::println("Options:");
    
    std::println("  -d, --debug [verbose_option]  Enable debugging options:\n"
    "                                 verbose_lexer | verbose_parser | verbose_codegen | verbose");
    
    std::println("  -h, --help                     Display this help message.");
}

int main(int argc, char* argv[]) {
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  bool verbose_lexer = false;
  verbose_parser = false;
  bool verbose_codegen = false;
  
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "-d" || arg == "--debug") {
      debug = true;
      
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        ++i;
        
        std::string debug_option = argv[i];
        
        if (debug_option == "verbose_lexer") {
          verbose_lexer = true;
        }
        else if (debug_option == "verbose_parser") {
          verbose_parser = true;
        }
        else if (debug_option == "verbose_codegen") {
          verbose_codegen = true;
        }
        else if (debug_option == "verbose") {
          verbose_lexer = true;
          verbose_parser = true;
          verbose_codegen = true;
        }
      }
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
  
  std::cout << "[occultc] \033[1;36mcompleted parsing \033[0m" << duration.count() << "ms" << std::endl;
  
  if (debug && verbose_parser) {
    root->visualize();
  }
  
  occult::codegen codegen(std::move(root));
  
  if (verbose_codegen) {
    // do stuff here for verbose logging
  }
  
  return 0;
}
  
