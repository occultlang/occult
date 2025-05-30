#include "jit.hpp"
#include <fstream>

// variables buggy with for loop (vars initialized before the for) 
// TODO: change sizes of variables in IR and make x64writer more verbose
/*
namespace occult {
  void jit_runtime::convert_ir() {
    for (auto& func : ir_funcs) {
      compile_function(func);
    }
  }

  void jit_runtime::compile_function(const ir_function& func) {
    if (function_map.contains(func.name)) {
      return;
    }

    std::unordered_map<std::string, std::int64_t> local_variable_map;
    std::unordered_map<std::string, std::size_t> local_variable_size_map;
    
    auto w = std::make_unique<x64writer>();
    w->emit_function_prologue(0);
    
    std::size_t totalsizes = 0;
    auto count = 8; // first arg - 8
    
    for (auto& arg : func.args) {
      totalsizes += type_sizes[arg.type];
      count += 8;
      
      w->emit_mov_reg_mem("rax", "rbp", count); // load first arg into rax
      w->emit_sub_reg8_64_imm8_32("rsp", totalsizes); // allocate size on stack
      w->emit_mov_mem_reg("rbp", -totalsizes, "rax"); // store
      
      if (debug) {
        std::cout << YELLOW << "Arg(s) size(s) in store: " << totalsizes << "\nLocal: " << type_sizes[arg.type] << RESET << "\n";
      }
      
      local_variable_map.insert({arg.name, totalsizes});
      local_variable_size_map.insert({arg.name, type_sizes[arg.type]});
    }
    
    cleanup_size_map[func.name] = count - 8; // adjust for stack args weird placement
    
    function_map.insert({func.name, reinterpret_cast<jit_function>(w->memory)});
    
    bool ismain = false;
    if (func.name == "main") {
      ismain = true;
    }
    
    generate_code(func.code, w.get(), local_variable_map, local_variable_size_map, totalsizes, ismain);

    if (debug) {
      std::cout << CYAN;
      w->print_bytes();
      std::cout << RESET;
      
      std::ofstream output_file("output.bin", std::ios::binary);
      output_file.write(reinterpret_cast<const char*>(w->get_code().data()), w->get_code().size());
      output_file.close();
    }
    
    function_raw_code_map.insert({func.name, w->get_code()});
    
    w->setup_function();
    writers.push_back(std::move(w));
  }
  
  void jit_runtime::backpatch_jump(ir_opcode op, std::size_t location, std::size_t label_location, x64writer* w) {
    std::int32_t offset = label_location - (location + ((op == ir_opcode::op_jmp) ? 5 : 6));
    auto& code = w->get_code();
  
    switch (op) {
      case ir_opcode::op_jnz: code[location] = 0x0F; code[location+1] = 0x85; break;
      case ir_opcode::op_jge: code[location] = 0x0F; code[location+1] = 0x8D; break;
      case ir_opcode::op_jle: code[location] = 0x0F; code[location+1] = 0x8E; break;
      case ir_opcode::op_jl:  code[location] = 0x0F; code[location+1] = 0x8C; break;
      case ir_opcode::op_jg:  code[location] = 0x0F; code[location+1] = 0x8F; break;
      case ir_opcode::op_jz:  code[location] = 0x0F; code[location+1] = 0x84; break;
      case ir_opcode::op_jmp: code[location] = 0xE9; break;
      default: return;
    }
  
    std::size_t offset_start = (op == ir_opcode::op_jmp) ? 1 : 2;
    for (int i = 0; i < 4; ++i) {
      code[location + offset_start + i] = static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
    }
  }
  
  void jit_runtime::generate_code(std::vector<ir_instr> ir_code, x64writer* w, std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                  std::unordered_map<std::string, std::size_t>& local_variable_size_map, std::size_t totalsizes, bool ismain) {
    std::unordered_map<std::string, std::size_t> label_map;
    std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;
    
    for (const auto& instr : ir_code) {
      switch (instr.op) {
        case ir_opcode::op_push: {
          if (std::holds_alternative<std::string>(instr.operand)) { // generate strings dynamically BUT we need to fix the efficiency of this, and especially the size LOL but thats a later issue
            const auto& str = std::get<std::string>(instr.operand);
            
            w->emit_mov_reg_imm("rdi", str.length());
            w->emit_mov_reg_imm("rbx", reinterpret_cast<std::int64_t>(&function_map["__stralloc"]));
            w->emit_call_reg64("rbx");
            
            for (const auto& b : str) {
              w->emit_mov_mem_imm("rax", 0, b, k8bit);
              w->emit_inc_reg("rax");
            }
            
            w->emit_mov_mem_imm("rax", 0, '\0', k8bit);
            
            w->emit_sub_reg8_64_imm8_32("rax", str.length());
            
            w->emit_push_reg_64("rax");
          }
          else if (std::holds_alternative<std::int64_t>(instr.operand)) {
            w->emit_mov_reg_imm("rax", std::get<std::int64_t>(instr.operand));
            w->emit_push_reg_64("rax");
          }
          else if (std::holds_alternative<std::uint64_t>(instr.operand)) {
            w->emit_mov_reg_imm("rax", std::get<std::uint64_t>(instr.operand));
            w->emit_push_reg_64("rax");
          }// more and fix stuff later on
          
          break;
        }
        case ir_opcode::op_store: { // now handles existing variables
          auto var_name = std::get<std::string>(instr.operand);
          auto it = local_variable_map.find(var_name);
        
          if (it == local_variable_map.end()) { // variable doesn't exist yet
            if (debug) std::cout << BLUE << "EMITTING VARIABLE '" << var_name << "'\n" << RESET;
            
            totalsizes += type_sizes[instr.type];
            
            w->emit_pop_reg_64("rax");
            w->emit_sub_reg8_64_imm8_32("rsp", totalsizes);
            
            switch (type_sizes[instr.type]) { // this seems to work...?
              case 1: {
                w->emit_mov_mem_reg("rbp", -totalsizes, "rax", k8bit);
                break;
              }
              case 2: {
                w->emit_mov_mem_reg("rbp", -totalsizes, "rax", k16bit);
                break;
              }
              case 4: {
                w->emit_mov_mem_reg("rbp", -totalsizes, "rax", k32bit);
                break;
              }
              case 8: {
                w->emit_mov_mem_reg("rbp", -totalsizes, "rax");
                break;
              }
              default: {
                break;
              }
            }
        
            local_variable_map.insert({var_name, totalsizes});
            local_variable_size_map.insert({var_name, type_sizes[instr.type]});
          } 
          else { // variable already exists, reuse stack location
            if (debug) std::cout << BLUE << "EMITTING REUSE VARIABLE '" << var_name << "'\n" << RESET;
            w->emit_pop_reg_64("rax");
            
            switch (local_variable_size_map[var_name]) {
              case 1: {
                w->emit_mov_mem_reg("rbp", -it->second, "rax", k8bit);
                break;
              }
              case 2: {
                w->emit_mov_mem_reg("rbp", -it->second, "rax", k16bit);
                break;
              }
              case 4: {
                w->emit_mov_mem_reg("rbp", -it->second, "rax", k32bit);
                break;
              }
              case 8: {
                w->emit_mov_mem_reg("rbp", -it->second, "rax");
                break;
              }
              default: {
                break;
              }
            }
          }
        
          if (debug) {
            std::cout << YELLOW << "Sizes in store: " << totalsizes
                      << "\nLocal: " << type_sizes[instr.type]
                      << "\nVariable " << var_name << " at " << local_variable_map[var_name] << "\n";
            
            std::cout << BLUE << "CURRENT STORE SIZE IN VAR: " << local_variable_size_map[var_name] << RESET << std::endl;
          }
        
          break;
        }
        case ir_opcode::op_load: { 
          const auto& var_name = std::get<std::string>(instr.operand);
        
          auto it = local_variable_map.find(var_name);
          
          if (it == local_variable_map.end()) {
            std::cerr << RED << "Attempted to load undeclared variable '" << var_name << "'\n";
          }
        
          int offset = -it->second;
        
          if (debug) {
            std::cout << YELLOW << "Loading: " << var_name << "\nLocation: " << offset << std::endl;
          }
          
          auto var_size = local_variable_size_map[var_name];
          
          if (debug) {
            std::cout << YELLOW << "VAR_SIZE: " << var_size << RESET << std::endl;
          }
          
          if (var_size == 1) {
            if (debug) std::cout << BLUE << "EMITTED 1-BYTE VAR LOAD" <<  RESET << std::endl;
            w->emit_mov_reg_mem("rax", "rbp", offset, k8bit);
          }
          else if (var_size == 2) {
            if (debug) std::cout << BLUE << "EMITTED 2-BYTE VAR LOAD" <<  RESET << std::endl;
            w->emit_mov_reg_mem("rax", "rbp", offset, k16bit);
          }
          else if (var_size == 4) {
            if (debug) std::cout << BLUE << "EMITTED 4-BYTE VAR LOAD" <<  RESET << std::endl;
            w->emit_mov_reg_mem("rax", "rbp", offset, k32bit);
          }
          else if (var_size == 8) {
            if (debug) std::cout << BLUE << "EMITTED 8-BYTE VAR LOAD" <<  RESET << std::endl;
            w->emit_mov_reg_mem("rax", "rbp", offset);
          }
          
          w->emit_push_reg_64("rax");
          
          break;
        }
        case ir_opcode::op_pushf: { // pushing float values
          if (std::holds_alternative<float>(instr.operand)) {
              if (debug) std::cout << BLUE << "PUSHING FLOAT\n" << RESET;
          
              w->emit_mov_mem_imm("rax", 0, static_cast<std::uint64_t>(w->get_raw_float32_val(std::get<float>(instr.operand))), k32bit);
              w->emit_movss_reg_mem("xmm0", "rax", "", 0);
              w->emit_sub_reg8_64_imm8_32("rsp", 16);
              w->emit_movaps_mem_reg("xmm0", "", 0, "rsp");
          }
          else if (std::holds_alternative<double>(instr.operand)) {
            if (debug) std::cout << BLUE << "PUSHING DOUBLE\n" << RESET;
            
            w->emit_mov_reg_imm("rbx", w->get_raw_float64_val(std::get<double>(instr.operand)));
            w->emit_mov_mem_reg("rax", 0, "rbx");
            w->emit_movsd_reg_mem("xmm0", "rax", "", 0);
            w->emit_sub_reg8_64_imm8_32("rsp", 16);
            w->emit_movaps_mem_reg("xmm0", "", 0, "rsp");
          }
          
          break;
        }
        case ir_opcode::op_storef: { // store floats
          auto var_name = std::get<std::string>(instr.operand);
          auto it = local_variable_map.find(var_name);
          
          if (it == local_variable_map.end()) { // variable doesn't exist yet
            totalsizes += type_sizes[instr.type];
            
            w->emit_movss_reg_mem("xmm0", "rsp", "", 0);
            w->emit_movss_mem_reg("rbp", "", -totalsizes, "xmm0");
            
            local_variable_map.insert({var_name, totalsizes});
            local_variable_size_map.insert({var_name, type_sizes[instr.type]});
          }
          else {
            w->emit_movss_reg_mem("xmm0", "rsp", "", 0);
            w->emit_movss_mem_reg("rbp", "", -it->second, "xmm0");
          }
          
          break;
        }
        case ir_opcode::op_loadf: { // load floats 
          break;
        }
        case ir_opcode::op_ret: {
          if (!isjit && ismain) {
            w->emit_pop_reg_64("rax");
            w->emit_function_epilogue();
            w->emit_mov_reg_reg("rdi", "rax");
            w->emit_mov_reg_imm("rax", 60);
            w->emit_syscall();
          }
          else {
            w->emit_pop_reg_64("rax");
            w->emit_function_epilogue();
            w->emit_ret();
          }
          
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
        case ir_opcode::label: { // we realloc the label location in here
          auto current_location = w->get_code().size();
          auto label_name = std::get<std::string>(instr.operand);
          
          label_map[label_name] = current_location; // update label location
          
          break;
        }
        case ir_opcode::op_jmp: {
          auto jump_instr = instr;
          jump_instr.operand = std::get<std::string>(instr.operand);
          
          w->push_bytes({0x90, 0x90, 0x90, 0x90, 0x90});
          
          jump_instructions.push_back({jump_instr, w->get_code().size() - 5});
          
          break;
        }
        case ir_opcode::op_jle:
        case ir_opcode::op_jl:
        case ir_opcode::op_jg:
        case ir_opcode::op_jz:
        case ir_opcode::op_jge:
        case ir_opcode::op_jnz: {
          auto jump_instr = instr;
          jump_instr.operand = std::get<std::string>(instr.operand);
          
          w->push_bytes({0x90, 0x90, 0x90, 0x90, 0x90, 0x90});

          jump_instructions.push_back({jump_instr, w->get_code().size() - 6});
          
          break;
        }
        case ir_opcode::op_cmp: {
          w->emit_pop_reg_64("rax");
          w->emit_mov_reg_reg("rbx", "rax");
          w->emit_pop_reg_64("rax");
          w->emit_cmp_reg_reg("rax", "rbx");
          
          break;
        }
        case ir_opcode::op_call: { // recursion is iffy, TODO: find a way around lazy function compilation to handle recursion well ( i think i fixed this already )
          std::string func_name = std::get<std::string>(instr.operand);
          
          if (func_name == "__stralloc") {
            throw std::runtime_error("Cannot call internal function: " + func_name);
          }
          
          if (debug) {
            std::cout << BLUE <<  "Calling: " << func_name << "\n";
          }
          
          // somewhere around here needs to be fixed i think
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
          
          if (func_name == "print") {
            w->emit_add_reg8_64_imm8_32("rsp", 8);
          }
          else {
            w->emit_add_reg8_64_imm8_32("rsp", cleanup_size_map[func_name]);
          }
          
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
    
    for (auto& jump : jump_instructions) {
      if (debug) {
        std::cout << YELLOW << "Jump Backpatching:\n";
      }
      
      auto& instr = jump.first;
      auto jump_type = instr.op;
      auto label_name = std::get<std::string>(instr.operand);
      
      if (debug) {
        std::cout << "\tOpcode: " << opcode_to_string(jump_type) << std::endl;
        std::cout << "\tLocation: " << label_map[label_name] << RESET << std::endl;
      }
      
      if (label_map.contains(label_name)) {
        backpatch_jump(jump_type, jump.second, label_map[label_name], w); 
      }
      else {
        throw std::runtime_error("Label not found: " + label_name);
      }
    }
  }
} // namespace occult
*/