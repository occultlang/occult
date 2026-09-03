#pragma once
#include "ir_gen.hpp"

// IR lifter from linear stack to linear register,

namespace occult {
    class ir_lifter {
        std::vector<ir_function> functions;

        struct visitor_register {
            void operator()(std::monostate) const {}
            void operator()(const rir_vreg& v) const { std::cout << "v" << v.id; }
            void operator()(const std::string& s) const { std::cout << "\"" << s << "\""; }
            template <typename T>
            void operator()(const T& v) const {
                std::cout << v;
            }
        };

    public:
        static void visualize_register_ir(const std::vector<rir_function>& funcs) {
            std::cout << CYAN << "Function(s): \n" << RESET;
            for (const auto& [code, args, name, type, uses_shellcode, is_external, is_variadic, uses_assembly, vreg_count] : funcs) {
                std::cout << "Type: " << type << "\n";
                std::cout << "Name: " << name << "\n";
                std::cout << "Vregs allocated: " << vreg_count << "\n";

                std::cout << "Args:\n";
                for (auto& arg : args) {
                    std::cout << "  " << arg.type << " " << arg.name << "\n";
                }

                std::cout << "Code:\n";
                for (auto& i : code) {
                    std::cout << "  ";
                    if (i.dst.is_valid()) {
                        std::visit(visitor_register(), rir_operand(i.dst));
                        std::cout << " = ";
                    }
                    std::cout << ropcode_to_string(i.op) << " ";

                    std::visit(visitor_register(), i.src[0]);
                    if (!std::holds_alternative<std::monostate>(i.src[1])) {
                        std::cout << ", ";
                        std::visit(visitor_register(), i.src[1]);
                    }
                    for (auto& e : i.extra) {
                        std::cout << ", ";
                        std::visit(visitor_register(), e);
                    }

                    if (!i.type.empty())
                        std::cout << "\t (type = " << i.type << ")";
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        }

        ir_lifter(std::vector<ir_function>& functions) : functions(functions) {}

        std::vector<rir_function> lift() {
            std::vector<rir_function> lifted_functions;

            auto to_rir_operand = [](const ir_operand& v) -> rir_operand {
                return std::visit(
                    [](auto&& x) -> rir_operand {
                        using T = std::decay_t<decltype(x)>;
                        if constexpr (std::is_same_v<T, std::monostate>)
                            return {};
                        else
                            return x;
                    },
                    v);
            };

            for (auto& f : functions) {
                rir_function translated_func;

                vreg_allocator _vreg_allocator;

                std::vector<rir_vreg> sim_stack;

                translated_func.name = f.name;
                translated_func.type = f.type;
                translated_func.args = std::vector<ir_argument>{f.args};

                translated_func.uses_shellcode = f.uses_shellcode;
                translated_func.is_external = f.is_external;
                translated_func.is_variadic = f.is_variadic;
                translated_func.uses_assembly = f.uses_assembly;

                auto pop_vreg = [&sim_stack]() -> rir_operand {
                    rir_vreg v = sim_stack.back();
                    sim_stack.pop_back();
                    return v;
                };

                auto emit_binop = [&](rir_opcode rop, const std::string& type) {
                    rir_operand rhs = pop_vreg();
                    rir_operand lhs = pop_vreg();
                    rir_instr r_instr(rop, _vreg_allocator.fresh(), lhs, rhs);
                    r_instr.type = type;
                    sim_stack.emplace_back(r_instr.dst);
                    translated_func.code.emplace_back(std::move(r_instr));
                };

                auto emit_unop = [&](rir_opcode rop, const std::string& type) {
                    rir_operand src = pop_vreg();
                    rir_instr r_instr(rop, _vreg_allocator.fresh(), src);
                    r_instr.type = type;
                    sim_stack.emplace_back(r_instr.dst);
                    translated_func.code.emplace_back(std::move(r_instr));
                };

                for (auto& instr : f.code) {
                    switch (instr.op) {
                    case ir_opcode::op_push:
                        {
                            rir_instr r_instr;
                            r_instr.type = instr.type;
                            r_instr.op = rir_opcode::rop_mov;
                            r_instr.dst = _vreg_allocator.fresh();
                            r_instr.src[0] = to_rir_operand(instr.operand);
                            sim_stack.emplace_back(r_instr.dst);
                            translated_func.code.emplace_back(r_instr);
                            break;
                        }
                    case ir_opcode::op_add:
                        emit_binop(rop_add, instr.type);
                        break;
                    case ir_opcode::op_sub:
                        emit_binop(rop_sub, instr.type);
                        break;
                    case ir_opcode::op_mul:
                        emit_binop(rop_mul, instr.type);
                        break;
                    case ir_opcode::op_div:
                        emit_binop(rop_div, instr.type);
                        break;
                    case ir_opcode::op_mod:
                        emit_binop(rop_mod, instr.type);
                        break;
                    case ir_opcode::op_imul:
                        emit_binop(rop_imul, instr.type);
                        break;
                    case ir_opcode::op_idiv:
                        emit_binop(rop_idiv, instr.type);
                        break;
                    case ir_opcode::op_imod:
                        emit_binop(rop_imod, instr.type);
                        break;
                    case ir_opcode::op_bitwise_and:
                        emit_binop(rop_bitwise_and, instr.type);
                        break;
                    case ir_opcode::op_bitwise_or:
                        emit_binop(rop_bitwise_or, instr.type);
                        break;
                    case ir_opcode::op_bitwise_xor:
                        emit_binop(rop_bitwise_xor, instr.type);
                        break;
                    case ir_opcode::op_bitwise_lshift:
                        emit_binop(rop_bitwise_lshift, instr.type);
                        break;
                    case ir_opcode::op_bitwise_rshift:
                        emit_binop(rop_bitwise_rshift, instr.type);
                        break;
                    case ir_opcode::op_ibitwise_rshift:
                        emit_binop(rop_ibitwise_rshift, instr.type);
                        break;
                    case ir_opcode::op_logical_and:
                        emit_binop(rop_logical_and, instr.type);
                        break;
                    case ir_opcode::op_logical_or:
                        emit_binop(rop_logical_or, instr.type);
                        break;
                    case ir_opcode::op_cmp:
                        emit_binop(rop_cmp, instr.type);
                        break;

                    case ir_opcode::op_negate:
                    case ir_opcode::op_negatef32:
                    case ir_opcode::op_negatef64:
                        emit_unop(rop_negate, instr.type);
                        break;
                    case ir_opcode::op_bitwise_not:
                        emit_unop(rop_bitwise_not, instr.type);
                        break;
                    case ir_opcode::op_not:
                        emit_unop(rop_not, instr.type);
                        break;

                    case ir_opcode::op_store:
                        {
                            rir_operand src = pop_vreg();
                            rir_instr r_instr(rop_store, rir_vreg{rir_vreg::invalid_id}, src, to_rir_operand(instr.operand));
                            translated_func.code.emplace_back(std::move(r_instr));
                            break;
                        }
                    case ir_opcode::op_load:
                        {
                            rir_instr r_instr(rop_load, _vreg_allocator.fresh(), to_rir_operand(instr.operand));
                            sim_stack.emplace_back(r_instr.dst);
                            translated_func.code.emplace_back(r_instr);
                            break;
                        }

                    case ir_opcode::label:
                        translated_func.code.emplace_back(rop_label, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jmp:
                        translated_func.code.emplace_back(rop_jmp, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jz:
                        translated_func.code.emplace_back(rop_jz, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jnz:
                        translated_func.code.emplace_back(rop_jnz, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jl:
                        translated_func.code.emplace_back(rop_jl, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jle:
                        translated_func.code.emplace_back(rop_jle, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jg:
                        translated_func.code.emplace_back(rop_jg, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;
                    case ir_opcode::op_jge:
                        translated_func.code.emplace_back(rop_jge, rir_vreg{rir_vreg::invalid_id}, to_rir_operand(instr.operand));
                        break;

                    case ir_opcode::op_ret:
                        {
                            rir_instr r_instr(rop_ret, rir_vreg{rir_vreg::invalid_id});
                            if (!sim_stack.empty()) {
                                r_instr.src[0] = pop_vreg();
                            }
                            translated_func.code.emplace_back(std::move(r_instr));
                            break;
                        }

                    default:
                        break;
                    }
                }
                translated_func.vreg_count = _vreg_allocator.next;
                lifted_functions.emplace_back(translated_func);
            }

            return lifted_functions;
        }
    };
} // namespace occult
