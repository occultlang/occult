#pragma once
#include "opcode.hpp"
#include <vector>
#include <array>
#include <stack>
#include <iostream>

namespace sigil {
  class vm {
    std::vector<instruction_t> code;
    std::size_t pc;
    std::unordered_map<std::string, register_t> symbol_table;
    std::stack<register_t> stack;
    std::stack<std::int64_t> callstack;
    bool debug;
    
    void push(register_t value);
    register_t pop();
  public:
    vm(const std::vector<instruction_t> code, bool debug = false) : code(code), pc(main_location), debug(debug) {}
    
    int execute();
  };
}
