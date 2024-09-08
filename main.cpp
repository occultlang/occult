#include "lexer.hpp"
#include <iostream>
#include "ast.hpp"

int main() {
  std::string source = R"(
  /*multiline
  comments are
  cool */
  
  // so are normal comments cool too?
  _identifieryeeea
  fn main "hi world world world" 'a'
  123423624562456
  []
  * / %
  == <= >=
  
  144.324234234
  3.14
  )";

  occult::lexer lexer(source);

  std::vector<occult::token_t> stream = lexer.analyze();

  lexer.visualize();
  
  auto root = occult::ast::new_node<occult::ast_root>(); // new root node
  
  root->add_child(occult::ast::new_node<occult::ast_binaryexpr>()); // create new node
  
  root->visualize();
  
  return 0;
}
