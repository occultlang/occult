#pragma once
#include "ast.hpp"

namespace occult {
  class codegen {
    std::unique_ptr<ast_root> root;
    
  public:
    codegen(std::unique_ptr<ast_root> root) : root(std::move(root)) {}
    
    // returns a bytecode blob
    char* compile();
  };
}
