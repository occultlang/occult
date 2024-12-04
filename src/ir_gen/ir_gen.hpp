#pragma once
#include "../parser/ast.hpp"
#include "../sigil/ir/opcode_maps.hpp"

namespace occult {
  class ir_generator {
    std::unique_ptr<ast_root> root;
    std::string ir_source;
    
    void emit_function(std::unique_ptr<ast_function> function);
    void emit_block(std::unique_ptr<ast_block> block);
  public:
    ir_generator(std::unique_ptr<ast_root>& root) : root(std::move(root)) {}
    
    const std::string& generate();
  };
} // namespace occult 
