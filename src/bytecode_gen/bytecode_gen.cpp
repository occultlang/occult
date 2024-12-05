#include "bytecode_gen.hpp"

namespace occult {
  void bytecode_generator::emit_function(std::unique_ptr<ast_function> function) {
    for (const auto& c : function->get_children()) {
      switch(c->get_type()) {
        case ast_type::identifier: {
          instructions.emplace_back(sigil::op_label);
          symbol_map.emplace(c->content, instructions.size());
          break;
        }
        case ast_type::functionarguments: {
          
        }
        default: {
          break;
        }
      }
    }
    
    function.release();
  }
  
  void bytecode_generator::emit_block(std::unique_ptr<ast_block> block) {
    block.release();
  }
  
  const std::vector<sigil::instruction_t>& bytecode_generator::generate() {
    for (const auto& c : root->get_children()) {
      switch(c->get_type()) {
        case ast_type::function: {
          auto func_node = ast::cast<ast_function>(c.get());
          emit_function(std::move(func_node));
          break;
        }
        default: {
          break;
        }
      }
    }
    
    return instructions;
  }
  
  void bytecode_generator::visualize() {
    for (const auto& instr : instructions) {
      switch(instr.op) {
        case sigil::op_label: {
          std::printf("%02X ", instr.op);
          break;
        }
        default: {
          break;
        }
      }
    }
    
    std::cout << std::endl;
  }
} // namespace occult
