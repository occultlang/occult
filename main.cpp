#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"
#include "parser.hpp"
#include "codegen.hpp"

// TODO organize files into directories
// function calls then if statements then we gucci

int main(int argc, char* argv[]) { // we're eventually going to have debug and non debug + help etc.
  std::string source = R"(
  fn main() int8 {
    int8 x = (1 + 5) * 10;
    
    return main(x, 1 + 3);
  }
  )"; 
  
  bool debug = false;
  bool verbose_lexer = false;
  bool verbose_parser = false;
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
  }
  
  occult::lexer lexer(source);
  
  std::vector<occult::token_t> stream = lexer.analyze();
  
  if (debug && verbose_lexer) {
    lexer.visualize();
  }
  
  occult::parser parser(stream);
  
  auto root = parser.parse();
  
  if (debug && verbose_parser) {
    root->visualize();
  }
  
  occult::codegen codegen(std::move(root));
  
  if (verbose_codegen) {
    // do stuff here for verbose logging
  }
  
  return 0;
}
  
