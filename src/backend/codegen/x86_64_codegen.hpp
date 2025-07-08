#pragma once

#include "x86_64_writer.hpp"
#include "ir_gen.hpp"
#include <map>

/*
 * A class for stack-oriented codegen, redone
 */

namespace occult {
  namespace x86_64 {
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

        grp pop() {
          if (reg_stack.empty()) {
            writer->print_bytes();
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
      
      void generate_code(const ir_function& func, x86_64_writer* w, bool is_main, bool use_jit) {
        register_pool pool(w);
        std::unordered_map<std::string, std::size_t> label_map;
        std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;
        std::unordered_map<std::string, std::int64_t> local_variable_map;
        
        std::uint32_t totalsizes = 0;

        const grp sysv_regs[] = {rdi, rsi, rdx, rcx, r8, r9};
        
        for (std::size_t i = 0; i < func.args.size(); ++i) {
          totalsizes += 8;
          auto r = pool.alloc();
        
          if (is_systemv) {
            if (i < sizeof(sysv_regs) / sizeof(grp)) {
              w->emit_mov(r, sysv_regs[i]);
            } 
            else {
              std::uint32_t offset = 16 + (i - 6) * 8;
              w->emit_mov(r, mem{rbp, offset});
            }
          } 
        
          w->emit_sub(rsp, 8);
          w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
        
          pool.free(r);
          local_variable_map.insert({func.args[i].name, totalsizes});
        }

        for (const auto& code : func.code) {
          switch (code.op) {
            case ir_opcode::op_push: {
              if (std::holds_alternative<int64_t>(code.operand)) {
                int64_t val = std::get<int64_t>(code.operand);

                grp r = pool.alloc();

                w->emit_mov(r, val);    

                pool.push(r);
              }

              break;
            }
            case ir_opcode::op_store: {
              auto var_name = std::get<std::string>(code.operand);
              auto it = local_variable_map.find(var_name);
            
              if (it == local_variable_map.end()) { // variable doesn't exist yet
                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET << var_name << std::endl;
                }

                totalsizes += 8;

                grp r = pool.pop();

                w->emit_sub(rsp, 8);
                w->emit_mov(mem{rbp, -totalsizes}, r);

                pool.free(r);
                  
                local_variable_map.insert({var_name, totalsizes});
              }
              else {
                if (debug) {
                  std::cout << BLUE << "[CODEGEN INFO] Re-emitting variable: " << RESET << var_name << std::endl;
                }

                auto offset = -it->second;

                grp r = pool.pop();

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

              auto it = local_variable_map.find(var_name);

              if (it == local_variable_map.end()) {
                std::cerr << RED << "[CODEGEN ERROR] Attempted to load undeclared variable: " << RESET << var_name << std::endl;

                return; 
              }

              auto offset = -it->second;
        
              if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Loading: " << RESET << var_name << "\n\tLocation: " << offset << std::endl;
              }

              auto r = pool.alloc();

              w->emit_mov(r, mem{rbp, offset});

              pool.push(r);

              break;
            }

            /* arith */
            case ir_opcode::op_add: {
              auto rhs = pool.pop();     
              auto lhs = pool.pop();   

              w->emit_add(lhs, rhs);  

              pool.free(rhs);
              pool.push(lhs); 

              break;
            }
            case ir_opcode::op_sub: {
              auto rhs = pool.pop();     
              auto lhs = pool.pop();   

              w->emit_sub(lhs, rhs);  

              pool.free(rhs);
              pool.push(lhs); 

              break;
            }
            case ir_opcode::op_mod: {
              auto rhs = pool.pop();     
              auto lhs = pool.pop();

              w->emit_mov(rax, lhs);
              w->emit_xor(rdx, rdx);
              w->emit_div(rhs);

              pool.push(rdx); // remainder
              
              break;
            }
            case ir_opcode::op_div: {
              auto rhs = pool.pop();     
              auto lhs = pool.pop();

              w->emit_mov(rax, lhs);
              w->emit_xor(rdx, rdx);
              w->emit_div(rhs);

              pool.push(rax); // quotient
              
              break;
            }
            case ir_opcode::op_mul: {
              auto rhs = pool.pop();     
              auto lhs = pool.pop();

              w->emit_mov(rax, lhs);
              w->emit_mul(rhs);

              pool.push(rax);

              break;
            }

            /* bitwise */
            case ir_opcode::op_bitwise_and: {
              auto rhs = pool.pop();
              auto lhs = pool.pop();

              auto result = pool.alloc();
              w->emit_mov(result, lhs);   
              w->emit_and(result, rhs);   

              pool.push(result);

              break;
            }
            case ir_opcode::op_bitwise_or: {
              auto rhs = pool.pop();
              auto lhs = pool.pop();

              auto result = pool.alloc();
              w->emit_mov(result, lhs);  
              w->emit_or(result, rhs);  

              pool.push(result);

              break;
            }
            case ir_opcode::op_bitwise_xor: {
              auto rhs = pool.pop();
              auto lhs = pool.pop();

              auto result = pool.alloc();
              w->emit_mov(result, lhs);   
              w->emit_xor(result, rhs);   

              pool.push(result);

              break;
            }
            case ir_opcode::op_bitwise_not: { // not implemented because I need to fix the precedence for it in IR and parser (its)
              break;
            }
            case ir_opcode::op_bitwise_lshift: { // have to add SHL which uses CL (reg) to x86_64_writer.hpp
              break;
            }
            case ir_opcode::op_bitwise_rshift: { // have to add SHR which uses CL (reg) to x86_64_writer.hpp
              break;
            }
          
            case ir_opcode::op_ret: {
              if (!pool.empty()) {
                auto result = pool.pop();

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
              auto rhs = pool.pop();
              auto lhs = pool.pop();

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
            case ir_opcode::op_logical_and: { // add cmov to x86_64_writer.hpp
              break;
            }
            case ir_opcode::op_logical_or: { // add cmov to x86_64_writer.hpp
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

                args.push_back(pool.pop());
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

              if (is_systemv) {
                const grp caller_saved[] = {r10, r11, r12, r13, r14, r15};
                for (auto reg : caller_saved) {
                  w->emit_push(reg);
                }
              }
              
              w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map[func_name]));
              w->emit_call(mem{rax});
          
              if (is_systemv) {
                const grp caller_saved[] = {r15, r14, r13, r12, r11, r10};
                for (auto reg : caller_saved) {
                  w->emit_pop(reg);
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

      void compile_function(const ir_function& func, bool use_jit) {
        if (debug && function_map.contains(func.name)) {
          std::cout << BLUE << "[CODEGEN INFO] Symbol " << YELLOW << "\"" << func.name << "\"" << BLUE << " already exists, probably already compiled or is compiling." << RESET << std::endl;

          return;
        }

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
      }
    public:
      std::unordered_map<std::string, jit_function> function_map;
      std::map<std::string, std::vector<std::uint8_t>> function_raw_code_map;

      codegen(std::vector<ir_function> ir_funcs, bool debug) : ir_funcs(ir_funcs), debug(debug) {}
      
      ~codegen() {
        writers.clear();
      }

      void compile(bool use_jit = true) {
        for (const auto& func : ir_funcs) {
          compile_function(func, use_jit);
        }
      }
    };
  }
} // namespace occult