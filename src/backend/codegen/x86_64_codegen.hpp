#pragma once

#include "ir_gen.hpp"
#include "x86_64_writer.hpp"

#include <initializer_list>
#include <map>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <variant>

/*
 * A class for stack-oriented codegen, redone
 */

// IMPORTANT (ignore this for now?)
// there's a bug with stack sizing (call and totalsize, we must properly deal
// with subtracting rsp for variables and maintaining the correct stack size
// after calls)

namespace occult::x86_64 {
static std::unordered_map<std::string, std::size_t> typename_sizes = {
    {"int64", 8},  {"int32", 4},  {"int16", 2}, {"int8", 1},    {"uint64", 8},
    {"uint32", 4}, {"uint16", 2}, {"uint8", 1}, {"float32", 4}, {"float64", 8},
}; /* we might have to use this instead of an 8-byte aligned stack, i'm not sure
    */

struct array_metadata {
    std::string type;
    std::vector<std::uint64_t> dimensions;
    std::int32_t total_size{};
};

class codegen {
#ifdef __linux
    bool is_systemv = true;
#elif _WIN64
    bool is_systemv = false;
#endif

    template <typename RegT, std::size_t RegSize>
    class register_pool {
        struct stack_value {
            bool is_spilled = false;
            RegT reg;
            std::int32_t offset = 0;
        };

        std::vector<RegT> free_regs;
        std::vector<RegT> initial_free_regs; // saved for reset()
        std::vector<stack_value> reg_stack;

        x86_64_writer* writer = nullptr;
        std::int32_t* total_stack_size = nullptr;

    public:
        explicit register_pool(x86_64_writer* w, std::initializer_list<RegT> initial_regs)
            : writer(w), initial_free_regs(initial_regs) {
            free_regs = initial_free_regs;
        }

        void set_stack_size_tracker(std::int32_t* ptr) { total_stack_size = ptr; }

        RegT alloc() {
            if (free_regs.empty()) {
                if (!reg_stack.empty() && total_stack_size) {
                    for (size_t i = 0; i < reg_stack.size(); ++i) {
                        if (!reg_stack[i].is_spilled) {
                            RegT r = reg_stack[i].reg;
                            *total_stack_size += static_cast<std::int32_t>(RegSize);
                            std::int32_t spill_offset = -*total_stack_size;

                            if constexpr (std::is_same_v<RegT, grp>) {
                                writer->emit_mov(mem{rbp, spill_offset}, r);
                            } else if constexpr (std::is_same_v<RegT, simd128>) {
                                writer->emit_movsd(mem{rbp, spill_offset},
                                                   r); // or emit_movaps if aligned
                            }

                            reg_stack[i].is_spilled = true;
                            reg_stack[i].offset = spill_offset;
                            free_regs.push_back(r);
                            break;
                        }
                    }
                }

                if (free_regs.empty()) {
                    writer->print_bytes();
                    throw std::runtime_error("no registers free / cannot spill");
                }
            }

            RegT r = free_regs.back();
            free_regs.pop_back();
            return r;
        }

        void free(RegT r) { free_regs.push_back(r); }

        void push(RegT r) { reg_stack.push_back({.is_spilled = false, .reg = r, .offset = 0}); }

        RegT pop(ir_function& func, std::size_t instr_index = 0) {
            if (reg_stack.empty()) {
                writer->print_bytes();

                // GP-specific debug trace (only makes sense for register_pool<grp, 8>)
                // For SIMD we just throw without trace
                if constexpr (std::is_same_v<RegT, grp>) {
                    std::cout << "IR TRACE:\n";
                    for (std::size_t j = instr_index; j < func.code.size(); ++j) {
                        ir_instr& code = func.code.at(j);
                        std::cout << opcode_to_string(code.op) << " ";
                        std::visit(visitor_stack(), code.operand);
                        if (!code.type.empty()) {
                            std::cout << "(type = " << code.type << ")";
                        }
                        std::cout << "\n";
                    }
                }

                throw std::runtime_error("stack underflow");
            }

            stack_value val = reg_stack.back();
            reg_stack.pop_back();

            if (val.is_spilled) {
                RegT r = alloc();

                if constexpr (std::is_same_v<RegT, grp>) {
                    writer->emit_mov(r, mem{rbp, val.offset});
                } else if constexpr (std::is_same_v<RegT, simd128>) {
                    writer->emit_movsd(r, mem{rbp, val.offset});
                }

                return r;
            } else {
                return val.reg;
            }
        }

        [[nodiscard]] RegT top() {
            if (reg_stack.empty()) {
                writer->print_bytes();
                throw std::runtime_error("stack empty");
            }

            stack_value& val = reg_stack.back();
            if (val.is_spilled) {
                RegT r = alloc();

                if constexpr (std::is_same_v<RegT, grp>) {
                    writer->emit_mov(r, mem{rbp, val.offset});
                } else if constexpr (std::is_same_v<RegT, simd128>) {
                    writer->emit_movsd(r, mem{rbp, val.offset});
                }

                val.is_spilled = false;
                val.reg = r;
            }
            return val.reg;
        }

        void reset() {
            reg_stack.clear();
            free_regs = initial_free_regs; // restore original free list
        }

        [[nodiscard]] bool empty() const { return reg_stack.empty(); }
        [[nodiscard]] size_t stack_size() const { return reg_stack.size(); }

        [[nodiscard]] std::vector<RegT> get_active_registers() const {
            std::vector<RegT> regs;
            for (const auto& val : reg_stack) {
                if (!val.is_spilled) {
                    regs.push_back(val.reg);
                }
            }
            return regs;
        }
    };

    using register_pool_gp = register_pool<grp, 8>;
    using register_pool_simd = register_pool<simd128, 16>;

