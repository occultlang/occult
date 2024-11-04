#pragma once
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "parser.hpp"

namespace occult {
  enum opcode {
    op_nop,
    op_push,
    op_pop,
    op_call,
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
  };
}
