#include "jit.hpp"
#include "../libs/fast_float.hpp"

namespace occult {
  void jit::generate_prologue() {
    w.emit_push_reg_64("rbp");
    w.emit_mov_reg_reg("rbp", "rsp"); 
  }
  
  void jit::generate_epilogue() {
    w.emit_mov_reg_reg("rsp", "rbp"); 
    w.emit_pop_reg_64("rbp");
    w.emit_ret();
  }
  
  void jit::emit_function(std::unique_ptr<ast_function> func) {
    generate_prologue();
    
    for (auto& c : func->get_children()) {
      switch (c->get_type()) {
        case ast_type::block: {
          auto break_node = ast::cast<ast_block>(c.get());
          
          emit_block(std::move(break_node));
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    func.release();
  }
  
  void jit::emit_return(std::unique_ptr<ast_returnstmt> ret) {
    for (auto& c : ret->get_children()) {
      switch(c->get_type()) {
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          std::int64_t val = 0;
          
          fast_float::from_chars(number->content.data(), number->content.size() + number->content.data(), val);
          number.release();
          
          w.emit_mov_reg_imm("rax", val);
          generate_epilogue();
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    ret.release();
  }
  
  void jit::emit_block(std::unique_ptr<ast_block> block) {
    for (auto& c : block->get_children()) {
      switch(c->get_type()) {
        case ast_type::returnstmt: {
          auto return_node = ast::cast<ast_returnstmt>(c.get());
          
          emit_return(std::move(return_node));
          
          break; 
        }
        default: {
          break;
        }
      }
    }
    
    block.release();
  }
  
  jit_function jit::generate() {
    for (auto& c : root->get_children()) {
      switch (c->get_type()) {
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
    
    return w.setup_function();
  }
} // namespace occult
