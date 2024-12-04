#pragma once
#include "../parser/ast.hpp"
#include "../sigil/vm/opcode_maps.hpp"

namespace occult {
  class bytecode_generator {
    std::unique_ptr<ast_root> root;
    std::vector<sigil::instruction_t> instructions;
    
    void emit_function(std::unique_ptr<ast_function> function);
    void emit_block(std::unique_ptr<ast_block> block);
  public:
    bytecode_generator(std::unique_ptr<ast_root>& root) : root(std::move(root)) {}
    
    const std::vector<sigil::instruction_t>& generate();
  };
} // namespace occult 
