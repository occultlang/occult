#pragma once

#include "x86_64_writer.hpp"
#include "ir_gen.hpp"
#include <map>

/*
 * A class for stack-oriented codegen, redone
 */

namespace occult {
  namespace x86_64 {
    static std::unordered_map<std::string, std::size_t> typename_sizes = {
      {"int64", 8},
      {"int32", 4},
      {"int16", 2},
      {"int8", 1},
      {"uint64", 8},
      {"uint32", 4},
      {"uint16", 2},
      {"uint8", 1},
    }; /* we might have to use this instead of an 8-byte aligned stack, i'm not sure */

    class codegen {
      #ifdef __linux 
        bool is_systemv = true;
      #elif _WIN64
        bool is_systemv = false;
      #endif

      class register_pool {
        std::vector<grp> free_regs;
        std::vector<grp> reg_stack;
        x86_64_writer* writer;
      public:
        register_pool(x86_64_writer* w) : writer(w) {
          free_regs = {
            r10, r11, r12, r13, r14, r15
          };
        }

        grp alloc() {
          if (free_regs.empty()) {
            writer->print_bytes();
            throw std::runtime_error("no registers free"); 
          }
          grp r = free_regs.back();
          free_regs.pop_back();
          return r;
        }

        void free(grp r) {
          free_regs.push_back(r);
        }

        void push(grp r) {
          reg_stack.push_back(r);
        }

        grp pop(ir_function& func, std::size_t i) {
          if (reg_stack.empty()) {
            writer->print_bytes();

            std::cout << "IR TRACE:\n";
            for (auto j = i; j < func.code.size(); j++) {
              ir_instr& code = func.code.at(j);
              std::cout << opcode_to_string(code.op) << " ";
              std::visit(visitor_stack(), code.operand);
              if (code.type != "")
                std::cout << "(type = " << code.type << ")" << std::endl;
            }

            throw std::runtime_error("stack underflow");
          }
          grp r = reg_stack.back();
          reg_stack.pop_back();
          return r;
        }

        grp top() const {
          if (reg_stack.empty()) {
            writer->print_bytes();
            throw std::runtime_error("stack empty"); 
          }
          return reg_stack.back();
        }

        void reset() {
          reg_stack.clear();
          free_regs = {
            r10, r11, r12, r13, r14, r15
          };
        }

        bool empty() const {
          return reg_stack.empty();
        }

        size_t stack_size() const {
          return reg_stack.size();
        }

        const std::vector<grp>& get_stack() const {
          return reg_stack;
        }
      };

      std::vector<ir_function> ir_funcs;
      std::vector<std::unique_ptr<x86_64_writer>> writers;
      std::unordered_map<std::string, std::int32_t> cleanup_size_map;

      bool debug;

      void backpatch_jump(ir_opcode op, std::size_t location, std::size_t label_location, x86_64_writer* w) {
        std::int32_t offset = label_location - (location + ((op == ir_opcode::op_jmp) ? 5 : 6));
        auto& code = w->get_code();
      
        switch (op) {
          case ir_opcode::op_jnz: code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JNZ_rel32; break;
          case ir_opcode::op_jge: code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JNL_rel32; break;
          case ir_opcode::op_jle: code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JLE_rel32; break;
          case ir_opcode::op_jl:  code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JL_rel32; break;
          case ir_opcode::op_jg:  code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JNLE_rel32; break;
          case ir_opcode::op_jz:  code[location] = k2ByteOpcodePrefix; code[location+1] = opcode_2b::JZ_rel32; break;
          case ir_opcode::op_jmp: code[location] = opcode::JMP_rel32; break;
          default: return;
        }
      
        std::size_t offset_start = (op == ir_opcode::op_jmp) ? 1 : 2;
        for (int i = 0; i < 4; ++i) {
          code[location + offset_start + i] = static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
        }
      }

      void emit_temporary_zero_extend(const grp& reg, x86_64_writer* w) { // this is temporary until i put proper zero extend into the writer
        w->push_byte(0x45);
        w->push_byte(k2ByteOpcodePrefix);
        w->push_byte(opcode_2b::MOVZX_r16_to_64_rm8);
        w->push_byte(modrm(mod_field::register_direct, reg, reg));
      }

