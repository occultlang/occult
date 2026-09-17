#pragma once
#include "x86_64_writer.hpp"
#include "ir_gen.hpp"

// a renewed stack codegen, more organized and stable

namespace occult::x86_64 {
    class codegen_v2 {    
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
        std::unordered_map<std::string, std::uint64_t> function_map; // map of where the function locations are for calls

        // we will use this with ir_opcode::push and use a visitor to handle all of these types.
        template<typename T>
        void handle_push(register_allocator& reg_alloc, ir_instr instr) {
            auto handle_integer = [&]<typename U>() -> void {
                if (std::holds_alternative<U>(instr.operand)) {
                    U val = std::get<U>(instr.operand);
                    auto reg = reg_alloc.allocate<grp>();

                    w->emit_mov(reg, val);

                    reg_alloc.push<grp>(reg);
                }
            };

            if (std::holds_alternative<double>(instr.operand)) {free
                const double& lf = std::get<double>(instr.operand);
                auto xmm = reg_alloc.allocate<simd128>();
                auto reg = reg_alloc.allocate<grp>();

                w->emit_mov(reg, double_to_bits(lf));
                w->emit_movq(xmm, reg);

                reg_alloc.release<grp>(reg);
                reg_alloc.push<simd128>(xmm);
            } 
            else if (std::holds_alternative<float>(instr.operand)) {
                const float& f = std::get<float>(instr.operand);
                auto xmm = reg_alloc.allocate<simd128>();
                auto reg = reg_alloc.allocate<grp>();

                w->emit_mov(as_32(reg), float_to_bits(f));
                w->emit_movd(xmm, as_32(reg));

                reg_alloc.release<grp>(reg);
                reg_alloc.push<simd128>(xmm);
            }
            // strings go here 
            else {
                handle_integer.template operator()<T>();
            }
        }

        void generate_code(ir_function& func) {
            register_allocator reg_alloc;
        }

        void compile_struct(ir_struct& struct_) {
            
        }

        void compile_function(ir_function& func) {
                        
        }

        public:

        codegen_v2(const std::vector<ir_function>& stack_ir, const std::vector<ir_struct>& ir_structs, const bool debug = false) : stack_ir(stack_ir), ir_structs(ir_structs), w(new x86_64_writer{}) {} 

        void compile(const bool use_jit = true) { // default to jit compilation
            for (auto& func : stack_ir) {
                compile_function(func);
            }
        }
    };
} // namespace occult::x86_64
