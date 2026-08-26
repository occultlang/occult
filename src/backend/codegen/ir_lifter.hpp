#pragma once
#include "ir_gen.hpp"

// IR lifter from linear stack to linear register,

namespace occult {
    class ir_lifter {
        std::vector<rir_function> lifted_functions;
        std::vector<ir_function> functions;

    public: 
        ir_lifter(std::vector<ir_function>& functions) : functions(functions) {}

        void lift_to_rir() {
            auto to_rir_operand = [](const ir_operand& v) -> rir_operand {
                return std::visit([](auto&& x) -> rir_operand {
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, std::monostate>) return {};
                    else return x;
                }, v);
            };

            rir_function translated_func;

            vreg_allocator _vreg_allocator;

            std::vector<rir_vreg> sim_stack;

            for (auto& f : functions) {
                translated_func.name = f.name;
                translated_func.type = f.type;
                translated_func.args = std::vector<ir_argument>{f.args};

                translated_func.uses_shellcode = f.uses_shellcode;
                translated_func.is_external = f.is_external;
                translated_func.is_variadic = f.is_variadic;
                translated_func.uses_assembly = f.uses_assembly;

                for(auto& instr : f.code) {
                    switch(instr.op) {
                        case ir_opcode::op_push: {
                            rir_instr r_instr;
                            
                            r_instr.type = instr.type;
                            r_instr.op = rir_opcode::rop_mov;
                            r_instr.dst = _vreg_allocator.fresh(); 
                            r_instr.operand[0] = to_rir_operand(instr.operand);

                            sim_stack.emplace_back(r_instr.dst);
                            translated_func.emplace_back(r_instr);

                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    };
} // namespace occult