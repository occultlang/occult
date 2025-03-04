#pragma once
#include "ir_gen.hpp"
#include "x64writer.hpp"

namespace occult {
  const std::unordered_map<std::string, std::size_t> type_sizes = {
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
    
  class jit {
    std::vector<ir_function> ir_funcs;
    std::vector<std::unique_ptr<x64writer>> writers;
    bool debug;
  public:
    jit(std::vector<ir_function> ir_funcs, bool debug = false) : ir_funcs(ir_funcs), debug(debug) {
      auto w = std::make_unique<x64writer>(); 
      w->emit_function_prologue(0);
      w->emit_mov_reg_imm("rax", 1);
      w->emit_mov_reg_imm("rdi", 1);
      w->emit_mov_reg_mem("rdx", "rbp", 16);
      w->emit_mov_reg_mem("rsi", "rbp", 24);
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
    
    void convert_ir();
    void generate_code(std::vector<ir_instr> ir_code, x64writer* w);
    void compile_function(const ir_function& func);
    std::unordered_map<std::string, jit_function> function_map = {};
    std::unordered_map<std::string, std::int64_t> string_map = {};
  };
} // namespace occult
