#include "vm.hpp"
#include <cmath>
#include <algorithm>

namespace sigil {
  int vm::execute() { 
    while (pc < code.size()) {
      const auto& i = code.at(pc);
      
      switch (i.op) {
        case opcode::push: {
          stack.push(i.operand);
          
          break;
        }
        case opcode::pop: {
          stack.pop();
          
          break;
        }
        case opcode::load: {
          stack.push(symbol_table[std::get<std::string>(i.operand)]);
          
          break;
        }
        case opcode::store: {
          auto val = stack.top();
          stack.pop();
          
          if (symbol_table.contains(std::get<std::string>(i.operand))) { // updates the current variable
            symbol_table.erase(std::get<std::string>(i.operand));
          }
          
          symbol_table.insert({std::get<std::string>(i.operand), val});
          
          break;
        }
        case opcode::call: {
          if (pc < code.size()) {
            callstack.push(static_cast<std::int64_t>(pc + 1));
            
            auto operand = std::get<std::int64_t>(i.operand);
            
            pc = operand;
            
            continue;
          }
          else {
            throw std::runtime_error("call out of bounds");
          }
        }
        case opcode::ret: {
          if (debug) {
            std::visit([](const auto& value) {
              std::cout << "[occultc] stack top " << value << std::endl;
            }, stack.top());
          }
          
          if (!callstack.empty()) {
            pc = callstack.top();
            callstack.pop();
            continue; 
          }
          else { // have to fix this?
            if (std::holds_alternative<std::int64_t>(stack.top())) {
              return std::get<std::int64_t>(stack.top());
            }
            else {
              return 0;
            }
          }
          
          break;
        }
        case opcode::add: {
          auto top1 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          auto top2 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          stack.push(top1 + top2);
          
          break;
        }
        case opcode::sub: {
          auto top1 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          auto top2 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          stack.push(top1 - top2);
          
          break;
        }
        case opcode::mul: {
          auto top1 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          auto top2 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          stack.push(top1 * top2);
          
          break;
        }
        case opcode::div: {
          auto top1 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          auto top2 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          stack.push(top1 / top2);
          
          break;
        }
        case opcode::mod: {
          auto top1 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          auto top2 = std::get<std::int64_t>(stack.top());
          stack.pop();
          
          stack.push(top1 % top2);
          
          break;
        }
        case opcode::fadd: {
          auto top1 = std::get<double>(stack.top());
          stack.pop();
          
          auto top2 = std::get<double>(stack.top());
          stack.pop();
          
          stack.push(top1 + top2);
          
          break;
        }
        case opcode::fsub: {
          auto top1 = std::get<double>(stack.top());
          stack.pop();
          
          auto top2 = std::get<double>(stack.top());
          stack.pop();
          
          stack.push(top1 - top2);
          
          break;
        }
        case opcode::fmul: {
          auto top1 = std::get<double>(stack.top());
          stack.pop();
          
          auto top2 = std::get<double>(stack.top());
          stack.pop();
          
          stack.push(top1 * top2);
          
          break;
        }
        case opcode::fdiv: {
          auto top1 = std::get<double>(stack.top());
          stack.pop();
          
          auto top2 = std::get<double>(stack.top());
          stack.pop();
          
          stack.push(top1 / top2);
          
          break;
        }
        case opcode::fmod: {
          auto top1 = std::get<double>(stack.top());
          stack.pop();
          
          auto top2 = std::get<double>(stack.top());
          stack.pop();
          
          stack.push(std::fmod(top1, top2));
          
          break;
        }
        case opcode::cout: {
          auto amount_output = std::get<std::int64_t>(i.operand);
          
          std::vector<register_t> to_print;
          
          for (auto i = 0; i < amount_output; i++) {
            to_print.emplace_back(stack.top());
            stack.pop();
          }
          
          std::ranges::reverse(to_print);
          
          for (const auto& p : to_print) {
            if (std::holds_alternative<std::int64_t>(p)) {
              std::cout << std::get<std::int64_t>(p) << "\n";
            }
            else if (std::holds_alternative<std::string>(p)) {
              std::cout << std::get<std::string>(p) << "\n";
            }
            else if (std::holds_alternative<double>(p)) {
              std::cout << std::get<double>(p) << "\n";
            }
          }
          
          break;
        }
        case opcode::jmp: {
          pc = std::get<std::int64_t>(i.operand);
          
          continue;
        }
        case opcode::jne: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val1 != val2) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jge: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 >= val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jle: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 <= val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jg: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 > val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jl: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 < val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jz: {
          auto val = stack.top();
          stack.pop();
          
          if (std::get<int64_t>(val) == 0) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jnz: {
          auto val = stack.top();
          stack.pop();
          
          if (std::get<int64_t>(val) != 0) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jg_eq: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 >= val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        case opcode::jl_eq: {
          auto val1 = stack.top();
          stack.pop();
          auto val2 = stack.top();
          stack.pop();
          
          if (val2 <= val1) {
            pc = std::get<std::int64_t>(i.operand);
            
            continue;
          }
          
          break;
        }
        default: {
          break;
        }
      }
      
      pc++;
    }
    
    return 0;
  }
}
