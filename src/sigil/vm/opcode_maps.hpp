#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <variant>

namespace sigil {
  enum stack_opcode : std::uint8_t { // add more later on
    op_label = 1,
    op_add,
    op_sub,
    op_mul,
    op_div,
    op_mod,
    op_call,
    op_ret,
    op_jump,
    op_copy,
    op_swap,
    op_push,
    op_pop,
    op_exit
  };
  
  const std::unordered_map<std::string, stack_opcode> stack_opcode_map = {
    {"label", op_label},
    {"add", op_add},
    {"sub", op_sub},
    {"mul", op_mul},
    {"div", op_div},
    {"mod", op_mod},
    {"call", op_call},
    {"ret", op_ret},
    {"jump", op_jump},
    {"copy", op_copy},
    {"swap", op_swap},
    {"push", op_push},
    {"pop", op_pop},
    {"exit", op_exit}
  };
  
  const std::unordered_map<stack_opcode, std::string> reverse_stack_opcode_map = {
    {op_label, "label"},
    {op_add, "add"},
    {op_sub, "sub"},
    {op_mul, "mul"},
    {op_div, "div"},
    {op_mod, "mod"},
    {op_call, "call"},
    {op_ret, "ret"},
    {op_jump, "jump"},
    {op_copy, "copy"},
    {op_swap, "swap"},
    {op_push, "push"},
    {op_pop, "pop"},
    {op_exit, "exit"}
  };

  struct stack_instruction_t {
    stack_opcode op;
    int64_t operand1; 
    
    stack_instruction_t(const stack_opcode& op, const int64_t& operand1 = 0) :
      op(op), operand1(operand1) {}
  };
} // namespace sigil
