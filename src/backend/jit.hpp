#pragma once
#include "ir_gen.hpp"
#include "x64writer.hpp"

namespace occult {  
  class jit_runtime {
    std::vector<ir_function> ir_funcs;
    std::vector<std::unique_ptr<x64writer>> writers;
    bool debug;
    std::unordered_map<std::string, std::int64_t> string_map = {};
    std::unordered_map<std::string, std::size_t> type_sizes = {
      {"int64", 8},
      {"int32", 4},
      {"int16", 2},
      {"int8", 1},
      {"uint64", 8},
      {"uint32", 3},
      {"uint16", 2},
      {"uint8", 1},
      {"float32", 4},
      {"float64", 8},
      {"bool", 1}, 
      {"char", 1}};
      
      void generate_code(std::vector<ir_instr> ir_code, x64writer* w);
      void compile_function(const ir_function& func);
      void backpatch_jump(ir_opcode op, std::size_t location, std::size_t label_location, x64writer* w);
  public:
    jit_runtime(std::vector<ir_function> ir_funcs, bool debug = false) : ir_funcs(ir_funcs), debug(debug) {
      auto w1 = std::make_unique<x64writer>();
      w1->emit_function_prologue(0);
      w1->emit_mov_reg_mem("rdi", "rbp", 16); // first arg
      w1->emit_xor_reg_reg("rax", "rax");
      auto loc = w1->get_code().size();
      w1->push_bytes({0x80, 0x3C, 0x07, 0x00});
      w1->emit_jz_short(42); // jmp if done
      w1->emit_mov_reg_imm("rbx", 1);
      w1->emit_add_reg_reg("rax", "rbx");
      w1->emit_short_jmp(loc);
      w1->emit_function_epilogue();
      w1->emit_ret();
      
      if (debug) {
        w1->print_bytes();
      }
      
      auto jit_strlen = w1->setup_function();
      
      function_map.insert({"strlen", jit_strlen});
      writers.push_back(std::move(w1)); 
      
      auto w = std::make_unique<x64writer>(); 
      w->emit_function_prologue(0);
      w->emit_mov_reg_mem("rsi", "rbp", 16);
      w->emit_mov_reg_reg("rdx", "rsi");
      w->emit_push_reg_64("rdx");
      w->emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&function_map["strlen"]));
      w->emit_call_reg64("rax");
      w->emit_mov_reg_reg("rdx", "rax");
      w->emit_mov_reg_imm("rax", 1);
      w->emit_mov_reg_imm("rdi", 1);
      w->emit_syscall();
      w->emit_function_epilogue();
      w->emit_ret();
      
      if (debug) {
        w->print_bytes();
      }
      
      auto jit_print = w->setup_function();

      function_map.insert({"print", jit_print});
      writers.push_back(std::move(w)); 
    }
    
    ~jit_runtime() {
      function_map.clear();
      string_map.clear();
      writers.clear();
    }
    
    void convert_ir();
    std::unordered_map<std::string, jit_function> function_map;
  };
} // namespace occult
