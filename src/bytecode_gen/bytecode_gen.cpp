#include "bytecode_gen.hpp"
#include "../libs/fast_float.hpp"

namespace occult {
  void bytecode_generator::emit_function(std::unique_ptr<ast_function> function) {
    for (const auto& c : function->get_children()) {
      switch(c->get_type()) {
        case ast_type::identifier: {
          instructions.emplace_back(sigil::op_label);
          symbol_map.emplace(c->content, instructions.size()); // + 1 for the insertion of the entry point
          break;
        }
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
    
    function.release();
  }
  
  void bytecode_generator::emit_return(std::unique_ptr<ast_returnstmt> return_stmt) {
    for (const auto& c : return_stmt->get_children()) {
      switch(c->get_type()) {
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          auto val = 0;
          
          fast_float::from_chars(number->content.data(), number->content.size() + number->content.data(), val);
          number.release();
          
          instructions.emplace_back(sigil::op_push, val);
          break;
        }
        default: {
          break;
        }
      }
    }
    
    instructions.emplace_back(sigil::op_ret);
    
    return_stmt.release();
  }
  
  void bytecode_generator::emit_functionarg(std::unique_ptr<ast_functionarg> arg) {
    for (const auto& c : arg->get_children()) {
      switch(c->get_type()) {
        case ast_type::stringliteral: {
          auto string = c.get();
          
          instructions.emplace_back(sigil::op_push, string->content);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    arg.release();
  }
  
  void bytecode_generator::emit_block(std::unique_ptr<ast_block> block) {
    for (const auto& c : block->get_children()) {
      switch(c->get_type()) {
        case ast_type::returnstmt: {
          auto return_node = ast::cast<ast_returnstmt>(c.get());
          emit_return(std::move(return_node));
          break; 
        }
        case ast_type::functioncall: {
          const auto call_node = c.get();
          
          const auto func_name_node = c->get_children().front().get();
          
          if (call_node->content == "start_call") {
            if (func_name_node->content == "println") {
              auto i = 1; // starting after func name
              
              while (c->get_children().at(i)->content != "end_call") {
                auto arg_node = ast::cast<ast_functionarg>(c->get_children().at(i).get());
                
                emit_functionarg(std::move(arg_node));
                
                i++;
              }
              
              i--; // fix arguments due to end_call
              
              instructions.emplace_back(sigil::op_cout, i); // number of arguments to print
              
              break;
            }
            
            break;
          }
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
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
    
    auto instr = sigil::instruction_t(sigil::op_call, static_cast<std::intptr_t>(symbol_map["main"])); // call main
    
    instructions.insert(instructions.begin(), instr); 
    
    return instructions;
  }
  
  void bytecode_generator::visualize() {
    for (const auto& instr : instructions) {
      auto op_map = sigil::reverse_opcode_map.find(instr.op);
      const std::string& pair = op_map->second;
      std::cout << pair << " ";
      
      if (std::holds_alternative<std::intptr_t>(instr.operand1)) {
        std::cout << std::get<std::intptr_t>(instr.operand1) << std::endl;
      }
      else if (std::holds_alternative<std::string>(instr.operand1)) {
        std::cout << std::get<std::string>(instr.operand1) << std::endl;
      }
    }
  }
} // namespace occult