    std::vector<ir_function> ir_funcs;
    std::vector<ir_struct> ir_structs;
    std::vector<std::unique_ptr<x86_64_writer>> writers;
    std::unordered_map<std::string, std::int32_t> cleanup_size_map;
    std::unordered_map<std::string, std::string> function_return_types;

    bool debug;

    static void backpatch_jump(const ir_opcode op, const std::size_t location,
                               std::size_t label_location, x86_64_writer* w) {
        const std::int32_t offset =
            label_location - (location + ((op == ir_opcode::op_jmp) ? 5 : 6));
        auto& code = w->get_code();

        switch (op) {
        case ir_opcode::op_jnz:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JNZ_rel32;
            break;
        case ir_opcode::op_jge:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JNL_rel32;
            break;
        case ir_opcode::op_jle:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JLE_rel32;
            break;
        case ir_opcode::op_jl:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JL_rel32;
            break;
        case ir_opcode::op_jg:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JNLE_rel32;
            break;
        case ir_opcode::op_jz:
            code[location] = k2ByteOpcodePrefix;
            code[location + 1] = opcode_2b::JZ_rel32;
            break;
        case ir_opcode::op_jmp:
            code[location] = opcode::JMP_rel32;
            break;
        default:
            return;
        }

        const std::size_t offset_start = (op == ir_opcode::op_jmp) ? 1 : 2;
        for (int i = 0; i < 4; ++i) {
            code[location + offset_start + i] =
                static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
        }
    }

