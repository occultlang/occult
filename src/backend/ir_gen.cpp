#include "ir_gen.hpp"
#include "../lexer/number_parser.hpp"

namespace occult {
  ir_function ir_gen::generate_function(ast_function* func_node) {
    ir_function function;
    
    for (const auto& c : func_node->get_children()) {
      switch(c->get_type()) {
        case ast_type::int8_datatype: 
        case ast_type::int16_datatype:
        case ast_type::int32_datatype:
        case ast_type::int64_datatype:
        case ast_type::uint8_datatype:
        case ast_type::uint16_datatype:
        case ast_type::uint32_datatype:
        case ast_type::uint64_datatype:
        case ast_type::float32_datatype:
        case ast_type::float64_datatype:
        case ast_type::string_datatype: {
          function.type = c->to_string().substr(4, c->to_string().size());

          break;
        }
        case ast_type::functionarguments: {
          generate_function_args(function, ast::cast_raw<ast_functionargs>(c.get()));
          
          break;
        }
        case ast_type::identifier: {
          function.name = c->content;
          
          break;
        }
        case ast_type::block: {
          generate_block(function, ast::cast_raw<ast_block>(c.get()));
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    return function;
  }
  
  void ir_gen::generate_function_args(ir_function& function, ast_functionargs* func_args_node) {
    for (const auto& arg : func_args_node->get_children()) {
      for (const auto& c : arg->get_children()) {
        function.args.emplace_back(c->content, arg->to_string().substr(4, arg->to_string().size()));
      }
    }
  }
  
  void ir_gen::generate_int32(ir_function& function, ast_assignment* assignment_node) {
    for (const auto& c : assignment_node->get_children()) {
      switch(c->get_type()) {
        case ast_type::number_literal: {
          function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(c->content));
          
          break;
        }
        case ast_type::add_operator: {
          function.code.emplace_back(op_add);
          
          break;
        }
        case ast_type::subtract_operator: {
          function.code.emplace_back(op_sub);
          
          break;
        }
        case ast_type::multiply_operator: {
          function.code.emplace_back(op_mul);
          
          break;
        }
        case ast_type::division_operator: {
          function.code.emplace_back(op_div);
          
          break;
        }
        case ast_type::modulo_operator: {
          function.code.emplace_back(op_mod);
          
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  
  void ir_gen::generate_block(ir_function& function, ast_block* block_node) {
    for (const auto& c : block_node->get_children()) {
      switch(c->get_type()) {
        case ast_type::int8_datatype:  {
          break;
        }
        case ast_type::int16_datatype: {
          break;
        }
        case ast_type::int32_datatype: {
          auto node = ast::cast_raw<ast_int32>(c.get());
          
          auto identifier = ast::cast_raw<ast_identifier>(node->get_children().front().get()); // name
          auto assignment = ast::cast_raw<ast_assignment>(node->get_children().back().get()); // expression stuff
          
          generate_int32(function, assignment);
          
          function.code.emplace_back(op_store, identifier->content);
          
          break;
        }
        case ast_type::int64_datatype: {
          break;
        }
        case ast_type::uint8_datatype: {
          break;
        }
        case ast_type::uint16_datatype: {
          break;
        }
        case ast_type::uint32_datatype: {
          break;
        }
        case ast_type::uint64_datatype: {
          break;
        }
        case ast_type::float32_datatype: {
          break;
        }
        case ast_type::float64_datatype: {
          break;
        }
        case ast_type::string_datatype: {
          break;
        }
        case ast_type::identifier: {
          break;
        }
        case ast_type::functioncall: {
          break;
        }
        case ast_type::ifstmt: {
          break;
        }
        case ast_type::elseifstmt: {
          break;
        }
        case ast_type::elsestmt: {
          break;
        }
        case ast_type::loopstmt: {
          break;
        }
        case ast_type::whilestmt: {
          break;
        }
        case ast_type::forstmt: {
          break;
        }
        case ast_type::returnstmt: {
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  
  std::vector<ir_function> ir_gen::generate() {
    std::vector<ir_function> functions;
    
    for (const auto& c : root->get_children()) {
      auto type = c->get_type();
      
      if (type == ast_type::function) {
        functions.emplace_back(generate_function(ast::cast_raw<ast_function>(c.get())));
      }
    }
    
    return functions;
  }
} // namespace occult
