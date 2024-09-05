#include "lexer.hpp"
#include <iostream>

// the main architecture i'm going to support is ARM or RISC-V at first

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
  144.324
  3.14
  )";

  occult::lexer lexer(source);

  std::vector<occult::token_t> stream = lexer.analyze();

  for (auto s : stream) {
    std::println("Lexeme: {}\nType: {}\n", s.lexeme, occult::token_t::get_typename(s.tt));
  }

  return 0;
}
