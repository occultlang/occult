#pragma once
#include "../parser/ast.hpp"
#include "ir.hpp"

namespace occult {
  class ir_generator {
    std::unique_ptr<ast_root> root;
    std::vector<occult::instruction_t> instructions;
    std::unordered_map<std::string, std::size_t> symbol_map;
    
    void emit_function(std::unique_ptr<ast_function> function);
    void emit_functionargs(std::unique_ptr<ast_functionargs> functionargs);
    void emit_return(std::unique_ptr<ast_returnstmt> return_stmt);
    void emit_functionarg(std::unique_ptr<ast_functionarg> arg);
    void emit_if(std::unique_ptr<ast_ifstmt> if_stmt);
    void emit_block(std::unique_ptr<ast_block> block);
  public:
    ir_generator(std::unique_ptr<ast_root>& root) : root(std::move(root)) {}
    
    const std::vector<occult::instruction_t>& generate();
    void visualize(); 
  };
} // namespace occult 
