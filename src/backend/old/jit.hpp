#pragma once
#include "ir_gen.hpp"
#include "x86_64_writer.hpp"

namespace occult {
  class jit_runtime {
    
  };
}
/*#include "x64writer.hpp"
#include <map>

namespace occult {  
  class jit_runtime {
    std::vector<ir_function> ir_funcs;
    std::unordered_map<std::string, std::size_t> cleanup_size_map;
    std::vector<std::unique_ptr<x64writer>> writers;
    
    bool debug;
    bool isjit;
    std::unordered_map<std::string, std::size_t> type_sizes = {
      {"int64", 8},
      {"int32", 4},
      {"int16", 2},
      {"int8", 1},
      {"uint64", 8},
      {"uint32", 3},
      {"uint16", 2},
      {"uint8", 1},
      {"float32", 16},
      {"float64", 16},
      {"bool", 1}, 
      {"char", 1},
      {"string", 8}};
      
      void generate_code(std::vector<ir_instr> ir_code, x64writer* w, std::unordered_map<std::string, std::int64_t>& local_variable_map,
                         std::unordered_map<std::string, std::size_t>& local_variable_size_map, std::size_t totalsizes, bool ismain = false);
      void compile_function(const ir_function& func);
      void backpatch_jump(ir_opcode op, std::size_t location, std::size_t label_location, x64writer* w);
  public:
    jit_runtime(std::vector<ir_function> ir_funcs, bool debug = false, bool isjit = false) : ir_funcs(ir_funcs), debug(debug), isjit(isjit) {
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
        std::cout << CYAN;
        w1->print_bytes();
        std::cout << RESET;
      }
      
      auto jit_strlen = w1->setup_function();
      
      function_map.insert({"strlen", jit_strlen});
      function_raw_code_map.insert({"strlen", w1->get_code()});
      writers.push_back(std::move(w1)); 

      //start
      auto w = std::make_unique<x64writer>();
      w->emit_function_prologue(0);
      w->emit_mov_reg_mem("rcx", "rbp", 16);
      w->emit_push_reg_64("rcx");
      w->emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&function_map["strlen"]));
      w->emit_call_reg64("rax");
      w->emit_mov_reg_reg("rdx", "rax");
      w->emit_mov_reg_imm("rax", 4);
      w->emit_mov_reg_imm("rbx", 1);
      w->push_bytes({ 0xcd, 0x80 });
      w->emit_function_epilogue();
      w->emit_ret();

      if (debug) {
          std::cout << CYAN;
          w->print_bytes();
          std::cout << RESET;
      }

      auto jit_print = w->setup_function();

      function_map.insert({"print", jit_print });
      function_raw_code_map.insert({"print", w->get_code()});
      writers.push_back(std::move(w));
      //end
    
      auto w2 = std::make_unique<x64writer>();
      w2->emit_function_prologue(0);
      w2->push_bytes({0x48, 0xC7, 0xC0, 0x2D, 0x00, 0x00, 0x00, 0x48, 0x31, 0xDB, 0xCD, 0x80, 0x48, 0x01, 0xF8, 0x48, 0x89, 0xC3, 0x48, 0xC7, 0xC0, 0x2D, 0x00, 0x00, 0x00, 0xCD, 0x80, 0x48, 0x29, 0xF8});
      w2->emit_function_epilogue();
      w2->emit_ret();
      
      if (debug) {
        std::cout << CYAN;
        w2->print_bytes();
        std::cout << RESET;
      }
      
      auto jit_stralloc = w2->setup_function();
      
      function_map.insert({"__stralloc", jit_stralloc});
      function_raw_code_map.insert({"__stralloc", w2->get_code()});
      writers.push_back(std::move(w2));
      
      auto w3 = std::make_unique<x64writer>();
      w3->emit_function_prologue(0);
      w3->emit_mov_reg_mem("rsi", "rbp", +16);
      w3->emit_xor_reg_reg("rax", "rax");
      w3->emit_xor_reg_reg("rcx", "rcx");
      auto atoi_loop_start = w3->get_code().size();
      w3->emit_xor_reg_reg("rdx", "rdx");
      w3->emit_mov_reg_mem("rdx", "rsi", 0, k8bit);
      w3->emit_test_reg_reg("rdx", "rdx");
      w3->emit_jz_short(76 + 7); // end loop
      w3->emit_cmp_reg8_64_imm8_32("rdx", '0');
      w3->emit_jl_short(76+ 7); // end loop
      w3->emit_cmp_reg8_64_imm8_32("rdx", '9');
      w3->emit_jnl_short(76 + 7); // end loop
      w3->emit_mov_reg_imm("rbx", '0');
      w3->emit_sub_reg_reg("rdx", "rbx");
      w3->emit_imul_reg_reg_imm_8_32("rax", "rax", 10);
      w3->emit_add_reg_reg("rax", "rdx");
      w3->emit_inc_reg("rsi");
      w3->emit_jmp(atoi_loop_start);
      w3->emit_function_epilogue();
      w3->emit_ret();
      
      if (debug) {
        std::cout << CYAN;
        w3->print_bytes();
        std::cout << RESET;;
      }
      
      auto jit_atoi = w3->setup_function();
      
      function_map.insert({"atoi", jit_atoi});
      function_raw_code_map.insert({"atoi", w3->get_code()});
      writers.push_back(std::move(w3));
    }
    
    ~jit_runtime() {
      function_map.clear();
      writers.clear();
    }
    
    void convert_ir();
    std::unordered_map<std::string, jit_function> function_map;
    std::map<std::string, std::vector<std::uint8_t>> function_raw_code_map;
  };
} // namespace occult
*/