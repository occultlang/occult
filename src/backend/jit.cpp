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
    auto after_prologue = w.get_code().size();
    
    for (auto& c : func->get_children()) {
      switch (c->get_type()) {
        case ast_type::block: {
          auto break_node = ast::cast<ast_block>(c.get());
          
          emit_block(std::move(break_node));
          
          break;
        }
        case ast_type::identifier: {
          symbol_map.emplace(c->content, w.get_code().size() - after_prologue); // - prologue size
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    func.release();
  }
  
  void jit::emit_return(std::unique_ptr<ast_returnstmt> ret, std::unordered_map<std::string, std::intptr_t> variable_location) {
    for (auto& c : ret->get_children()) {
      switch(c->get_type()) {
        case ast_type::identifier: {
          w.emit_mov_reg_mem("rax", "rbp", variable_location[c.get()->content]);
          generate_epilogue();
          
          break;
        }
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          std::int64_t val = 0; // handle unsigned eventually
          
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
  
  void jit::emit_assignment(std::unique_ptr<ast_assignment> assignment, std::intptr_t var_loc, bool _signed) {
    for (std::size_t i = assignment->get_children().size() - 1; i > 0; i--) {
      auto& c = assignment->get_children().at(i);
      
      switch(c->get_type()) {
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          std::int64_t val = 0; // have to handle unsigned eventually
          
          fast_float::from_chars(number->content.data(), number->content.size() + number->content.data(), val);
          number.release();
          
          w.emit_mov_mem_imm("rbp", var_loc, val, k32bit); // 32 bit for now
          
          break;
        }
        case ast_type::add_operator: {
          auto left = ast::cast<ast_numberliteral>(assignment->get_children().at(i - 1).get());
          std::int64_t leftval = 0; // have to handle unsigned eventually
          
          fast_float::from_chars(left->content.data(), left->content.size() + left->content.data(), leftval);
          left.release();
          
          auto right = ast::cast<ast_numberliteral>(assignment->get_children().at(i - 2).get());
          std::int64_t rightval = 0; // have to handle unsigned eventually
          
          fast_float::from_chars(right->content.data(), right->content.size() + right->content.data(), rightval);
          right.release();
          
          i--; // why only once? i cant tell ya
          
          w.emit_mov_reg_imm("rax", leftval);
          w.emit_add_accumulator_imm_32_8(rightval);
          w.emit_mov_mem_reg("rbp", var_loc, "rax");
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    assignment.release();
  }
  
  void jit::emit_block(std::unique_ptr<ast_block> block) {
    std::unordered_map<std::string, std::size_t> variable_scope = {};
    std::unordered_map<std::string, std::intptr_t> variable_location = {};
    
    for (auto& c : block->get_children()) {
      switch(c->get_type()) {
        case ast_type::returnstmt: {
          auto return_node = ast::cast<ast_returnstmt>(c.get());
          
          emit_return(std::move(return_node), variable_location);
          
          break; 
        }
        case ast_type::int32_datatype: {
          w.emit_sub_reg8_64_imm8_32("rsp", 4); // variable size (8)
          
          auto name = c->get_children().front().get()->content;
          
          if (!variable_scope.contains(name)) {
            variable_scope.insert({name, 4});
          }
          
          if (!variable_location.contains(name)) {
            variable_location.insert({name, -((1 + variable_location.size()) * 4)});
          }
          
          auto assignment = ast::cast<ast_assignment>(c->get_children().back().get());
          
          emit_assignment(std::move(assignment), variable_location[name]);
          
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
