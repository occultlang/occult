#pragma once
#include "../parser/ast.hpp"
#include "x64writer.hpp"

namespace occult {
  struct jit {
    x64writer w;
    std::unique_ptr<ast_root> root;
    std::vector<std::string> string_pool = {};
    std::unordered_map<std::string, std::size_t> symbol_map = {};
    
    void generate_prologue();
    void generate_epilogue();
    void emit_function(std::unique_ptr<ast_function> function);
    void emit_functionargs(std::unique_ptr<ast_functionargs> functionargs);
    void emit_return(std::unique_ptr<ast_returnstmt> return_stmt, std::unordered_map<std::string, std::intptr_t> variable_location);
    void emit_functionarg(std::unique_ptr<ast_functionarg> arg);
    void emit_if(std::unique_ptr<ast_ifstmt> if_stmt);
    void emit_assignment(std::unique_ptr<ast_assignment> assignment, std::intptr_t var_loc, bool _signed = false);
    void emit_block(std::unique_ptr<ast_block> block);
  public:
    jit(std::unique_ptr<ast_root> root, std::size_t size = 1024) : w(size), root(std::move(root)) {}
    
    jit_function generate();
  };
} // namespace occult
