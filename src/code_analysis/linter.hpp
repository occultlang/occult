#pragma once
#include "../lexer/lexer.hpp"

namespace occult {
  class linter {
    std::vector<token_t> stream;
    
  public:
    linter(const std::vector<token_t>& stream) : stream(stream) {}
    
    bool analyze();
  };
} // namespace occult
