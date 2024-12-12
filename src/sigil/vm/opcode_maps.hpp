#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>

namespace sigil {
  enum opcode : std::uint8_t { // add more later on
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
    op_cout,
    op_cin,
    op_exit
  };
  
  const std::unordered_map<std::string, opcode> opcode_map = {
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
    {"cout", op_cout},
    {"cin", op_cin},
    {"exit", op_exit}
  };
  
  const std::unordered_map<opcode, std::string> reverse_opcode_map = {
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
    {op_cout, "cout"},
    {op_cin, "cin"},
    {op_exit, "exit"}
  };

  struct instruction_t {
    opcode op;
    std::variant<std::intptr_t, std::string> operand1;
    
    instruction_t(const opcode& op, std::optional<std::variant<std::intptr_t, std::string>> operand1 = std::nullopt) : op(op) {
      if (operand1.has_value()) {
        this->operand1 = operand1.value();
      }
    }
  };
} // namespace sigil
