#include "bytecode_generator.hpp"
#include <bitset>

namespace occult {
  std::unordered_map<occult::ast_type, int> datatype_sizes = {
    {occult::ast_type::int8_datatype, 1},
    {occult::ast_type::uint8_datatype, 1},
    {occult::ast_type::int16_datatype, 2},
    {occult::ast_type::uint16_datatype, 2},
    {occult::ast_type::int32_datatype, 4},
    {occult::ast_type::uint32_datatype, 4},
    {occult::ast_type::int64_datatype, 8},
    {occult::ast_type::uint64_datatype, 8},
    {occult::ast_type::float32_datatype, 4},
    {occult::ast_type::float64_datatype, 8},
  };
  
  void bytecode_generator::emit(opcode op, std::variant<int, std::string> operand) {
    instructions.emplace_back(op, operand);
  }
  
  void bytecode_generator::emit_label(std::string name) {
    labels[name] = instructions.size();
    labels_reverse[instructions.size()] = name;
  }
  
  void bytecode_generator::generate_bytecode(std::unique_ptr<ast> node) {
    for (auto& c : node->get_children()) {
      switch(c->get_type()) {
        case ast_type::function: {
          emit_label(c->get_children().at(0)->content); // function->identifier (name)
         
          auto& args = c->get_children().at(1)->get_children();
          for (std::size_t i = args.size(); i > 0; i--) { // function->function_args->arg
            auto& arg = args.at(i - 1);
            
            if (arg->get_type() == ast_type::string_datatype) {
              throw std::runtime_error("string not yet implemented");
            } // maybe use the string size? or we just have a push string opcode..?
            
            emit(op_push_arg, datatype_sizes[arg->get_type()]); // increase stack by the size?
          }
          
          // not sure what to do with the return type here... just going to jump straight to the body
          
          generate_bytecode(std::move(c->get_children().at(3))); // body (4th node)
          
          break;
        }
        case ast_type::functioncall: {
          if (c->content == "body_function_call") {
            
          }
          
          break;
        }
        case ast_type::returnstmt: {
          generate_bytecode(std::move(c));
          
          emit(op_ret);
          
          break;
        }
        case ast_type::number_literal: {
          emit(op_push, std::stoi(c->content.c_str()));
          
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  
  std::vector<occult_instruction_t>& bytecode_generator::get_bytecode() { // insert CALL label["main"] into the first part (first instr)
    return instructions;
  }
  
  void bytecode_generator::visualize() {
    for (auto i : instructions) {
      std::cout << std::hex << i.op << " ";
      
      if (std::holds_alternative<int>(i.operand)) {
        std::cout << std::hex << std::get<int>(i.operand) << " ";
      }
    }
    std::cout << std::dec << std::endl;
  }
  
  void bytecode_generator::visualize_code() {
    for (std::size_t index = 0; index < instructions.size(); index++) {
      auto& i = instructions[index];
      
      auto label_it = labels_reverse.find(index);
      if (label_it != labels_reverse.end()) {
        std::cout << label_it->second << ":" << std::endl; 
      }
      
      switch (i.op) {
        case op_push: {
          std::cout << "push " << std::get<int>(i.operand) << std::endl;
          break;
        }
        case op_ret: {
          std::cout << "ret" << std::endl;
          break;
        }
        default: {
          std::cout << "unknown opcode" << std::endl;
          break;
        }
      }
    }
  }
}
