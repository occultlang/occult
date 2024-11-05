#pragma once
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "parser.hpp"
#include <iomanip>

namespace occult {
  enum opcode {
    op_nop,
    op_push,
    op_pop,
    op_call,
    op_push_arg,
    op_ret
  };
  
  struct occult_instruction_t {
    opcode op;
    std::variant<int, std::string> operand;
    
    occult_instruction_t(opcode op, std::variant<int, std::string> operand = 0) : op(op), operand(operand) {}
  };
  
  class bytecode_generator {
    std::vector<occult_instruction_t> instructions;
    std::unordered_map<std::string, int> labels;
    std::unordered_map<std::size_t, std::string> labels_reverse;
    
    void emit(opcode op, std::variant<int, std::string> operand = 0);
    void emit_label(std::string name);
  public:
    void generate_bytecode(std::unique_ptr<ast> node);
    std::vector<occult_instruction_t>& get_bytecode();
    void visualize();
    void visualize_code();
  };
}
