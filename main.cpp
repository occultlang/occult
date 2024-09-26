#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"
#include "parser.hpp"

// TODO organize files into directories

int main() {
  std::string source = R"(
  fn func_call(int32 y) int32 {
    return y + 6;
  }
  
  fn test() int32 {
    return func_call(3);
  }
  
  fn main() {
    int32 tvar = 3;
    int32 foo = 43 9 +;
    func_call(5);
    int32 x = func_call(6) 4 +;
    int32 y = test() 9 %;
  }
  )"; // The goal is to parse this fully, right now if we get everything in the TODO done, we can.
  
  std::println("{}\n", source);
  
  occult::lexer lexer(source);

  std::vector<occult::token_t> stream = lexer.analyze();

  lexer.visualize();
  
  occult::parser parser(stream);
  
  auto root = parser.parse();
  
  root->visualize();
  
  return 0;
}
  
