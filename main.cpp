#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"
#include "parser.hpp"

int main() {
  /*std::string source = R"(
  
  // so are normal comments cool too?
  _identifieryeeea
  fn main "hi world world world" 'a'
  123423624562456
  []
  * / %
  == <= >=
  
  144.324234234
  3.14
  )";*/
  
  std::string source = "fn main(int32 a) int32 {} fn main(int32 a) int32 {}"; 

  occult::lexer lexer(source);

  std::vector<occult::token_t> stream = lexer.analyze();

  lexer.visualize();
  
  occult::parser parser(stream);
  
  auto root = parser.parse();
  
  root->visualize();
  
  return 0;
}