    void generate_code(ir_function& func, x86_64_writer* w, bool is_main, bool use_jit) {
        register_pool_gp pool(w, {r10, r11, r12, r13, r14, r15});
        register_pool_simd simd_pool(w, {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9,
                                         xmm10, xmm11, xmm12, xmm13, xmm14, xmm15});

        std::unordered_map<std::string, std::size_t> label_map;
        std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;

        std::unordered_map<std::string, std::int64_t> local_variable_map;
        std::unordered_map<std::string, std::string> local_variable_map_types;

        std::unordered_map<std::string, array_metadata> local_array_metadata;

        bool is_reference_next = false;
        bool is_dereference_next = false;
        bool is_dereference_assign_next = false;
        std::size_t deref_count_normal = 0;
        std::size_t deref_count_assign = 0;

        std::int32_t totalsizes = 0;

        pool.set_stack_size_tracker(&totalsizes); // register spilling
        simd_pool.set_stack_size_tracker(&totalsizes);

        constexpr grp sysv_regs[] = {rdi, rsi, rdx, rcx, r8, r9};
        constexpr simd128 sysv_regs_simd[] = {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7};

        if (!func.uses_shellcode) {
            std::size_t gp_arg_idx = 0;
            std::size_t fp_arg_idx = 0;
            std::size_t stack_arg_offset = 16; // start after return address and saved rbp

            for (std::size_t i = 0; i < func.args.size(); ++i) {
                const auto& arg_type = func.args[i].type;
                bool is_fp = (arg_type == "float32" || arg_type == "float64");

                totalsizes += 8;

                if (is_systemv) {
                    if (is_fp) {
                        // sysv floating point args (xmm)
                        if (fp_arg_idx < 8) {
                            auto simd_tmp = simd_pool.alloc();
                            w->emit_movsd(simd_tmp, sysv_regs_simd[fp_arg_idx]);
                            w->emit_sub(rsp, 8);
                            w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)},
                                          simd_tmp);
                            simd_pool.free(simd_tmp);
                            fp_arg_idx++;
                        } else { // stack
                            auto simd_tmp = simd_pool.alloc();
                            w->emit_movsd(simd_tmp,
                                          mem{rbp, static_cast<std::int32_t>(stack_arg_offset)});
                            w->emit_sub(rsp, 8);
                            w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)},
                                          simd_tmp);
                            simd_pool.free(simd_tmp);
                            stack_arg_offset += 8;
                        }
                    } else { // gpr for integers
                        if (gp_arg_idx < 6) {
                            auto r = pool.alloc();
                            w->emit_mov(r, sysv_regs[gp_arg_idx]);
                            w->emit_sub(rsp, 8);
                            w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                            pool.free(r);
                            gp_arg_idx++;
                        } else { // stack
                            auto r = pool.alloc();
                            w->emit_mov(r, mem{rbp, static_cast<std::int32_t>(stack_arg_offset)});
                            w->emit_sub(rsp, 8);
                            w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                            pool.free(r);
                            stack_arg_offset += 8;
                        }
                    }
                } else { // windows (yuck)
                    constexpr grp win_regs[] = {rcx, rdx, r8, r9};
                    constexpr simd128 win_regs_simd[] = {xmm0, xmm1, xmm2, xmm3};

                    if (is_fp) {
                        if (i < 4) {
                            auto simd_tmp = simd_pool.alloc();
                            w->emit_movsd(simd_tmp, win_regs_simd[i]);
                            w->emit_sub(rsp, 8);
                            w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)},
                                          simd_tmp);
                            simd_pool.free(simd_tmp);
                        } else { // stack
                            auto simd_tmp = simd_pool.alloc();
                            w->emit_movsd(simd_tmp, mem{rbp, static_cast<std::int32_t>(
                                                                 32 + (i - 4) * 8 + 16)});
                            w->emit_sub(rsp, 8);
                            w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)},
                                          simd_tmp);
                            simd_pool.free(simd_tmp);
                        }
                    } else {
                        if (i < 4) {
                            auto r = pool.alloc();
                            w->emit_mov(r, win_regs[i]);
                            w->emit_sub(rsp, 8);
                            w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                            pool.free(r);
                        } else {
                            auto r = pool.alloc();
                            w->emit_mov(r,
                                        mem{rbp, static_cast<std::int32_t>(32 + (i - 4) * 8 + 16)});
                            w->emit_sub(rsp, 8);
                            w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                            pool.free(r);
                        }
                    }
                }

                local_variable_map.insert({func.args[i].name, totalsizes});
                local_variable_map_types.insert({func.args[i].name, arg_type});
            }
        }

        bool pushing_for_ret = false;
        bool push_single = false;
        for (std::size_t i = 0; i < func.code.size(); i++) {
            auto& code = func.code.at(i);
            std::uint64_t index_to_push;

            switch (code.op) {
            case ir_opcode::op_push_shellcode: {
                if (std::holds_alternative<std::uint8_t>(code.operand)) {
                    std::uint8_t val = std::get<std::uint8_t>(code.operand);
                    w->push_byte(val);
                }

                break;
            }
            case ir_opcode::op_push: {
                if (std::holds_alternative<std::int64_t>(code.operand)) {
                    std::int64_t val = std::get<std::int64_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, val);
                    pool.push(r);
                } else if (std::holds_alternative<std::uint64_t>(code.operand)) {
                    std::uint64_t val = std::get<std::uint64_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, val);
                    pool.push(r);
                } else if (std::holds_alternative<std::int32_t>(code.operand)) {
                    std::int32_t val = std::get<std::int32_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, val);
                    pool.push(r);
                } else if (std::holds_alternative<std::uint32_t>(code.operand)) {
                    std::uint32_t val = std::get<std::uint32_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, val);
                    pool.push(r);
                } else if (std::holds_alternative<std::int16_t>(code.operand)) {
                    std::int16_t val = std::get<std::int16_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, static_cast<std::int64_t>(val));
                    pool.push(r);
                } else if (std::holds_alternative<std::uint16_t>(code.operand)) {
                    std::uint16_t val = std::get<std::uint16_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, static_cast<std::uint64_t>(val));
                    pool.push(r);
                } else if (std::holds_alternative<std::int8_t>(code.operand)) {
                    std::int8_t val = std::get<std::int8_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, static_cast<std::int64_t>(val));
                    pool.push(r);
                } else if (std::holds_alternative<std::uint8_t>(code.operand)) {
                    std::uint8_t val = std::get<std::uint8_t>(code.operand);
                    grp r = pool.alloc();
                    w->emit_mov(r, static_cast<std::uint64_t>(val));
                    pool.push(r);
                } else if (std::holds_alternative<std::string>(code.operand)) {
                    const std::string& str = std::get<std::string>(code.operand);

                    // Round to 16 bytes instead of 8!
                    std::uint32_t rounded_size = ((str.size() + 15) / 16) * 16;

                    w->emit_sub(rsp, rounded_size);

                    grp tmp = pool.alloc();
                    for (std::size_t i = 0; i < str.size(); ++i) {
                        w->emit_mov(tmp, static_cast<std::uint8_t>(str[i]));
                        w->emit_mov(mem{rsp, static_cast<std::int32_t>(i)}, tmp);
                    }
                    pool.free(tmp);

                    grp r = pool.alloc();
                    w->emit_mov(r, rsp);
                    pool.push(r);

                    totalsizes += rounded_size;

                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Pushed string: " << RESET << "\""
                                  << str << "\"\n"
                                  << "\tSize: " << str.size() << "\n"
                                  << "\tRounded size (16-byte aligned): " << rounded_size << "\n"
                                  << "\tAddress in " << reg_to_string(r) << std::endl;
                    }
                } else if (std::holds_alternative<double>(code.operand)) {
                    const double& lf = std::get<double>(code.operand);
                    simd128 xmm = simd_pool.alloc();
                    grp r = pool.alloc();

                    w->emit_mov(r, double_to_bits(lf));
                    w->emit_movq(xmm, r);

                    pool.free(r);
                    simd_pool.push(xmm);
                } else if (std::holds_alternative<float>(code.operand)) {
                    const float& f = std::get<float>(code.operand);

                    simd128 xmm = simd_pool.alloc();
                    grp r = pool.alloc();

                    w->emit_mov(as_32(r), float_to_bits(f));
                    w->emit_movq(xmm, as_32(r));

                    pool.free(r);
                    simd_pool.push(xmm);
                }

                break;
            }
            case ir_opcode::op_push_for_ret: {
                if (std::holds_alternative<std::int64_t>(code.operand)) {
                    std::int64_t val = std::get<std::int64_t>(code.operand);
                    w->emit_mov(rax, val);

                    pushing_for_ret = true;
                }

                break;
            }
            case ir_opcode::op_push_single: {
                if (std::holds_alternative<std::int64_t>(code.operand)) {
                    std::int64_t val = std::get<std::int64_t>(code.operand);
                    grp r = (push_single) ? pool.top() : pool.alloc(); // have to check this

                    w->emit_mov(r, val);

                    if (!push_single) {
                        pool.push(r);
                    }

                    push_single = true;
                }

                break;
            }
            case ir_opcode::op_store: {
                auto var_name = std::get<std::string>(code.operand);
                auto var_type = code.type;
                auto it = local_variable_map.find(var_name);

                if (pool.empty() && !simd_pool.empty()) {
                    auto simd_reg = simd_pool.pop(func, i);
                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Floating point store" << RESET
                                  << std::endl;
                    }

                    if (it == local_variable_map.end()) {
                        // variable doesn't exist yet
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET
                                      << var_name << std::endl;
                            std::cout << "\tSize: " << typename_sizes[var_type] << std::endl;
                        }

                        totalsizes += 8;

                        w->emit_sub(rsp, 8);

                        if (var_type == "float32") {
                            w->emit_movss(mem{rbp, -totalsizes}, simd_reg);

                        } else if (var_type == "float64") {
                            w->emit_movsd(mem{rbp, -totalsizes}, simd_reg);
                        }

                        simd_pool.free(simd_reg);

                        local_variable_map.insert({var_name, totalsizes});
                        local_variable_map_types.insert({var_name, var_type});

                        break;
                    }
                }

                grp r = (push_single) ? pool.top() : pool.pop(func, i);
                push_single = false;

                if (it == local_variable_map.end()) {
                    // variable doesn't exist yet
                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET
                                  << var_name << std::endl;
                        std::cout << "\tSize: " << typename_sizes[var_type] << std::endl;
                    }

                    totalsizes += 8;

                    if (is_reference_next) {
                        std::cout << RED << "[CODEGEN ERROR] Cannot have reference in store."
                                  << RESET << std::endl;
                    }

                    w->emit_sub(rsp, 8);

                    if (var_type == "int8" || var_type == "uint8" || var_type == "bool") {
                        w->emit_mov(mem{bpl, -totalsizes}, as_8(r));
                    } else if (var_type == "int16" || var_type == "uint16") {
                        w->emit_mov(mem{bp, -totalsizes}, as_16(r));
                    } else if (var_type == "int32" || var_type == "uint32") {
                        w->emit_mov(mem{ebp, -totalsizes}, as_32(r));
                    } else {
                        w->emit_mov(mem{rbp, -totalsizes}, r);
                    }

                    pool.free(r);

                    local_variable_map.insert({var_name, totalsizes});
                    local_variable_map_types.insert({var_name, var_type});
                } else {
                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Re-emitting variable: " << RESET
                                  << var_name << std::endl;
                    }

                    std::int32_t offset = -it->second;

                    if (var_type == "int8" || var_type == "uint8") {
                        w->emit_mov(mem{bpl, offset}, as_8(r));
                    } else if (var_type == "int16" || var_type == "uint16") {
                        w->emit_mov(mem{bp, offset}, as_16(r));
                    } else if (var_type == "int32" || var_type == "uint32") {
                        w->emit_mov(mem{ebp, offset}, as_32(r));
                    } else {
                        w->emit_mov(mem{rbp, offset}, r);
                    }

                    pool.free(r);

                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Variable location: " << RESET << offset
                                  << std::endl;
                    }
                }

                break;
            }
            case ir_opcode::op_load: {
                const auto& var_name = std::get<std::string>(code.operand);
                const auto& var_type = local_variable_map_types[var_name];
                auto it = local_variable_map.find(var_name);

                if (it == local_variable_map.end()) {
                    std::cerr << RED
                              << "[CODEGEN ERROR] Attempted to load undeclared variable: " << RESET
                              << var_name << std::endl;

                    return;
                }

                std::int32_t offset = -it->second;

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Loading: " << RESET << var_name
                              << "\n\tLocation: " << offset << "\n\tType: " << YELLOW << var_type
                              << RESET << std::endl;
                }

                auto r = pool.alloc();

                if (is_reference_next) {
                    w->emit_lea(r, mem{rbp, offset});
                    is_reference_next = false;

                    pool.push(r);

                    break;
                }

                if (is_dereference_next) {
                    for (std::size_t size = 1; size <= deref_count_normal; size++) {
                        if (size == 1) {
                            w->emit_mov(r, mem{rbp, offset});
                            w->emit_mov(r, mem{r});
                        } else {
                            w->emit_mov(r, mem{r});
                        }
                    }

                    is_dereference_next = false;
                    deref_count_normal = 0;

                    pool.push(r);

                    break;
                }

                if (is_dereference_assign_next) {
                    for (std::size_t size = 1; size <= deref_count_assign; size++) {
                        if (size == 1) {
                            w->emit_mov(r, mem{rbp, offset});
                        } else {
                            w->emit_mov(r, mem{r});
                        }
                    }

                    deref_count_assign = 0;
                    is_dereference_assign_next = false;

                    pool.push(r);

                    break;
                }

                if (var_type == "int8" || var_type == "bool") {
                    w->emit_movsx(r, mem{rbp, offset});
                } else if (var_type == "uint8") {
                    w->emit_movzx(r, mem{rbp, offset});
                } else if (var_type == "int16") {
                    w->emit_movsx(r, mem{rbp, offset}, false);
                } else if (var_type == "uint16") {
                    w->emit_movzx(r, mem{rbp, offset}, false);
                } else if (var_type == "int32") {
                    w->emit_movsxd(r, mem{rbp, offset});
                } else if (var_type == "uint32") {
                    w->emit_mov(as_32(r), mem{rbp, offset});
                } else if (var_type == "int64" || var_type == "uint64") {
                    w->emit_mov(r, mem{rbp, offset});
                } else if (var_type == "string") {
                    w->emit_mov(r, mem{rbp, offset});
                } else if (var_type == "float64") {
                    auto simd_reg = simd_pool.alloc();
                    w->emit_movsd(simd_reg, mem{rbp, offset});
                    simd_pool.push(simd_reg);

                    break;
                } else if (var_type == "float32") {
                    auto simd_reg = simd_pool.alloc();
                    w->emit_movss(simd_reg, mem{rbp, offset});
                    simd_pool.push(simd_reg);

                    break;
                }

                pool.push(r);

                break;
            }

            case ir_opcode::op_reference: {
                is_reference_next = true;

                break;
            }
            /*case ir_opcode::op_dereference: {
                auto operand = std::get<std::int64_t>(code.operand);

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Dereference count: " << RESET
            << operand << std::endl;
                }

                is_dereference_next = true;
                deref_count_normal = operand;

                break;
            }*/
            case ir_opcode::op_dereference: {
                auto operand = std::get<std::int64_t>(code.operand);

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Dereference count: " << RESET << operand
                              << std::endl;
                }

                grp addr = pool.top();

                for (std::size_t count = 0; count < operand; count++) {
                    w->emit_mov(addr, mem{addr});
                }

                break;
            }
            case ir_opcode::op_dereference_assign: {
                auto operand = std::get<std::int64_t>(code.operand);

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Dereference (assignment) count: " << RESET
                              << operand << std::endl;
                }

                is_dereference_assign_next = true;
                deref_count_assign = operand;

                break;
            }
            case ir_opcode::op_store_at_addr: {
                auto val = pool.pop(func, i);
                auto addr = pool.pop(func, i);

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Storing value in " << RESET
                              << reg_to_string(val) << BLUE << " at address in " RESET
                              << reg_to_string(addr) << std::endl;
                }

                w->emit_mov(mem{addr}, val);

                pool.free(val);
                pool.free(addr);

                is_dereference_next = false;

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

                w->emit_mov(lhs, rdx);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_div: {
                auto rhs = pool.pop(func, i);
                auto lhs = pool.pop(func, i);

                w->emit_mov(rax, lhs);
                w->emit_xor(rdx, rdx);
                w->emit_div(rhs);

                w->emit_mov(lhs, rax);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_mul: {
                auto rhs = pool.pop(func, i);
                auto lhs = pool.pop(func, i);

                w->emit_mov(rax, lhs);
                w->emit_mul(rhs);

                w->emit_mov(lhs, rax);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_imod: {
                auto rhs = pool.pop(func, i);
                auto lhs = pool.pop(func, i);

                w->emit_mov(rax, lhs);
                w->emit_xor(rdx, rdx);
                w->emit_idiv(rhs);

                w->emit_mov(lhs, rdx);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_idiv: {
                auto rhs = pool.pop(func, i);
                auto lhs = pool.pop(func, i);

                w->emit_mov(rax, lhs);
                w->emit_xor(rdx, rdx);
                w->emit_idiv(rhs);

                w->emit_mov(lhs, rax);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_imul: {
                auto rhs = pool.pop(func, i);
                auto lhs = pool.pop(func, i);

                w->emit_mov(rax, lhs);
                w->emit_imul(rhs);

                w->emit_mov(lhs, rax);

                pool.free(rhs);
                pool.push(lhs);

                break;
            }
            case ir_opcode::op_negate: {
                auto r = pool.pop(func, i);

                w->emit_neg(r);

                pool.push(r);

                break;
            }

            /* floating point arith */
            case ir_opcode::op_addf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_addss(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_addf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_addsd(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_subf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_subss(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_subf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_subsd(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_divf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_divss(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_divf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_divsd(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_mulf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_mulss(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_mulf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_mulsd(rhs, lhs);

                simd_pool.push(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_modf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                auto tmp1 = simd_pool.alloc();
                auto tmp2 = simd_pool.alloc();
                auto tmp_gpr1 = pool.alloc();

                w->emit_fmod_float(rhs, rhs, lhs, tmp1, tmp2, tmp_gpr1);

                simd_pool.push(rhs);

                simd_pool.free(lhs);
                simd_pool.free(tmp1);
                simd_pool.free(tmp2);
                pool.free(tmp_gpr1);

                break;
            }
            case ir_opcode::op_modf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                auto tmp1 = simd_pool.alloc();
                auto tmp2 = simd_pool.alloc();
                auto tmp_gpr1 = pool.alloc();

                w->emit_fmod_double(rhs, rhs, lhs, tmp1, tmp2, tmp_gpr1);

                simd_pool.push(rhs);

                simd_pool.free(lhs);
                simd_pool.free(tmp1);
                simd_pool.free(tmp2);
                pool.free(tmp_gpr1);

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
                if (!simd_pool.empty() && pool.empty() && !pushing_for_ret) {

                    auto result = simd_pool.pop(func, i);

                    if (func.type == "float64") {

                        w->emit_movsd(xmm0, result);
                    } else if (func.type == "float32") {
                        w->emit_movss(xmm0, result);
                    }

                    simd_pool.free(result);
                } else if (!pool.empty() && !pushing_for_ret) {
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
                } else {
                    w->emit_function_epilogue();
                    w->emit_ret();
                }
                break;
            }

            /* logical */
            case ir_opcode::label: {
                // we realloc the label location in here
                auto current_location = w->get_code().size();
                auto label_name = std::get<std::string>(code.operand);

                label_map[label_name] = current_location; // update label location

                break;
            }
            case ir_opcode::op_jmp: {
                auto jump_instr = code;
                jump_instr.operand = std::get<std::string>(code.operand);

                w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});

                jump_instructions.emplace_back(jump_instr, w->get_code().size() - 5);

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

                w->push_bytes(
                    {opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});

                jump_instructions.emplace_back(jump_instr, w->get_code().size() - 6);

                break;
            }
            case ir_opcode::op_cmpf64: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_comisd(lhs, rhs);

                simd_pool.free(rhs);
                simd_pool.free(lhs);

                break;
            }
            case ir_opcode::op_cmpf32: {
                auto rhs = simd_pool.pop(func, i);
                auto lhs = simd_pool.pop(func, i);

                w->emit_comiss(lhs, rhs);

                simd_pool.free(rhs);
                simd_pool.free(lhs);

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
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_setnz: {
                auto target = pool.alloc();

                w->emit_setnz(target);
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_setl: {
                auto target = pool.alloc();

                w->emit_setl(target);
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_setle: {
                auto target = pool.alloc();

                w->emit_setle(target);
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_setg: {
                auto target = pool.alloc();

                w->emit_setnl(target);
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_setge: {
                auto target = pool.alloc();

                w->emit_setnle(target);
                w->emit_mov(target, target);

                pool.push(target);

                break;
            }
            case ir_opcode::op_not: {
                auto r = pool.pop(func, i);

                w->emit_cmp(r, 0);
                w->emit_setz(r); // set r to 1 if equal (i.e., was 0), else 0
                pool.push(r);

                break;
            }

            /* function calls */
            case ir_opcode::op_call: {
                std::string func_name = std::get<std::string>(code.operand);

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Calling: " << YELLOW << "\"" << func_name
                              << "\"\n"
                              << RESET;
                }

                auto it = std::ranges::find_if(
                    ir_funcs, [&](const ir_function& f) { return f.name == func_name; });

                if (it != ir_funcs.end()) {
                    compile_function(*it, use_jit);
                }

                if (!function_map.contains(func_name)) {
                    std::cout << RED << "[CODEGEN ERROR] Function \"" << func_name
                              << "\" not found." << RESET << std::endl;
                    return;
                }

                int arg_count = it->args.size();
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Argument count: " << RESET << arg_count
                              << std::endl;
                }

                // collect arguments with their types
                std::vector<std::pair<std::variant<grp, simd128>, bool>> args; // pair<reg, is_fp>

                for (int i1 = arg_count - 1; i1 >= 0; i1--) {
                    const auto& arg_type = it->args[i1].type;
                    bool is_fp = (arg_type == "float32" || arg_type == "float64");

                    if (is_fp) {
                        if (simd_pool.empty()) {
                            std::cout << RED
                                      << "[CODEGEN ERROR] Not enough FP arguments on stack for "
                                         "function call: "
                                      << func_name << RESET << std::endl;
                            return;
                        }
                        args.emplace_back(simd_pool.pop(func, i1), true);
                    } else {
                        if (pool.empty()) {
                            std::cout << RED
                                      << "[CODEGEN ERROR] Not enough GP arguments on stack for "
                                         "function call: "
                                      << func_name << RESET << std::endl;
                            return;
                        }
                        args.emplace_back(pool.pop(func, i1), false);
                    }
                }

                std::reverse(args.begin(), args.end());

                // shadow space for Windows
                if (!is_systemv) {
                    w->emit_sub(rsp, 32);
                }

                std::vector<grp> caller_saved_gp;
                std::vector<simd128> caller_saved_simd;

                if (is_systemv) {
                    constexpr grp caller_saved[] = {r10, r11, r12, r13, r14, r15};
                    for (auto reg : caller_saved) {
                        auto active = pool.get_active_registers();
                        if (std::find(active.begin(), active.end(), reg) != active.end()) {
                            w->emit_push(reg);
                            caller_saved_gp.push_back(reg);
                        }
                    }

                    auto active_simd = simd_pool.get_active_registers();
                    for (auto xmm : active_simd) {
                        w->emit_sub(rsp, 16);
                        w->emit_movsd(mem{rsp, 0}, xmm);
                        caller_saved_simd.push_back(xmm);
                    }
                } else {
                    constexpr simd128 win_caller_saved[] = {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5};
                    auto active_simd = simd_pool.get_active_registers();
                    for (auto xmm : win_caller_saved) {
                        if (std::find(active_simd.begin(), active_simd.end(), xmm) !=
                            active_simd.end()) {
                            w->emit_sub(rsp, 16);
                            w->emit_movsd(mem{rsp, 0}, xmm);
                            caller_saved_simd.push_back(xmm);
                        }
                    }
                }

                std::size_t gp_reg_idx = 0;
                std::size_t fp_reg_idx = 0;
                std::vector<std::pair<std::variant<grp, simd128>, bool>> stack_args;

                for (std::size_t arg_idx = 0; arg_idx < args.size(); arg_idx++) {
                    auto [reg_var, is_fp] = args[arg_idx];

                    if (is_systemv) {
                        if (is_fp) {
                            if (fp_reg_idx < 8) {
                                simd128 arg_reg = std::get<simd128>(reg_var);
                                w->emit_movsd(sysv_regs_simd[fp_reg_idx], arg_reg);
                                simd_pool.free(arg_reg);
                                fp_reg_idx++;
                            } else {
                                stack_args.emplace_back(reg_var, is_fp);
                            }
                        } else {
                            if (gp_reg_idx < 6) {
                                grp arg_reg = std::get<grp>(reg_var);
                                w->emit_mov(sysv_regs[gp_reg_idx], arg_reg);
                                pool.free(arg_reg);
                                gp_reg_idx++;
                            } else {
                                stack_args.emplace_back(reg_var, is_fp);
                            }
                        }
                    } else {
                        constexpr grp win_regs[] = {rcx, rdx, r8, r9};
                        constexpr simd128 win_regs_simd[] = {xmm0, xmm1, xmm2, xmm3};

                        if (arg_idx < 4) {
                            if (is_fp) {
                                simd128 arg_reg = std::get<simd128>(reg_var);
                                w->emit_movsd(win_regs_simd[arg_idx], arg_reg);
                                simd_pool.free(arg_reg);
                            } else {
                                grp arg_reg = std::get<grp>(reg_var);
                                w->emit_mov(win_regs[arg_idx], arg_reg);
                                pool.free(arg_reg);
                            }
                        } else {
                            stack_args.emplace_back(reg_var, is_fp);
                        }
                    }
                }

                // pushing to stack reverse order
                for (auto it_stack = stack_args.rbegin(); it_stack != stack_args.rend();
                     ++it_stack) {
                    auto [reg_var, is_fp] = *it_stack;
                    if (is_fp) {
                        simd128 arg_reg = std::get<simd128>(reg_var);
                        w->emit_sub(rsp, 8);
                        w->emit_movsd(mem{rsp, 0}, arg_reg);
                        simd_pool.free(arg_reg);
                    } else {
                        grp arg_reg = std::get<grp>(reg_var);
                        w->emit_push(arg_reg);
                        pool.free(arg_reg);
                    }
                }

                // align to 16 bytes before call
                std::size_t stack_adjustment = 0;
                if (!stack_args.empty()) {
                    std::size_t pushed_bytes = stack_args.size() * 8;
                    if (pushed_bytes % 16 != 0) {
                        stack_adjustment = 16 - (pushed_bytes % 16);
                        w->emit_sub(rsp, stack_adjustment);
                    }
                }

                w->emit_mov(rax, reinterpret_cast<std::int64_t>(&function_map[func_name]));
                w->emit_call(mem{rax});

                if (!stack_args.empty()) {
                    w->emit_add(rsp, stack_args.size() * 8 + stack_adjustment);
                }

                // restore xmm
                for (auto it_simd = caller_saved_simd.rbegin(); it_simd != caller_saved_simd.rend();
                     ++it_simd) {
                    w->emit_movsd(*it_simd, mem{rsp, 0});
                    w->emit_add(rsp, 16);
                }

                // restore gpr
                if (is_systemv) {
                    for (auto it_gp = caller_saved_gp.rbegin(); it_gp != caller_saved_gp.rend();
                         ++it_gp) {
                        w->emit_pop(*it_gp);
                    }
                }

                // windows shadow space cleanup
                if (!is_systemv) {
                    w->emit_add(rsp, 32);
                }

                auto& ret_type = function_return_types[func_name];
                bool ret_is_fp = (ret_type == "float32" || ret_type == "float64");

                if (ret_is_fp) {
                    auto ret_reg = simd_pool.alloc();
                    w->emit_movsd(ret_reg, xmm0);
                    simd_pool.push(ret_reg);
                } else if (!ret_type.empty()) {
                    auto ret_reg = pool.alloc();
                    w->emit_mov(ret_reg, rax);
                    pool.push(ret_reg);
                }

                break;
            }

            /* arrays */
            case ir_opcode::op_array_decl: {
                auto name = std::get<std::string>(code.operand);
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Got name of array: " << RESET << name
                              << std::endl;
                }

                auto type = std::get<std::string>(func.code.at(++i).operand);
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Array type: " << RESET << type
                              << std::endl;
                }

                auto dimensions = std::get<std::uint64_t>(func.code.at(++i).operand);
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Dimensions count: " << RESET << dimensions
                              << std::endl;
                }

                std::vector<std::uint64_t> dimensions_vec;
                for (std::size_t j = 0; j < dimensions; j++) {
                    dimensions_vec.emplace_back(std::get<std::uint64_t>(func.code.at(++i).operand));
                }

                if (debug) {
                    int j = 0;
                    for (const auto& e : dimensions_vec) {
                        std::cout << BLUE << "[CODEGEN INFO] Size in dimension (" << ++j
                                  << "): " << RESET << e << std::endl;
                    }
                }

                std::size_t element_size = 8;
                // tmp to align with what we got until i can figure out diff byte sizes
                std::size_t element_count = 1;
                for (const auto& dim : dimensions_vec) {
                    element_count *= dim;
                }

                std::int32_t total_mem = element_count * element_size;
                total_mem = ((total_mem + 15) / 16) * 16; // 16 byte alignment
                w->emit_sub(rsp, total_mem);
                std::int32_t base_offset = -totalsizes - total_mem;
                totalsizes += total_mem;

                local_array_metadata[name] = {type, dimensions_vec, total_mem};
                local_variable_map[name] =
                    -base_offset; // store base address as positive offset from rbp

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Allocated array: " << RESET << name << "\n"
                              << BLUE << "               At index: " << RESET << -base_offset
                              << std::endl;
                }

                break;
            }
            case ir_opcode::op_declare_where_to_store: {
                auto index = std::get<std::uint64_t>(code.operand);
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Declaring store index: " << RESET << index
                              << std::endl;
                }

                index_to_push = index;

                break;
            }
            case ir_opcode::op_array_store_element: {
                auto content = pool.pop(func, i);
                std::uint64_t index = index_to_push;
                std::int32_t scale = -8 * (index + 1);

                auto array_name = std::get<std::string>(func.code.at(i).operand);
                auto it = local_variable_map.find(array_name);

                if (it == local_variable_map.end()) {
                    std::cerr << RED << "[CODEGEN ERROR] Array " << array_name << " not found"
                              << RESET << std::endl;
                    return;
                }

                w->emit_mov(mem{rbp, scale}, content);

                pool.free(content);

                index_to_push = 0;

                break;
            }
            case ir_opcode::op_array_access_element: {
                auto array_name = std::get<std::string>(code.operand);

                auto it = local_variable_map.find(array_name);
                if (it == local_variable_map.end()) {
                    std::cerr << RED << "[CODEGEN ERROR] Array " << array_name << " not found"
                              << RESET << std::endl;
                    return;
                }

                auto& local_metadata = local_array_metadata[array_name];

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Array access: " << RESET << array_name
                              << std::endl;
                    std::cout << BLUE << "[CODEGEN INFO] Array access total dims: " << RESET
                              << local_metadata.dimensions.size() << std::endl;
                }

                // pop all indices from the stack (they're in reverse order, last
                // dimension first)
                std::vector<grp> indices;
                for (std::size_t dims = 0; dims < local_metadata.dimensions.size(); dims++) {
                    indices.push_back(pool.pop(func, i));
                }

                // reverse to get them in correct order
                std::reverse(indices.begin(), indices.end());

                auto true_index = pool.alloc();
                w->emit_xor(true_index, true_index); // zero out

                for (std::size_t dim = 0; dim < local_metadata.dimensions.size(); dim++) {
                    std::uint64_t stride = 1;
                    for (std::size_t next_dim = dim + 1;
                         next_dim < local_metadata.dimensions.size(); next_dim++) {
                        stride *= local_metadata.dimensions[next_dim];
                    }

                    if (debug) {
                        std::cout << BLUE << "[CODEGEN INFO] Dimension " << dim
                                  << " stride: " << RESET << stride << std::endl;
                    }

                    if (stride == 1) {
                        w->emit_add(true_index, indices[dim]);
                    } else {
                        bool rax_in_use = (true_index == rax);
                        bool rdx_in_use = false;

                        for (const auto& active_reg : pool.get_active_registers()) {
                            if (active_reg == rdx) {
                                rdx_in_use = true;

                                break;
                            }
                        }

                        if (rax_in_use) {
                            w->emit_push(rax);
                        }
                        if (rdx_in_use) {
                            w->emit_push(rdx);
                        }

                        w->emit_mov(rax, indices[dim]);

                        if (stride <= 0x7FFFFFFF) {
                            w->emit_imul(rax, rax, static_cast<std::int32_t>(stride));
                        } else {
                            auto temp = pool.alloc();
                            w->emit_mov(temp, stride);
                            w->emit_mul(temp);
                            pool.free(temp);
                        }

                        w->emit_add(true_index, rax);

                        if (rdx_in_use) {
                            w->emit_pop(rdx);
                        }
                        if (rax_in_use) {
                            w->emit_pop(rax);
                        }
                    }

                    pool.free(indices[dim]);
                }

                w->emit_add(true_index, 1); // add 1 (1-indexed)
                w->emit_neg(true_index);    // negate to get negative offset

                w->emit_mov(true_index, mem{rbp, true_index, 3, 0}); // scale = 3 (8)
                pool.push(true_index);

                break;
            }
            /* structures */
            case ir_opcode::op_struct_decl: {
                break;
            }
            case ir_opcode::op_struct_store: {
                break;
            }

            case ir_opcode::op_member_access: {
                break;
            }

            case ir_opcode::op_member_store: {
                break;
            }
            default: {
                break;
            }
            }
        }

        if (debug) {
            std::cout << BLUE << "[CODEGEN INFO] Total jump instructions to backpatch: " << RESET
                      << jump_instructions.size() << std::endl;
        }

        for (auto& [fst, snd] : jump_instructions) {
            if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Jump Backpatching:\n" << RESET;
            }

            auto& instr = fst;
            auto jump_type = instr.op;
            auto label_name = std::get<std::string>(instr.operand);

            if (debug) {
                std::cout << "\tOpcode: " << opcode_to_string(jump_type) << std::endl;
                std::cout << "\tLabel: " << label_name << std::endl;

                if (label_map.contains(label_name)) {
                    std::cout << "\tLabel location: " << label_map[label_name] << std::endl;
                    std::cout << "\tJump location: " << snd << std::endl;
                    std::cout << "\tOffset will be: "
                              << (label_map[label_name] -
                                  (snd + ((jump_type == ir_opcode::op_jmp) ? 5 : 6)))
                              << RESET << std::endl;
                } else {
                    std::cout << RED << "\tERROR: Label not found!" << RESET << std::endl;
                }
            }

            if (label_map.contains(label_name)) {
                backpatch_jump(jump_type, snd, label_map[label_name], w);
            } else {
                std::cout << RED << "[CODEGEN ERROR] Label \"" << label_name << "\" not found."
                          << RESET << std::endl;
                return;
            }
        }
    }

    void compile_function(ir_function& func, const bool use_jit) {
        static std::unordered_set<std::string> compiling_functions;

        if (!func.type.empty()) {
            function_return_types[func.name] = func.type;
        }

        if (debug && function_map.contains(func.name)) {
            /*std::cout << BLUE << "[CODEGEN INFO] Symbol " << YELLOW << "\"" <<
               func.name << "\"" << BLUE << " already exists, probably already
               compiled or is compiling." << RESET << std::endl;*/
            return;
        }

        if (compiling_functions.contains(func.name)) {
            return;
        }

        compiling_functions.insert(func.name);

        if (debug) {
            std::cout << BLUE << "[CODEGEN INFO] Compiling function: " << YELLOW << "\""
                      << func.name << "\"\n"
                      << RESET;
        }

        auto w = std::make_unique<x86_64_writer>(debug);
        function_map.insert({func.name, reinterpret_cast<jit_function>(w->memory)});

        if (!func.uses_shellcode) {
            w->emit_function_prologue();
        }

        bool is_main = false;
        if (func.name == "main") {
            is_main = true;
        }

        generate_code(func, w.get(), is_main, use_jit);

        if (debug) {
            std::cout << GREEN << "[CODEGEN SUCCESS] Generated code for " << YELLOW << "\""
                      << func.name << "\"\n"
                      << RESET;
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

    explicit codegen(const std::vector<ir_function>& ir_funcs,
                     const std::vector<ir_struct>& ir_structs, const bool debug = false)
        : ir_funcs(ir_funcs), ir_structs(ir_structs), debug(debug) {
        for (const auto& s : ir_structs) {
        }
    }

    ~codegen() { writers.clear(); }

    void compile(const bool use_jit = true) {
        for (auto& func : ir_funcs) {
            compile_function(func, use_jit);
        }
    }
};
} // namespace occult::x86_64
