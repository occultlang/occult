#include "jit.hpp"

// just testing JIT functionality and viability

namespace occult {
  void jit::convert_ir() {
    for (auto& func : ir_funcs) {
      compile_function(func);
    }
  }

  void jit::compile_function(const ir_function& func) {
    if (function_map.contains(func.name)) {
      return;
    }

    auto w = std::make_unique<x64writer>();
    w->emit_function_prologue(0); // future stack size

    generate_code(func.code, w.get());

    if (debug) {
      w->print_bytes();
    }

    auto f = w->setup_function();

    function_map.insert({func.name, f});
    writers.push_back(std::move(w));
  }

  void jit::generate_code(std::vector<ir_instr> ir_code, x64writer* w) {
    std::unordered_map<std::string, std::size_t> local_variable_map;
    std::size_t totalsizes = 0;
    
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
        case ir_opcode::op_store: { // not done
          totalsizes += type_sizes[instr.type];
          
          if (instr.type != "string") {
            w->emit_sub_reg8_64_imm8_32("rsp", totalsizes);
            w->emit_mov_mem_reg("rbp", -totalsizes, "rax");
          }
          
          if (debug) {
            std::cout << "sizes in store: " << totalsizes << "\nlocal: " << type_sizes[instr.type] << "\n";
          }
          
          local_variable_map.insert({std::get<std::string>(instr.operand), totalsizes});
          
          break;
        }
        case ir_opcode::op_load: { // not done 
          if (instr.type != "string") {
            w->emit_mov_reg_mem("rax", "rbp", -local_variable_map[std::get<std::string>(instr.operand)]);
            w->emit_push_reg_64("rax");
          }
          
          break;
        }
        case ir_opcode::op_ret: {
          w->emit_pop_reg_64("rax");
          w->emit_function_epilogue();
          w->emit_ret();
          
          break;
        }
        case ir_opcode::op_add: {
          w->emit_pop_reg_64("rax");
          w->emit_mov_reg_reg("rbx", "rax");
          w->emit_pop_reg_64("rax");
          w->emit_add_reg_reg("rax", "rbx");
          w->emit_push_reg_64("rax");
          
          break;
        }
        case ir_opcode::op_sub: {
          w->emit_pop_reg_64("rax");
          w->emit_mov_reg_reg("rbx", "rax");
          w->emit_pop_reg_64("rax");
          w->emit_sub_reg_reg("rax", "rbx");
          w->emit_push_reg_64("rax");
          
          break;
        }
        case ir_opcode::op_call: {
          std::string func_name = std::get<std::string>(instr.operand);
          
          if (debug) {
            std::cout << "calling: " << func_name << "\n";
          }
          
          auto it = std::find_if(ir_funcs.begin(), ir_funcs.end(), [&](const ir_function& f) {
            return f.name == func_name;
          });

          if (it != ir_funcs.end()) {
            compile_function(*it); 
          }

          if (!function_map.contains(func_name)) {
            throw std::runtime_error("undefined function: " + func_name);
          }
          
          w->emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&function_map[func_name]));
          w->emit_call_reg64("rax");
          
          if (func_name != "print") {
            w->emit_push_reg_64("rax"); // push return value onto stack if not print
          }
          
          break;
        }

        default: {
          break;
        }
      }
    }
  }
} // namespace occult
