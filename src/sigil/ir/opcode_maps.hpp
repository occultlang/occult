#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>

namespace sigil {
  enum sigil_opcode : std::uint8_t { // add more later on
    op_label,
    op_mov,
    op_add,
    op_sub,
    op_mul,
    op_div,
    op_mod,
    op_call,
    op_ret
  };
  
  std::unordered_map<std::string, sigil_opcode> opcode_map = {
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
} // namespace sigil
