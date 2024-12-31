#include "bytecode_gen.hpp"
#include "../libs/fast_float.hpp"
#include <iomanip>

/*
 * NOTE TO SELF:
 * If there is a segfault, it is probably because of a forgetful std::unique_ptr<T>.release();
 * So just double check all of the initialization of std::unique_ptr<T> before debugging
 */

namespace occult {
  void bytecode_generator::emit_function(std::unique_ptr<ast_function> function) {
      for (auto& c : function->get_children()) {
      switch(c->get_type()) {
        case ast_type::functionarguments: {
          auto funcargs_node = ast::cast<ast_functionargs>(c.get());
          
          auto func_name_node = function->get_children().front().get();
          
          emit_functionargs(std::move(funcargs_node), func_name_node->content); // this breaks
          
          break;
        }
        case ast_type::block: {
          auto break_node = ast::cast<ast_block>(c.get());
          
          auto func_name_node = c->get_children().front().get();
          
          emit_block(std::move(break_node), func_name_node->content);
          
          break;
        }
        case ast_type::identifier: {
          symbol_map.emplace(c->content, instructions.size());
          
          break;
        }
        default: { // we need type checking for return types (but this could be with the static analyzer?)
          break;
        }
      }
    }
    
    function.release();
  }
  
  void bytecode_generator::emit_functionargs(std::unique_ptr<ast_functionargs> functionargs, const std::string func_name) {
    std::vector<std::string> args;
    
    for (auto& c : functionargs->get_children()) {
      switch(c->get_type()) {
        case ast_type::int64_datatype: { // add more later on as we get more type checking
          const auto i64_name_node = c->get_children().front().get();
          
          //instructions.emplace_back(sigil::opcode::push, 0);
          instructions.emplace_back(sigil::opcode::store, i64_name_node->content);
          
          if (symbol_args_map.contains(func_name)) {
            args = symbol_args_map[func_name];
            symbol_args_map.erase(func_name);
          }
          
          args.emplace_back(i64_name_node->content);
          
          break;
        }
        case ast_type::string_datatype: { 
          const auto string_node = c->get_children().front().get();
          
          //instructions.emplace_back(sigil::opcode::push, "");
          instructions.emplace_back(sigil::opcode::store, string_node->content);
          
          if (symbol_args_map.contains(func_name)) {
            args = symbol_args_map[func_name];
            symbol_args_map.erase(func_name);
          }
          
          args.emplace_back(string_node->content);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    symbol_args_map.insert({func_name, args});
    
    functionargs.release();
  }
  
  void bytecode_generator::emit_return(std::unique_ptr<ast_returnstmt> return_stmt) {
    for (auto& c : return_stmt->get_children()) {
      switch(c->get_type()) {
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          std::int64_t val = 0;
          
          fast_float::from_chars(number->content.data(), number->content.size() + number->content.data(), val);
          number.release();
          
          instructions.emplace_back(sigil::opcode::push, val);
          
          break;
        }
        case ast_type::stringliteral: {
          auto str = ast::cast<ast_stringliteral>(c.get());
          
          instructions.emplace_back(sigil::opcode::push, str->content);
          
          str.release();
          
          break;
        }
        case ast_type::identifier: {
          auto identifier = ast::cast<ast_identifier>(c.get());
          
          instructions.emplace_back(sigil::opcode::load, identifier->content);
          
          identifier.release();
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    instructions.emplace_back(sigil::opcode::ret, 0);
    
    return_stmt.release();
  }
  
  void bytecode_generator::emit_functionarg(std::unique_ptr<ast_functionarg> arg) {
    for (auto& c : arg->get_children()) {
      switch(c->get_type()) {
        case ast_type::stringliteral: {
          auto string = c.get();
          
          instructions.emplace_back(sigil::opcode::push, string->content);
          
          break;
        }
        case ast_type::number_literal: {
          auto number = ast::cast<ast_numberliteral>(c.get());
          std::int64_t val = 0;
          
          fast_float::from_chars(number->content.data(), number->content.size() + number->content.data(), val);
          number.release();
          
          instructions.emplace_back(sigil::opcode::push, val);
          
          break;
        }
        case ast_type::identifier: {
          instructions.emplace_back(sigil::opcode::load, c->content);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    arg.release();
  }
  
  void bytecode_generator::emit_block(std::unique_ptr<ast_block> block, const std::string func_name) {
    std::vector<std::string> args;
    
    for (auto& c : block->get_children()) {
      switch(c->get_type()) {
        case ast_type::returnstmt: {
          auto return_node = ast::cast<ast_returnstmt>(c.get());
          
          emit_return(std::move(return_node));
          
          break; 
        }
        case ast_type::string_datatype: { // string x = "x";
          const auto string_node = c->get_children().front().get();
          
          if (c->get_children().back().get()->get_type() == ast_type::stringliteral) {
            instructions.emplace_back(sigil::opcode::push, c->get_children().back().get()->content);
          }
          else if (auto node = c->get_children().back().get(); node->get_type() == ast_type::functioncall && node->content == "start_call") {
            auto func_call_name = node->get_children().front().get()->content;
            
            for (auto& c : node->get_children()) {
              if (c->get_type() == ast_type::functionargument) {
                auto funcarg_node = ast::cast<ast_functionarg>(c.get());
                
                emit_functionarg(std::move(funcarg_node));
              }
            }
            
            instructions.emplace_back(sigil::opcode::call, static_cast<std::int64_t>(symbol_map[func_call_name] + 1));
          }
          
          instructions.emplace_back(sigil::opcode::store, string_node->content);
          
          if (symbol_args_map.contains(func_name)) { // updating the list
            args = symbol_args_map[func_name];
            symbol_args_map.erase(func_name);
          }
          
          args.emplace_back(string_node->content);
          
          break;
        }
        case ast_type::functioncall: {
          const auto call_node = c.get();
          
          const auto func_name_node = c->get_children().front().get();
          
          if (call_node->content == "start_call") {
            if (func_name_node->content == "println") {
              std::int64_t i = 1; // starting after func name
              
              while (c->get_children().at(i)->content != "end_call") {
                auto arg_node = ast::cast<ast_functionarg>(c->get_children().at(i).get());
                
                emit_functionarg(std::move(arg_node));
                
                i++;
              }
              
              i--; // fix arguments due to end_call
              
              instructions.emplace_back(sigil::opcode::cout, i); // number of arguments to print
              
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
    
    symbol_args_map.insert({func_name, args});
    
    block.release();
  }
  
  const std::vector<sigil::instruction_t>& bytecode_generator::generate() {
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
    
    // this is temporary until we actually resolve proper arguments in main
    
    if (symbol_map.size() == 1) {
      sigil::main_location = symbol_map["main"] + 1;
    }
    else {
      auto instr = sigil::instruction_t(sigil::call, static_cast<std::intptr_t>(symbol_map["main"] + 1)); // call main
      
      instructions.insert(instructions.begin(), instr); 
    }
    
    return instructions;
  }
    
  void bytecode_generator::visualize() {
    for (auto& i : instructions) {
      i.print();
    }
  }
} // namespace occult
