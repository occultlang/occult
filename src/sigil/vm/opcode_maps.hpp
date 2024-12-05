#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <optional>

namespace sigil {
  enum opcode : std::uint8_t { // add more later on
    op_mov,
    op_add,
    op_sub,
    op_mul,
    op_div,
    op_mod,
    op_call,
    op_ret,
    op_label,
  };
  
  const std::unordered_map<std::string, opcode> opcode_map = {
    {"label", op_label},
    {"mov", op_mov},
    {"add", op_add},
    {"sub", op_sub},
    {"mul", op_mul},
    {"div", op_div},
    {"mod", op_mod},
    {"call", op_call},
    {"ret", op_ret},
  };
  
  const std::unordered_map<opcode, std::string> reverse_opcode_map = {
    {op_label, "label"},
    {op_mov, "mov"},
    {op_add, "add"},
    {op_sub, "sub"},
    {op_mul, "mul"},
    {op_div, "div"},
    {op_mod, "mod"},
    {op_call, "call"},
    {op_ret, "ret"}
  };
  
  struct instruction_t {
    opcode op;
    std::intptr_t operand1; // reg, mval
    std::intptr_t operand2; // reg, imm
    
    instruction_t(const opcode& op, const std::intptr_t& operand1 = 0, const std::intptr_t& operand2 = 0) :
      op(op), operand1(operand1), operand2(operand2) {}
  };
} // namespace sigil