      void emit_temporary_cmovz(const grp& reg, const grp& reg2, x86_64_writer* w) {
        w->push_byte(0x45);
        w->push_byte(k2ByteOpcodePrefix);
        w->push_byte(static_cast<opcode_2b>(0x44));
        w->push_byte(modrm(mod_field::register_direct, reg2, reg));
      }

      void emit_temporary_cmovnz(const grp& reg, const grp& reg2, x86_64_writer* w) { 
        w->push_byte(0x45);
        w->push_byte(k2ByteOpcodePrefix);
        w->push_byte(static_cast<opcode_2b>(0x45));
        w->push_byte(modrm(mod_field::register_direct, reg2, reg));
      }
      
      void generate_code(ir_function& func, x86_64_writer* w, bool is_main, bool use_jit) {
        register_pool pool(w);
        std::unordered_map<std::string, std::size_t> label_map;
        std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;
        std::unordered_map<std::string, std::int64_t> local_variable_map;
        std::unordered_map<std::string, std::string> local_variable_map_types;

        std::int32_t totalsizes = 0;

        const grp sysv_regs[] = {rdi, rsi, rdx, rcx, r8, r9};
        
        for (std::size_t i = 0; i < func.args.size(); ++i) {
          totalsizes += 8; // do correct sizes
          auto r = pool.alloc();
        
          if (is_systemv) {
            if (i < sizeof(sysv_regs) / sizeof(grp)) {
              w->emit_mov(r, sysv_regs[i]);
            } 
            else {
              std::int32_t offset = 16 + (i - 6) * 8;
              w->emit_mov(r, mem{rbp, offset});
            }
          } 
        
          w->emit_sub(rsp, 8);
          w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
        
          pool.free(r);
          local_variable_map.insert({func.args[i].name, totalsizes});
          local_variable_map_types.insert({func.args[i].name, func.args[i].type});
        }

        for (std::size_t i = 0; i < func.code.size(); i++) {
          auto& code = func.code.at(i);
          
          switch (code.op) {
            case ir_opcode::op_push: {
              if (std::holds_alternative<std::int64_t>(code.operand)) {
                std::int64_t val = std::get<std::int64_t>(code.operand);

                grp r = pool.alloc();

                w->emit_mov(r, val);    

                pool.push(r);
              }
              else if (std::holds_alternative<std::uint64_t>(code.operand)) {
                std::uint64_t val = std::get<std::uint64_t>(code.operand);

                grp r = pool.alloc();

                w->emit_mov(r, val); 

                pool.push(r);
              }
              
              break;
            }
            case ir_opcode::op_store: {
              auto var_name = std::get<std::string>(code.operand);
              auto var_type = code.type;
              auto it = local_variable_map.find(var_name);
            
              grp r = pool.pop(func, i);

              if (it == local_variable_map.end()) { // variable doesn't exist yet
                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET << var_name << std::endl;
                  std::cout << "\tSize: " << typename_sizes[var_type] << std::endl;
                }

                totalsizes += 8;

                w->emit_sub(rsp, totalsizes);
                w->emit_mov(mem{rbp, -totalsizes}, r);

                pool.free(r);
                  
                local_variable_map.insert({var_name, totalsizes});
                local_variable_map_types.insert({var_name, var_type});
              }
              else {
                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Re-emitting variable: " << RESET << var_name << std::endl;
                }

                std::int32_t offset = -it->second;

                w->emit_mov(mem{rbp, offset}, r);

                pool.free(r);

                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Variable location: " << RESET << offset << std::endl;
                }
              }

              break;
            }
            case ir_opcode::op_load: {
              const auto& var_name = std::get<std::string>(code.operand);
              const auto& var_type = local_variable_map_types[var_name];
              auto it = local_variable_map.find(var_name);

              if (it == local_variable_map.end()) {
                std::cerr << RED << "[CODEGEN ERROR] Attempted to load undeclared variable: " << RESET << var_name << std::endl;

                return; 
              }

              std::int32_t offset = -it->second;
        
              if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Loading: " << RESET << var_name <<"\n\tLocation: " << offset << "\n\tType: " << YELLOW << var_type << RESET << std::endl;
              }

              auto r = pool.alloc();
              
