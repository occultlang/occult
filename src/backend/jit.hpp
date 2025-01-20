#pragma once
#include "../parser/ast.hpp"
#include "x64writer.hpp"

namespace occult {
  struct jit {
    x64writer w;
    std::unique_ptr<ast_root> root;
    std::vector<std::string> string_pool = {};
    
    void generate_prologue();
    void generate_epilogue();
    void emit_function(std::unique_ptr<ast_function> func);
    void emit_return(std::unique_ptr<ast_returnstmt> ret);
    void emit_block(std::unique_ptr<ast_block> block);
  public:
    jit(std::unique_ptr<ast_root> root, std::size_t size = 1024) : w(size), root(std::move(root)) {}
    
    jit_function generate();
  };
} // namespace occult
