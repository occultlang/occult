#include "jit.hpp"

// just testing JIT functionality and viability

namespace occult {
  void jit::convert_ir() {
    for (auto& func : ir_funcs) {
      auto w = std::make_unique<x64writer>(); 
      w->emit_function_prologue(0); // will be more than 0 later
      generate_code(func.code, w.get());
      //w->print_bytes();
      auto f = w->setup_function();
      
      function_map.insert({func.name, f});
      writers.push_back(std::move(w)); 
    }
  }
  
  void jit::generate_code(std::vector<ir_instr> ir_code, x64writer* w) {
    for (auto& instr : ir_code) {
      switch (instr.op) {
        case ir_opcode::op_push: {
          if (std::holds_alternative<std::string>(instr.operand)) {
            string_map.insert({std::get<std::string>(instr.operand), reinterpret_cast<std::int64_t>(std::get<std::string>(instr.operand).c_str())});
            w->emit_mov_reg_imm("rax", string_map[std::get<std::string>(instr.operand)]);
            w->emit_push_reg_64("rax");
          }
          else if (std::holds_alternative<std::int64_t>(instr.operand)) {
            w->emit_mov_reg_imm("rax", std::get<std::int64_t>(instr.operand));
            w->emit_push_reg_64("rax");
          } // more and fix stuff later on
          
          break;
        }
        case ir_opcode::op_ret: {
          w->emit_pop_reg_64("rax");
          w->emit_function_epilogue();
          w->emit_ret();
          
          break;
        }
        case ir_opcode::op_call: {
          w->emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&function_map[std::get<std::string>(instr.operand)]));
          w->emit_call_reg64("rax");
          
          break;
        }
        default: {
          break;
        }
      }
    }
  }
} // namespace occult
