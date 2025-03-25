#include "jit.hpp"
#include "writer.hpp"
#include <fstream>

// just testing JIT functionality and viability

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

    std::unordered_map<std::string, std::size_t> local_variable_map;

    auto w = std::make_unique<x64writer>();
    w->emit_function_prologue(0);
    
    auto totalsizes = 0;
    auto count = 8; // first arg - 8
    
    for (auto& arg : func.args) {
      totalsizes += type_sizes[arg.type];
      count += 8;
      
      if (arg.type != "string") {
        w->emit_mov_reg_mem("rax", "rbp", count); // load first arg into rax
        w->emit_sub_reg8_64_imm8_32("rsp", totalsizes); // allocate size on stack
        w->emit_mov_mem_reg("rbp", -totalsizes, "rax"); // store
      }
      
      if (debug) {
        std::cout << "ARGS sizes in store: " << totalsizes << "\nlocal: " << type_sizes[arg.type] << "\n";
      }
      
      local_variable_map.insert({arg.name, totalsizes});
    }
    
    cleanup_size_map[func.name] = count - 8; // adjust for stack args weird placement
    
    function_map.insert({func.name, reinterpret_cast<jit_function>(w->memory)});
    
    generate_code(func.code, w.get(), local_variable_map);

    if (debug) {
      w->print_bytes();
      
      std::ofstream output_file("output.bin", std::ios::binary);
      output_file.write(reinterpret_cast<const char*>(w->get_code().data()), w->get_code().size());
      output_file.close();
    }

    w->setup_function();
    writers.push_back(std::move(w));
  }
  
  void jit_runtime::backpatch_jump(ir_opcode op, std::size_t location, std::size_t label_location, x64writer* w) {
    switch (op) {
      case ir_opcode::op_jnz: {
        w->get_code().at(location) = 0x0F;             
        w->get_code().at(location + 1) = 0x85;          
        std::int32_t offset = label_location - (location + 6); 
        
        w->get_code().at(location + 2) = static_cast<std::uint8_t>(offset & 0xFF);         
        w->get_code().at(location + 3) = static_cast<std::uint8_t>((offset >> 8) & 0xFF); 
        w->get_code().at(location + 4) = static_cast<std::uint8_t>((offset >> 16) & 0xFF); 
        w->get_code().at(location + 5) = static_cast<std::uint8_t>((offset >> 24) & 0xFF);
        
        break;
      }
      case ir_opcode::op_jge: {
        w->get_code().at(location) = 0x0F;             
        w->get_code().at(location + 1) = 0x8D;          
        std::int32_t offset = label_location - (location + 6);
        
        w->get_code().at(location + 2) = static_cast<std::uint8_t>(offset & 0xFF);        
        w->get_code().at(location + 3) = static_cast<std::uint8_t>((offset >> 8) & 0xFF);  
        w->get_code().at(location + 4) = static_cast<std::uint8_t>((offset >> 16) & 0xFF); 
        w->get_code().at(location + 5) = static_cast<std::uint8_t>((offset >> 24) & 0xFF); 
        
        break;
      }
      case ir_opcode::op_jz: {
        w->get_code().at(location) = 0x0F;              
        w->get_code().at(location + 1) = 0x84;          
        std::int32_t offset = label_location - (location + 6);  
       
        w->get_code().at(location + 2) = static_cast<std::uint8_t>(offset & 0xFF);         
        w->get_code().at(location + 3) = static_cast<std::uint8_t>((offset >> 8) & 0xFF); 
        w->get_code().at(location + 4) = static_cast<std::uint8_t>((offset >> 16) & 0xFF);
        w->get_code().at(location + 5) = static_cast<std::uint8_t>((offset >> 24) & 0xFF); 
        
        break;
      }
      case ir_opcode::op_jmp: {
        w->get_code().at(location) = 0xE9;          
        std::int32_t offset = label_location - (location + 5);  
       
        w->get_code().at(location + 1) = static_cast<std::uint8_t>(offset & 0xFF);        
        w->get_code().at(location + 2) = static_cast<std::uint8_t>((offset >> 8) & 0xFF);  
        w->get_code().at(location + 3) = static_cast<std::uint8_t>((offset >> 16) & 0xFF); 
        w->get_code().at(location + 4) = static_cast<std::uint8_t>((offset >> 24) & 0xFF); 
        
        break;
      }
      default: {
        break;
      }
    }
  }

  void jit_runtime::generate_code(std::vector<ir_instr> ir_code, x64writer* w, std::unordered_map<std::string, std::size_t>& local_variable_map) {
    std::unordered_map<std::string, std::size_t> label_map;
    // We no longer need jump_instructions because every branch now records a relocation.
    std::size_t totalsizes = 0;

    for (const auto& instr : ir_code) {
      switch (instr.op) {
        case ir_opcode::op_push: {
          if (std::holds_alternative<std::string>(instr.operand)) { // For strings
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
          break;
        }
        case ir_opcode::op_store: {
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
        case ir_opcode::op_load: {
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
        case ir_opcode::label: {
          auto current_location = w->get_code().size();
          auto label_name = std::get<std::string>(instr.operand);
          label_map[label_name] = current_location;
          break;
        }
        case ir_opcode::op_jmp: {
          std::string label = std::get<std::string>(instr.operand);
          std::size_t patch_location = w->get_code().size();
          // Emit JMP opcode (0xE9) with 4-byte placeholder
          w->push_bytes({0xE9, 0x00, 0x00, 0x00, 0x00});
          // Record relocation: offset starts at patch_location + 1
          relocation_entry rel;
          rel.symbol = label;
          rel.patch_location = patch_location + 1;
          rel.instruction_length = 4;
          relocations.push_back(rel);
          break;
        }
        case ir_opcode::op_jz:
        case ir_opcode::op_jge:
        case ir_opcode::op_jnz: {
          std::string label = std::get<std::string>(instr.operand);
          std::size_t patch_location = w->get_code().size();
          // Emit conditional jump opcode: e.g., for JNZ: 0x0F 0x85 followed by a 4-byte offset
          w->push_bytes({0x0F, 0x85, 0x00, 0x00, 0x00, 0x00});
          relocation_entry rel;
          rel.symbol = label;
          rel.patch_location = patch_location + 2; // offset field begins after two opcode bytes
          rel.instruction_length = 4;
          relocations.push_back(rel);
          break;
        }
        case ir_opcode::op_cmp: {
          w->emit_pop_reg_64("rax");
          w->emit_mov_reg_reg("rbx", "rax");
          w->emit_pop_reg_64("rax");
          w->emit_cmp_reg_reg("rax", "rbx");
          break;
        }
        case ir_opcode::op_call: {
          std::string func_name = std::get<std::string>(instr.operand);
          if (func_name == "__stralloc") {
            throw std::runtime_error("cannot call internal function: " + func_name);
          }
          if (debug) {
            std::cout << "calling: " << func_name << "\n";
          }
          // Compile function if not already compiled
          auto it = std::find_if(ir_funcs.begin(), ir_funcs.end(), [&](const ir_function& f) {
            return f.name == func_name;
          });
          if (it != ir_funcs.end()) {
            compile_function(*it);
          }
          if (!function_map.contains(func_name)) {
            throw std::runtime_error("undefined function: " + func_name);
          }
          // Using register call approach (no relocation needed):
          w->emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&function_map[func_name]));
          w->emit_call_reg64("rax");
          if (func_name == "print") {
            w->emit_add_reg8_64_imm8_32("rsp", 8);
          } else {
            w->emit_add_reg8_64_imm8_32("rsp", cleanup_size_map[func_name]);
          }
          if (func_name != "print") {
            w->emit_push_reg_64("rax");
          }
          break;
        }
        default: {
          break;
        }
      }
    }
    // Remove any separate jump_instructions patching loop since relocations handle branches.
  }

  void jit_runtime::compile_to_binary(const std::string& binary_name) {
    std::vector<std::vector<std::uint8_t>> fragments;

    for (const auto& w : writers) {
      fragments.push_back(w->get_code());
    }

    std::unordered_map<std::string, jit_function> global_symbols = function_map;

   // std::vector<occult::relocation_entry> relocations; //placeholder

    occult::linker linker;
    auto final_code = linker.link(fragments, global_symbols, relocations);

    linker.create_elf_binary(binary_name, final_code);

    //elf::generate_binary(binary_name, full_code.size(), full_code);
  }
} // namespace occult
