#pragma once
#include "opcode_maps.hpp"
#include <stack>
#include <vector>
#include <variant>
#include <iostream>
#include <stdexcept>

namespace sigil_test {
  class vm {
    std::stack<std::variant<std::intptr_t, std::string>> stack;
    std::vector<sigil::instruction_t> instr;
    std::size_t pc; 
    
  public:
    vm(std::vector<sigil::instruction_t> instr) : instr(instr), pc(0) {}
    
    void execute() {
      while (pc < instr.size()) {
        const auto& c_instr = instr[pc];
        
        switch (c_instr.op) {
          case sigil::op_push: {
            stack.push(c_instr.operand1);
            
            break;
          }
          case sigil::op_call: {
            if (std::holds_alternative<std::intptr_t>(c_instr.operand1)) {
              pc = std::get<std::intptr_t>(c_instr.operand1);
              break;
            }
            else {
              throw std::runtime_error("Has to be integer when calling");
            }
          }
          case sigil::op_cout: {
            if (std::holds_alternative<std::intptr_t>(c_instr.operand1)) {
              auto end = std::get<std::intptr_t>(c_instr.operand1);
              
              std::vector<std::variant<std::intptr_t, std::string>> to_print;
              
              for (auto i = 0; i < end; i++) {
                to_print.emplace_back(stack.top());
                stack.pop();
              }
              
              for (const auto& p : to_print) {
                if (std::holds_alternative<std::intptr_t>(p)) {
                  std::cout << std::get<std::intptr_t>(p) << std::endl;
                }
                else if (std::holds_alternative<std::string>(p)) {
                  std::cout << std::get<std::string>(p) << std::endl;
                }
              }
              
              break;
            }
            else {
              throw std::runtime_error("Has to be integer when calling");
            }
          }
          case sigil::op_ret: {
            auto ret_val = stack.top();
            
            if (std::holds_alternative<std::intptr_t>(ret_val)) {
              if (std::get<std::intptr_t>(ret_val) == 0) {
                break; // success
              }
            }
            else if (std::holds_alternative<std::string>(ret_val)) {
              //std::cout << "return value: " << std::get<std::string>(ret_val) << std::endl;
            }
            
            break;
          }
          default: {
            break;
          }
        }
        
        pc++;
      }
    }
  };
}
