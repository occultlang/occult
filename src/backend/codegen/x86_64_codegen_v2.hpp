#pragma once
#include "x86_64_writer.hpp"
#include "ir_gen.hpp"

// a renewed stack codegen, more organized and stable

namespace occult::x86_64 {
    class codegen_v2 {    
        bool debug; 

        class register_allocator {
            std::unordered_map<std::variant<grp, simd128>, bool> register_liveliness = {
                {rax, false}, {rcx, false}, {rdx, false}, {rbx, false}, {rsi, false},
                {rdi, false}, {r8,  false}, {r9,  false}, {r10, false}, {r11, false},
                {r12, false}, {r13, false}, {r14, false}, {r15, false},
                
                {xmm0, false},  {xmm1, false}, {xmm2, false}, {xmm3, false}, {xmm4, false}, 
                {xmm5, false}, {xmm6, false}, {xmm7, false}, {xmm8, false}, {xmm9, false}, 
                {xmm10, false}, {xmm11, false}, {xmm12, false}, {xmm13, false}, {xmm14, false}, 
                {xmm15, false}
            };

            const std::vector<grp> allocation_order_grp = {
                rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11,
                rbx, r12, r13, r14, r15 
            };

            const std::vector<simd128> allocation_order_simd128 = {
                xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15
            };

            std::vector<std::variant<grp, simd128>> register_stack;
        public:
            register_allocator() = default;

            // returns true if register is live
            template<typename RegT>
            bool check(RegT reg) {
                auto it = register_liveliness.find(reg);

                return it != register_liveliness.end() && it->second;
            }

            // turns a specific register live and returns it
            template<typename RegT>
            std::optional<RegT> take(RegT reg) {
                if (!check(reg)) {
                    register_liveliness[reg] = true;

                    return reg;
                }

                return std::nullopt;
            }

            template<typename RegT>
            void release(RegT reg) {
                register_liveliness[reg] = false;
            }

            // allocates register based off of the allocation order
            template<typename RegT>
            RegT allocate() {
                if constexpr (std::is_same_v<RegT, grp>) {
                    for (auto& reg : allocation_order_grp) {
                        if (auto taken_reg = take(reg); taken_reg.has_value()) {
                            return taken_reg.value();
                        }
                    }
                }
                else if constexpr (std::is_same_v<RegT, simd128>) {
                    for (auto& reg : allocation_order_simd128) {
                        if (auto taken_reg = take(reg); taken_reg.has_value()) {
                            return taken_reg.value();
                        }
                    }
                }

                throw std::runtime_error("Invalid type for register allocator");
            }

            // pushes register onto register stack
            template<typename RegT>
            void push(RegT reg) { register_stack.push_back(reg); }

            // pops register from register stack and returns it
            template<typename RegT>
            RegT pop() {
                auto v = register_stack.back();
                register_stack.pop_back();
                return std::get<RegT>(v);
            }
        };

        std::unique_ptr<x86_64_writer> w; 
        std::vector<ir_function> stack_ir; 
        std::vector<ir_struct> ir_structs; 
        std::unordered_map<std::string, std::unt32_t> function_locs; // map of where the function locations are for calls
        std::unordered_set<std::string> compiling_functions; // currently compiling functions

        template<typename T>
        void handle_push(register_allocator& reg_alloc, ir_instr& instr) {
            if constexpr (std::is_same_v<T, double>) {
                double val = std::get<double>(instr.operand);
                auto xmm = reg_alloc.allocate<simd128>();
                auto reg = reg_alloc.allocate<grp>();
                w->emit_mov(reg, double_to_bits(val));
                w->emit_movq(xmm, reg);
                reg_alloc.release<grp>(reg);
                reg_alloc.push<simd128>(xmm);
            }
            else if constexpr (std::is_same_v<T, float>) {
                float val = std::get<float>(instr.operand);
                auto xmm = reg_alloc.allocate<simd128>();
                auto reg = reg_alloc.allocate<grp>();
                w->emit_mov(as_32(reg), float_to_bits(val));
                w->emit_movd(xmm, as_32(reg));
                reg_alloc.release<grp>(reg);
                reg_alloc.push<simd128>(xmm);
            }
            else if constexpr (std::is_same_v<T, std::string>) {
               // todo strings
            }
            else if constexpr (std::is_integral_v<T>) {
                T val = std::get<T>(instr.operand);
                auto reg = reg_alloc.allocate<grp>();
                w->emit_mov(reg, val);
                reg_alloc.push<grp>(reg);
            }
            else {
            }
        }