              if (var_type == "int8") {
                w->emit_movsx(r, mem{rbp, offset});
              }
              else if (var_type == "uint8") {
                w->emit_movzx(r, mem{rbp, offset});
              }
              else if (var_type == "int16") {
                w->emit_movsx(r, mem{rbp, offset}, false);
              }
              else if (var_type == "uint16") {
                w->emit_movzx(r, mem{rbp, offset}, false);
              }
              else if (var_type == "int32") {
                w->emit_movsxd(r, mem{rbp, offset});
              }
              else if (var_type == "uint32") {
                w->emit_mov(rdi, mem{rbp, offset}); // sysv_regs
                w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map["__cast_to_uint32__"]));
                w->emit_call(mem{rax});
                w->emit_mov(r, rax);
              }
              else if (var_type == "int64") {
                w->emit_mov(rdi, mem{rbp, offset}); // sysv_regs
                w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map["__cast_to_int64__"]));
                w->emit_call(mem{rax});
                w->emit_mov(r, rax);
              }
              else if (var_type == "uint64") {
                w->emit_mov(rdi, mem{rbp, offset}); // sysv_regs
                w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map["__cast_to_uint64__"]));
                w->emit_call(mem{rax});
                w->emit_mov(r, rax);
              }

              pool.push(r);

              break;
            }

            /* arith */
            case ir_opcode::op_add: {
              auto rhs = pool.pop(func, i);     
              auto lhs = pool.pop(func, i);   

              w->emit_add(lhs, rhs);  

              pool.free(rhs);
              pool.push(lhs); 

              break;
            }
            case ir_opcode::op_sub: {
              auto rhs = pool.pop(func, i);     
              auto lhs = pool.pop(func, i);   

              w->emit_sub(lhs, rhs);  

              pool.free(rhs);
              pool.push(lhs); 

              break;
            }
            case ir_opcode::op_mod: {
              auto rhs = pool.pop(func, i);     
              auto lhs = pool.pop(func, i);

              w->emit_mov(rax, lhs);
              w->emit_xor(rdx, rdx);
              w->emit_div(rhs);

              pool.push(rdx); // remainder
              
              pool.free(rhs);
              pool.free(lhs);

              break;
            }
            case ir_opcode::op_div: {
              auto rhs = pool.pop(func, i);     
              auto lhs = pool.pop(func, i);

              w->emit_mov(rax, lhs);
              w->emit_xor(rdx, rdx);
              w->emit_div(rhs);

              pool.push(rax); // quotient
              
              pool.free(rhs);
              pool.free(lhs);
              
              break;
            }
            case ir_opcode::op_mul: {
              auto rhs = pool.pop(func, i);     
              auto lhs = pool.pop(func, i);

              w->emit_mov(rax, lhs);
              w->emit_mul(rhs);

              pool.push(rax);

              pool.free(rhs);
              pool.free(lhs);

              break;
            }
            case ir_opcode::op_negate: {
              auto r = pool.pop(func, i); 

              w->emit_neg(r);

              pool.push(r);

              break;
            }

            /* bitwise */
            case ir_opcode::op_bitwise_and: {
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_and(lhs, rhs);   

              pool.push(lhs);
              pool.free(rhs);

              break;
            }
            case ir_opcode::op_bitwise_or: {
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_or(lhs, rhs);  

              pool.push(lhs);
              pool.free(rhs);

              break;
            }
            case ir_opcode::op_bitwise_xor: {
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_xor(lhs, rhs);   

              pool.push(lhs);
              pool.free(rhs);

              break;
            }
            case ir_opcode::op_bitwise_not: { 
              auto r = pool.pop(func, i); 

              w->emit_not(r);

              pool.push(r);

              break;
            }
            case ir_opcode::op_bitwise_lshift: { 
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_mov(rcx, rhs);
              w->emit_shl(lhs);

              pool.push(lhs);
              pool.free(rhs);

              break;
            }
            case ir_opcode::op_bitwise_rshift: { 
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_mov(rcx, rhs);
              w->emit_shr(lhs);

              pool.push(lhs);
              pool.free(rhs);

              break;
            }
          
            case ir_opcode::op_ret: {
              if (!pool.empty()) {
                auto result = pool.pop(func, i);

                w->emit_mov(rax, result); 

                pool.free(result);
              }

              if (!use_jit && is_main) {
                w->emit_pop(rax);
                w->emit_function_epilogue();
                w->emit_mov(rdi, rax);
                w->emit_mov(rax, 60); 
                w->emit_syscall();
              }
              else {
                w->emit_function_epilogue();
                w->emit_ret();
              }
              break;
            }

            /* logical */
            case ir_opcode::label: { // we realloc the label location in here
              auto current_location = w->get_code().size();
              auto label_name = std::get<std::string>(code.operand);
              
              label_map[label_name] = current_location; // update label location
              
              break;
            }
            case ir_opcode::op_jmp: {
              auto jump_instr = code;
              jump_instr.operand = std::get<std::string>(code.operand);
              
              w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});
              
              jump_instructions.push_back({jump_instr, w->get_code().size() - 5});
              
              break;
            }
            case ir_opcode::op_jle:
            case ir_opcode::op_jl:
            case ir_opcode::op_jg:
            case ir_opcode::op_jz:
            case ir_opcode::op_jge:
            case ir_opcode::op_jnz: {
              auto jump_instr = code;
              jump_instr.operand = std::get<std::string>(code.operand);
              
              w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});

              jump_instructions.push_back({jump_instr, w->get_code().size() - 6});
              
              break;
            }
            case ir_opcode::op_cmp: {
              auto rhs = pool.pop(func, i);
              auto lhs = pool.pop(func, i);

              w->emit_cmp(lhs, rhs);

              pool.free(rhs);
              pool.free(lhs);

              break;
            }
            case ir_opcode::op_setz: {
              auto target = pool.alloc();

              w->emit_setz(target); 
              emit_temporary_zero_extend(target, w);

              pool.push(target); 

              break;
            }
            case ir_opcode::op_setnz: {
              auto target = pool.alloc();

              w->emit_setnz(target); 
              emit_temporary_zero_extend(target, w);
              
              pool.push(target); 

              break;
            }
            case ir_opcode::op_setl: {
              auto target = pool.alloc();

              w->emit_setl(target); 
              emit_temporary_zero_extend(target, w);

              pool.push(target); 

              break;
            }
            case ir_opcode::op_setle: {
              auto target = pool.alloc();

              w->emit_setle(target); 
              emit_temporary_zero_extend(target, w);

              pool.push(target); 

              break;
            }
            case ir_opcode::op_setg: {
              auto target = pool.alloc();

              w->emit_setnl(target); 
              emit_temporary_zero_extend(target, w);

              pool.push(target); 

              break;
            }
            case ir_opcode::op_setge: {
              auto target = pool.alloc();

              w->emit_setnle(target); 
              emit_temporary_zero_extend(target, w);

              pool.push(target); 

              break;
            }
            case ir_opcode::op_not: {
              auto r = pool.pop(func, i);  

              w->emit_cmp(r, 0);         
              w->emit_setz(r);           // set r to 1 if equal (i.e., was 0), else 0
              pool.push(r);     

              break;
            }
            case ir_opcode::op_call: {
              std::string func_name = std::get<std::string>(code.operand);

              if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Calling: " << YELLOW << "\"" << func_name << "\"\n" << RESET;
              }

              auto it = std::find_if(ir_funcs.begin(), ir_funcs.end(), [&](const ir_function& f) { // lazy compilation
                return f.name == func_name;
              });

              if (it != ir_funcs.end()) {
                compile_function(*it, use_jit);
              }

              if (!function_map.contains(func_name)) {
                std::cout << RED << "[CODEGEN ERROR] Function \"" << func_name << "\" not found." << RESET << std::endl;

                return;
              }

              if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Popping arguments for function call: " << YELLOW << "\"" << func_name << "\"\n" << RESET;
              }
              
              int arg_count = it->args.size(); 
              if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Argument count: " << RESET << arg_count << std::endl;
              }

              std::vector<grp> reg_stack_copy = pool.get_stack();

              std::vector<grp> args;
              for (int i = 0; i < arg_count; i++) {
                if (pool.empty()) {
                  std::cout << RED << "[CODEGEN ERROR] Not enough arguments on stack for function call: " << func_name << RESET << std::endl;

                  return;
                }

                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Popping argument: " << RESET << reg_to_string(pool.top()) << std::endl;
                }

                args.push_back(pool.pop(func, i));
              }
              
              std::reverse(args.begin(), args.end());

              if (!is_systemv) {
                w->emit_sub(rsp, 32);  // shadow space for x64 MICROSOFT ABI
              }

              for (std::size_t i = 0; i < args.size(); i++) {
                if (is_systemv) {
                  const grp sysv_regs[] = {rdi, rsi, rdx, rcx, r8, r9};
                  
                  if (i < sizeof(sysv_regs) / sizeof(grp)) {
                    if (debug) {
                      std::cout << BLUE << "[CODEGEN INFO] Pushing argument: " << RESET << reg_to_string(sysv_regs[i]) << std::endl;
                    }
                    
                    w->emit_mov(sysv_regs[i], args[i]);
                  } 
                  else {
                    w->emit_push(args[i]);
                  }
                } 

                pool.free(args[i]);
              }

              std::vector<grp> caller_saved_used;
              if (is_systemv) {
                const grp caller_saved[] = {r10, r11, r12, r13, r14, r15};
                std::vector<grp> used;

                for (auto reg : caller_saved) {
                  if (std::find(reg_stack_copy.begin(), reg_stack_copy.end(), reg) != reg_stack_copy.end()) {
                    w->emit_push(reg);
                    used.push_back(reg);
                  }
                }

                caller_saved_used = std::move(used);
              }
              
              w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map[func_name]));
              w->emit_call(mem{rax});
          
              if (is_systemv) {
                for (auto it = caller_saved_used.rbegin(); it != caller_saved_used.rend(); ++it) {
                  w->emit_pop(*it);
                }
              }

              std::size_t reg_limit = is_systemv ? 6 : 4;
              if (args.size() > reg_limit) {
                w->emit_add(rsp, (args.size() - reg_limit) * 8);
              }

              auto ret_reg = pool.alloc();
              w->emit_mov(ret_reg, rax);
              pool.push(ret_reg);

              break;
            }

            /* arrays */
            case op_array_decl: {
              break;
            }

            default: {
              break;
            }
          }
        }

        if (debug) {
          std::cout << BLUE << "[CODEGEN INFO] Total jump instructions to backpatch " << RESET << jump_instructions.size() << std::endl;
        }

        for (auto& jump : jump_instructions) {
          if (debug) {
            std::cout << BLUE << "[CODEGEN INFO] Jump Backpatching:\n" << RESET;
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
            std::cout << RED << "[CODEGEN ERROR] Label \"" << label_name << "\" not found." << RESET << std::endl;

            return;
          }
        }
      }

      void compile_function(ir_function& func, bool use_jit) {
        static std::unordered_set<std::string> compiling_functions;

        if (debug && function_map.contains(func.name)) {
            std::cout << BLUE << "[CODEGEN INFO] Symbol " << YELLOW << "\"" << func.name << "\"" << BLUE << " already exists, probably already compiled or is compiling." << RESET << std::endl;
            return;
        }

        if (compiling_functions.contains(func.name)) {
            return; 
        }

        compiling_functions.insert(func.name);

        if (debug) {
            std::cout << BLUE << "[CODEGEN INFO] Compiling function: " << YELLOW << "\"" << func.name << "\"\n" << RESET;
        }

        auto w = std::make_unique<x86_64_writer>();
        function_map.insert({func.name, reinterpret_cast<jit_function>(w->memory)});
        w->emit_function_prologue();

        bool is_main = false;
        if (func.name == "main") {
            is_main = true;
        }

        generate_code(func, w.get(), is_main, use_jit);

        if (debug) {
            std::cout << GREEN << "[CODEGEN INFO] Generated code for " << YELLOW << "\"" << func.name << "\"\n" << RESET;
            w->print_bytes();
        }

        function_raw_code_map.insert({func.name, w->get_code()});
        w->setup_function();
        writers.push_back(std::move(w));
        compiling_functions.erase(func.name); 
    }
    public:
      std::unordered_map<std::string, jit_function> function_map;
      std::map<std::string, std::vector<std::uint8_t>> function_raw_code_map;

      codegen(std::vector<ir_function> ir_funcs, bool debug = false) : ir_funcs(ir_funcs), debug(debug) { }
      
      ~codegen() {
        writers.clear();
      }

      void compile(bool use_jit = true) {
        for (auto& func : ir_funcs) {
          compile_function(func, use_jit);
        }
      }
    };
  }
} // namespace occult