        struct push_visitor {
            codegen_v2* self;
            register_allocator& reg_alloc;
            ir_instr& instr;

            template<typename T>
            void operator()(const T&) const {
                self->handle_push<T>(reg_alloc, instr);
            }
        };

        void handle_store(ir_instr& instr, std::unordered_map<std::string, std::int64_t>& local_variable_map) {
            
        }

        void handle_load(ir_instr& instr, std::unordered_map<std::string, std::int64_t>& local_variable_map) {

        }

        void handle_cmp(register_allocator& reg_alloc, ir_instr& instr) {
            switch (instr.op) {
                case ir_opcode::op_cmpf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();

                    w->emit_comisd(lhs, rhs);

                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.release<simd128>(lhs);
                    
                    break;
                }
                case ir_opcode::op_cmpf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();

                    w->emit_comiss(lhs, rhs);

                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.release<simd128>(lhs);

                    break;
                }
                case ir_opcode::op_cmp: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();

                    w->emit_cmp(lhs, rhs);

                    reg_alloc.release<grp>(rhs);
                    reg_alloc.release<grp>(lhs);

                    break;
                }
                default:
                    break;
            }
        }

        void handle_call(ir_instr& instr) {
            std::string name = std::get<std::string>(instr.operand);

            auto it = std::ranges::find_if(stack_ir, [&](const ir_function& f) { return f.name == name; });

            if (it != stack_ir.end()) {
                compile_function(*it);
            }

            if (!function_locs.contains(name)) {
                std::cout << RED << "[CODEGEN ERROR] Function \"" << name << "\" not found." << RESET << std::endl;
                return;
            }

            w->emit_call(static_cast<std::uint32_t>(function_locs[name]));
        }

        static void backpatch_jumps() {

        }

        void generate_code(ir_function& func) {
            register_allocator reg_alloc;
            std::unordered_map<std::int64_t, std::string> local_variable_map;

            bool is_reference_next = false;
            bool is_dereference_next = false;
            bool is_dereference_assign_next = false;

            std::size_t deref_count_normal = 0;
            std::size_t deref_count_assign = 0;

            for (std::size_t i = 0; i < func.code.size(); i++) {
                auto& instr = func.code.at(i);

                switch(instr.op) {
                    case ir_opcode::op_push: {
                        std::visit(push_visitor{this, reg_alloc, instr}, instr.operand);

                        break;
                    }
                    case ir_opcode::op_load: {
                        break;
                    }
                    case ir_opcode::op_store: {
                        break;
                    }
                    case ir_opcode::op_call: {
                        handle_call(instr);

                        break;
                    }
                    case ir_opcode::op_cmp:
                    case ir_opcode::op_cmpf32:
                    case ir_opcode::op_cmpf64: {
                        handle_cmp(reg_alloc, instr);

                        break;
                    }
                    case ir_opcode::op_ret: {
                        w->emit_function_epilogue();
                        w->emit_ret();
                        break;
                    }
                }
            }
        }

        void compile_struct(ir_struct& struct_) {
            
        }

        void compile_function(ir_function& func) {
            if (function_locs.contains(func.name) || func.is_external) { // skip, already registered
                if (debug) {
                    std::cout << BLUE << "[CODEGEN] Already registered (skipping): " << YELLOW << func.name << RESET << "\n";
                }

                return;
            }

            if (compiling_functions.contains(func.name)) {
                return;
            }
            else {
                compiling_functions.insert(func.name);
            }

            // fresh compilation
            if (debug) {
                std::cout << BLUE << "[CODEGEN] Compiling: " << YELLOW << func.name << RESET << "\n";
            }

            auto loc = w->get_code().size();
            function_locs.insert({func.name, loc});

            if (!func.uses_shellcode && !func.uses_assembly) {
                w->emit_function_prologue();
            }

            generate_code(func);

            if (debug) {
                std::cout << GREEN << "[CODEGEN] Compiled: " << YELLOW << func.name << GREEN <<  " (loc: " << loc << ")" RESET << "\n";
            }

            compiling_functions.erase(func.name);
        }

        public:

        codegen_v2(const std::vector<ir_function>& stack_ir, const std::vector<ir_struct>& ir_structs, const bool debug = false) : stack_ir(stack_ir), ir_structs(ir_structs), debug(debug), w(new x86_64_writer{}) {} 

        void compile(const bool use_jit = true) { // default to jit compilation
            for (auto& func : stack_ir) {
                compile_function(func);
            }

            if (debug) {
                std::cout << GREEN << "[CODEGEN] Blob: " << RESET << "\n";
                w->print_bytes();
            }
        }
    };
} // namespace occult::x86_64
