#pragma once

#include "ir_gen.hpp"
#include "x86_64_writer.hpp"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <variant>

/*
 * A class for stack-oriented codegen, redone
 */

// IMPORTANT (IGNORE FOR NOW?)
// there's a bug with stack sizing (call and totalsize, we must properly deal
// with subtracting rsp for variables and maintaining the correct stack size
// after calls)

namespace occult::x86_64 {
    static std::unordered_map<std::string, std::size_t> typename_sizes = {
        {"int64", 8}, {"int32", 4}, {"int16", 2}, {"int8", 1}, {"uint64", 8}, {"uint32", 4}, {"uint16", 2}, {"uint8", 1}, {"float32", 4}, {"float64", 8},
    }; /* we might have to use this instead of an 8-byte aligned stack, i'm not sure
        */
    static const std::unordered_map<std::string, std::string> cast_keyword_to_typename = {
        {"i8", "int8"}, {"i16", "int16"}, {"i32", "int32"}, {"i64", "int64"}, {"u8", "uint8"}, {"u16", "uint16"}, {"u32", "uint32"}, {"u64", "uint64"}, {"f32", "float32"}, {"f64", "float64"},
    };
    inline static std::vector<std::unique_ptr<std::uint8_t[]>> literal_pool;

    struct array_metadata {
        std::string type;
        std::vector<std::uint64_t> dimensions;
        std::int32_t total_size{};
    };

    struct struct_member_info {
        std::string name;
        std::string type;
        std::int32_t offset; // offset from struct base (0, 8, 16, ...)
    };

    struct struct_layout {
        std::string name;
        std::vector<struct_member_info> members;
        std::int32_t total_size{};

        std::int32_t get_member_offset(const std::string& member_name) const {
            for (const auto& m : members) {
                if (m.name == member_name) {
                    return m.offset;
                }
            }
            return -1;
        }
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
            bool r_debug;

        public:
            explicit register_pool(x86_64_writer* w, std::initializer_list<RegT> initial_regs) : initial_free_regs(initial_regs), writer(w), r_debug(w->debug) { free_regs = initial_free_regs; }

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
                                }
                                else if constexpr (std::is_same_v<RegT, simd128>) {
                                    writer->emit_movsd(mem{rbp, spill_offset}, r); // or emit_movaps if aligned
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

            RegT pop() {
                if (reg_stack.empty()) {
                    /*if (r_debug) {
                        writer->print_bytes();
                    }*/
                    RegT r = alloc();
                    if constexpr (std::is_same_v<RegT, grp>) {
                        writer->emit_xor(r, r);

                        /*if (r_debug) {
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
                        }*/
                    }
                    return r;
                }

                stack_value val = reg_stack.back();
                reg_stack.pop_back();

                if (val.is_spilled) {
                    RegT r = alloc();

                    if constexpr (std::is_same_v<RegT, grp>) {
                        writer->emit_mov(r, mem{rbp, val.offset});
                    }
                    else if constexpr (std::is_same_v<RegT, simd128>) {
                        writer->emit_movsd(r, mem{rbp, val.offset});
                    }

                    return r;
                }
                else {
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
                    }
                    else if constexpr (std::is_same_v<RegT, simd128>) {
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
        std::unordered_map<std::string, struct_layout> struct_layouts;

        bool debug;

        static void backpatch_jump(const ir_opcode op, const std::size_t location, std::size_t label_location, x86_64_writer* w) {
            const std::int32_t offset = label_location - (location + ((op == ir_opcode::op_jmp) ? 5 : 6));
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
                code[location + offset_start + i] = static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
            }
        }

        void generate_code(ir_function& func, x86_64_writer* w, bool is_main, bool use_jit) {
            register_pool_gp pool(w, {r10, r11, r12, r13, r14, r15, rbx});
            // Pool allocates from back of list: xmm1 first, then xmm2-7, then xmm0 (reserved
            // as last-resort so it stays free for function returns/args), then xmm8-15.
            register_pool_simd simd_pool(w, {xmm15, xmm14, xmm13, xmm12, xmm11, xmm10, xmm9, xmm8, xmm0, xmm7, xmm6, xmm5, xmm4, xmm3, xmm2, xmm1});

            std::unordered_map<std::string, std::size_t> label_map;
            std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;

            std::unordered_map<std::string, std::int64_t> local_variable_map;
            std::unordered_map<std::string, std::string> local_variable_map_types;

            std::unordered_map<std::string, array_metadata> local_array_metadata;

            // struct tracking: variable name -> struct type name
            std::unordered_map<std::string, std::string> local_struct_var_types;
            std::string pending_struct_type; // set by op_struct_decl, consumed by op_struct_store

            bool is_reference_next = false;
            bool is_dereference_next = false;
            bool is_dereference_assign_next = false;
            std::size_t deref_count_normal = 0;
            std::size_t deref_count_assign = 0;

            const bool save_callee_saved = !func.uses_shellcode;
            const std::int32_t callee_saved_space = save_callee_saved ? 8 * 5 : 0;

            std::int32_t totalsizes = callee_saved_space;

            pool.set_stack_size_tracker(&totalsizes); // register spilling
            simd_pool.set_stack_size_tracker(&totalsizes);

            // reserve placeholder for stack allocation right after prologue
            // we'll patch this later with the actual totalsizes value
            std::size_t stack_alloc_placeholder_location = 0;
            if (!func.uses_shellcode) {
                stack_alloc_placeholder_location = w->get_code().size();
                w->emit_sub(rsp, callee_saved_space);

                if (save_callee_saved) {
                    // save callee-saved registers r12-r15
                    w->emit_mov(mem{rbp, -8}, rbx);
                    w->emit_mov(mem{rbp, -16}, r12);
                    w->emit_mov(mem{rbp, -24}, r13);
                    w->emit_mov(mem{rbp, -32}, r14);
                    w->emit_mov(mem{rbp, -40}, r15);
                }
            }

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
                                w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, simd_tmp);
                                simd_pool.free(simd_tmp);
                                fp_arg_idx++;
                            }
                            else { // stack
                                auto simd_tmp = simd_pool.alloc();
                                w->emit_movsd(simd_tmp, mem{rbp, static_cast<std::int32_t>(stack_arg_offset)});
                                w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, simd_tmp);
                                simd_pool.free(simd_tmp);
                                stack_arg_offset += 8;
                            }
                        }
                        else { // gpr for integers
                            if (gp_arg_idx < 6) {
                                auto r = pool.alloc();
                                w->emit_mov(r, sysv_regs[gp_arg_idx]);
                                w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                                pool.free(r);
                                gp_arg_idx++;
                            }
                            else { // stack
                                auto r = pool.alloc();
                                w->emit_mov(r, mem{rbp, static_cast<std::int32_t>(stack_arg_offset)});
                                w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                                pool.free(r);
                                stack_arg_offset += 8;
                            }
                        }
                    }
                    else { // windows (yuck)
                        constexpr grp win_regs[] = {rcx, rdx, r8, r9};
                        constexpr simd128 win_regs_simd[] = {xmm0, xmm1, xmm2, xmm3};

                        if (is_fp) {
                            if (i < 4) {
                                auto simd_tmp = simd_pool.alloc();
                                w->emit_movsd(simd_tmp, win_regs_simd[i]);
                                w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, simd_tmp);
                                simd_pool.free(simd_tmp);
                            }
                            else { // stack
                                auto simd_tmp = simd_pool.alloc();
                                w->emit_movsd(simd_tmp, mem{rbp, static_cast<std::int32_t>(32 + (i - 4) * 8 + 16)});
                                w->emit_movsd(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, simd_tmp);
                                simd_pool.free(simd_tmp);
                            }
                        }
                        else {
                            if (i < 4) {
                                auto r = pool.alloc();
                                w->emit_mov(r, win_regs[i]);
                                w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                                pool.free(r);
                            }
                            else {
                                auto r = pool.alloc();
                                w->emit_mov(r, mem{rbp, static_cast<std::int32_t>(32 + (i - 4) * 8 + 16)});
                                w->emit_mov(mem{rbp, -static_cast<std::int32_t>(totalsizes)}, r);
                                pool.free(r);
                            }
                        }
                    }

                    local_variable_map.insert({func.args[i].name, totalsizes});
                    local_variable_map_types.insert({func.args[i].name, arg_type});
                }
            }

            // for variadic functions, create a __varargs array from the __va* args
            if (func.is_variadic && !func.uses_shellcode) {
                std::vector<std::string> va_arg_names;
                for (const auto& arg : func.args) {
                    if (arg.name.size() >= 4 && arg.name.substr(0, 4) == "__va") {
                        va_arg_names.push_back(arg.name);
                    }
                }

                if (!va_arg_names.empty()) {
                    std::size_t va_count = va_arg_names.size();
                    std::int32_t total_mem = static_cast<std::int32_t>(va_count * 8);
                    total_mem = ((total_mem + 15) / 16) * 16; // 16-byte alignment

                    std::int32_t base_offset = -totalsizes - total_mem;
                    totalsizes += total_mem;

                    // copy each __va arg value into the contiguous array
                    for (std::size_t j = 0; j < va_count; j++) {
                        auto va_offset = local_variable_map[va_arg_names[j]];
                        auto r = pool.alloc();
                        w->emit_mov(r, mem{rbp, -static_cast<std::int32_t>(va_offset)});
                        std::int32_t element_offset = base_offset + static_cast<std::int32_t>(j * 8);
                        w->emit_mov(mem{rbp, element_offset}, r);
                        pool.free(r);
                    }

                    local_variable_map["__varargs"] = base_offset;
                    local_variable_map_types["__varargs"] = "int64";
                    local_array_metadata["__varargs"] = {"int64", {va_count}, total_mem};
                }
            }

            bool pushing_for_ret = false;
            bool push_single = false;
            std::uint64_t index_to_push = (std::numeric_limits<std::uint64_t>::max)();
            for (std::size_t i = 0; i < func.code.size(); i++) {
                auto& code = func.code.at(i);

                switch (code.op) {
                case ir_opcode::op_push_shellcode:
                    {
                        if (std::holds_alternative<std::uint8_t>(code.operand)) {
                            std::uint8_t val = std::get<std::uint8_t>(code.operand);
                            w->push_byte(val);
                        }

                        break;
                    }
                case ir_opcode::op_push:
                    {
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
                        else if (std::holds_alternative<std::int32_t>(code.operand)) {
                            std::int32_t val = std::get<std::int32_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, val);
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::uint32_t>(code.operand)) {
                            std::uint32_t val = std::get<std::uint32_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, val);
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::int16_t>(code.operand)) {
                            std::int16_t val = std::get<std::int16_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, static_cast<std::int64_t>(val));
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::uint16_t>(code.operand)) {
                            std::uint16_t val = std::get<std::uint16_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, static_cast<std::uint64_t>(val));
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::int8_t>(code.operand)) {
                            std::int8_t val = std::get<std::int8_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, static_cast<std::int64_t>(val));
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::uint8_t>(code.operand)) {
                            std::uint8_t val = std::get<std::uint8_t>(code.operand);
                            grp r = pool.alloc();
                            w->emit_mov(r, static_cast<std::uint64_t>(val));
                            pool.push(r);
                        }
                        else if (std::holds_alternative<std::string>(code.operand)) {
                            const std::string& str = std::get<std::string>(code.operand);
                            constexpr std::size_t header_size = 8;
                            // pad an extra 7 bytes after the null terminator so 64-bit
                            const std::size_t storage_size = header_size + str.size() + 8;

                            auto buf = std::make_unique<std::uint8_t[]>(storage_size);
                            std::memset(buf.get(), 0, storage_size);
                            *reinterpret_cast<std::uint64_t*>(buf.get()) = str.size();
                            std::memcpy(buf.get() + header_size, str.data(), str.size());

                            auto literal_ptr = buf.get() + header_size;
                            std::uint64_t host_addr = reinterpret_cast<std::uint64_t>(literal_ptr);

                            string_literals[host_addr] = str;

                            literal_pool.push_back(std::move(buf));

                            grp r = pool.alloc();
                            w->emit_mov(r, reinterpret_cast<std::int64_t>(literal_ptr));
                            pool.push(r);
                        }
                        else if (std::holds_alternative<double>(code.operand)) {
                            const double& lf = std::get<double>(code.operand);
                            simd128 xmm = simd_pool.alloc();
                            grp r = pool.alloc();

                            w->emit_mov(r, double_to_bits(lf));
                            w->emit_movq(xmm, r);

                            pool.free(r);
                            simd_pool.push(xmm);
                        }
                        else if (std::holds_alternative<float>(code.operand)) {
                            const float& f = std::get<float>(code.operand);

                            simd128 xmm = simd_pool.alloc();
                            grp r = pool.alloc();

                            w->emit_mov(as_32(r), float_to_bits(f));
                            w->emit_movd(xmm, as_32(r));

                            pool.free(r);
                            simd_pool.push(xmm);
                        }

                        break;
                    }
                case ir_opcode::op_push_for_ret:
                    {
                        if (std::holds_alternative<std::int64_t>(code.operand)) {
                            std::int64_t val = std::get<std::int64_t>(code.operand);
                            w->emit_mov(rax, val);

                            pushing_for_ret = true;
                        }

                        break;
                    }
                case ir_opcode::op_push_single:
                    {
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
                case ir_opcode::op_store:
                    {
                        auto var_name = std::get<std::string>(code.operand);
                        auto var_type = code.type;
                        auto it = local_variable_map.find(var_name);

                        if (pool.empty() && !simd_pool.empty()) {
                            auto simd_reg = simd_pool.pop();
                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Floating point store" << RESET << std::endl;
                            }

                            if (it == local_variable_map.end()) {
                                // variable doesn't exist yet
                                if (debug) {
                                    std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET << var_name << std::endl;
                                    std::cout << "\tSize: " << typename_sizes[var_type] << std::endl;
                                }

                                totalsizes += 8;

                                if (var_type == "float32") {
                                    w->emit_movss(mem{rbp, -totalsizes}, simd_reg);
                                }
                                else if (var_type == "float64") {
                                    w->emit_movsd(mem{rbp, -totalsizes}, simd_reg);
                                }

                                simd_pool.free(simd_reg);

                                local_variable_map.insert({var_name, totalsizes});
                                local_variable_map_types.insert({var_name, var_type});

                                break;
                            }
                        }

                        grp r = (push_single) ? pool.top() : pool.pop();
                        push_single = false;

                        if (it == local_variable_map.end()) {
                            // variable doesn't exist yet
                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Emitting variable: " << RESET << var_name << std::endl;
                                std::cout << "\tSize: " << typename_sizes[var_type] << std::endl;
                            }

                            totalsizes += 8;

                            if (is_reference_next) {
                                std::cout << RED << "[CODEGEN ERROR] Cannot have reference in store." << RESET << std::endl;
                            }

                            if (var_type == "int8" || var_type == "uint8" || var_type == "bool") {
                                w->emit_mov(mem{bpl, -totalsizes}, as_8(r));
                            }
                            else if (var_type == "int16" || var_type == "uint16") {
                                w->emit_mov(mem{bp, -totalsizes}, as_16(r));
                            }
                            else if (var_type == "int32" || var_type == "uint32") {
                                w->emit_mov(mem{ebp, -totalsizes}, as_32(r));
                            }
                            else {
                                w->emit_mov(mem{rbp, -totalsizes}, r);
                            }

                            pool.free(r);

                            local_variable_map.insert({var_name, totalsizes});
                            local_variable_map_types.insert({var_name, var_type});
                        }
                        else {
                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Re-emitting variable: " << RESET << var_name << std::endl;
                            }

                            std::int32_t offset = -it->second;

                            // check if this is a struct reassignment (full struct, not pointer)
                            if (local_struct_var_types.contains(var_name)) {
                                auto struct_type = local_struct_var_types[var_name];
                                auto layout_it = struct_layouts.find(struct_type);
                                if (layout_it != struct_layouts.end()) {
                                    // r contains a pointer to source struct data - copy it
                                    grp tmp = pool.alloc();
                                    std::int32_t actual_size = layout_it->second.total_size;
                                    for (std::int32_t off = 0; off < actual_size; off += 8) {
                                        w->emit_mov(tmp, mem{r, off});
                                        w->emit_mov(mem{rbp, offset + off}, tmp);
                                    }
                                    pool.free(tmp);
                                    pool.free(r);
                                }
                                else {
                                    w->emit_mov(mem{rbp, offset}, r);
                                    pool.free(r);
                                }
                            }
                            else if (var_type == "int8" || var_type == "uint8") {
                                w->emit_mov(mem{bpl, offset}, as_8(r));
                                pool.free(r);
                            }
                            else if (var_type == "int16" || var_type == "uint16") {
                                w->emit_mov(mem{bp, offset}, as_16(r));
                                pool.free(r);
                            }
                            else if (var_type == "int32" || var_type == "uint32") {
                                w->emit_mov(mem{ebp, offset}, as_32(r));
                                pool.free(r);
                            }
                            else {
                                w->emit_mov(mem{rbp, offset}, r);
                                pool.free(r);
                            }

                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Variable location: " << RESET << offset << std::endl;
                            }
                        }

                        break;
                    }
                case ir_opcode::op_load:
                    {
                        const auto& var_name = std::get<std::string>(code.operand);
                        const auto& var_type = local_variable_map_types[var_name];
                        auto it = local_variable_map.find(var_name);

                        if (it == local_variable_map.end()) {
                            std::cerr << RED << "[CODEGEN ERROR] Attempted to load undeclared variable: " << RESET << var_name << std::endl;
                            return;
                        }

                        std::int32_t offset = -it->second;

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Loading: " << RESET << var_name << "\n\tLocation: " << offset << "\n\tType: " << YELLOW << var_type << RESET << std::endl;
                        }

                        bool is_float_type = (var_type == "float32" || var_type == "float64");
                        grp r;

                        if (!is_float_type) {
                            r = pool.alloc();
                        }

                        bool loaded = false;

                        if (is_reference_next) {
                            if (!is_float_type) {
                                w->emit_lea(r, mem{rbp, offset});
                                is_reference_next = false;
                                pool.push(r);
                            }
                            break;
                        }

                        if (is_dereference_next) {
                            if (!is_float_type) {
                                for (std::size_t size = 1; size <= deref_count_normal; size++) {
                                    if (size == 1) {
                                        w->emit_mov(r, mem{rbp, offset});
                                        w->emit_mov(r, mem{r});
                                    }
                                    else {
                                        w->emit_mov(r, mem{r});
                                    }
                                }

                                is_dereference_next = false;
                                deref_count_normal = 0;
                                pool.push(r);
                            }
                            break;
                        }

                        if (is_dereference_assign_next) {
                            if (!is_float_type) { // FIX 4: Only use r if it was allocated
                                for (std::size_t size = 1; size <= deref_count_assign; size++) {
                                    if (size == 1) {
                                        w->emit_mov(r, mem{rbp, offset});
                                    }
                                    else {
                                        w->emit_mov(r, mem{r});
                                    }
                                }

                                deref_count_assign = 0;
                                is_dereference_assign_next = false;
                                pool.push(r);
                            }
                            break;
                        }

                        if (var_type == "int8" || var_type == "bool") {
                            w->emit_movsx(r, mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "uint8") {
                            w->emit_movzx(r, mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "int16") {
                            w->emit_movsx(r, mem{rbp, offset}, false);
                            loaded = true;
                        }
                        else if (var_type == "uint16") {
                            w->emit_movzx(r, mem{rbp, offset}, false);
                            loaded = true;
                        }
                        else if (var_type == "int32") {
                            w->emit_movsxd(r, mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "uint32") {
                            w->emit_mov(as_32(r), mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "int64" || var_type == "uint64") {
                            w->emit_mov(r, mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "string") {
                            w->emit_mov(r, mem{rbp, offset});
                            loaded = true;
                        }
                        else if (var_type == "float64") {
                            auto simd_reg = simd_pool.alloc();
                            w->emit_movsd(simd_reg, mem{rbp, offset});
                            simd_pool.push(simd_reg);
                            break;
                        }
                        else if (var_type == "float32") {
                            auto simd_reg = simd_pool.alloc();
                            w->emit_movss(simd_reg, mem{rbp, offset});
                            simd_pool.push(simd_reg);
                            break;
                        }

                        // check if this is a struct variable (not a pointer to struct)
                        if (!loaded && local_struct_var_types.contains(var_name)) {
                            // for struct variables load the base address
                            w->emit_lea(r, mem{rbp, offset});
                            loaded = true;
                        }

                        if (!loaded) {
                            w->emit_mov(r, mem{rbp, offset});
                        }

                        if (!is_float_type) {
                            pool.push(r);
                        }

                        break;
                    }
                case ir_opcode::op_reference:
                    {
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
                case ir_opcode::op_dereference:
                    {
                        auto operand = std::get<std::int64_t>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Dereference count: " << RESET << operand << std::endl;
                        }

                        grp addr = pool.pop(); // consume pointer

                        for (std::int64_t count = 0; count < operand; count++) {
                            w->emit_mov(addr, mem{addr});
                        }

                        pool.push(addr);

                        break;
                    }
                case ir_opcode::op_dereference_assign:
                    {
                        auto operand = std::get<std::int64_t>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Dereference (assignment) count: " << RESET << operand << std::endl;
                        }

                        is_dereference_assign_next = true;
                        deref_count_assign = operand;

                        break;
                    }
                case ir_opcode::op_store_at_addr:
                    {
                        auto val = pool.pop();
                        auto addr = pool.pop();

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Storing value in " << RESET << reg_to_string(val) << BLUE << " at address in " RESET << reg_to_string(addr) << std::endl;
                        }

                        w->emit_mov(mem{addr}, val);

                        pool.free(val);
                        pool.free(addr);

                        is_dereference_next = false;

                        break;
                    }

                /* arith */
                case ir_opcode::op_add:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_add(lhs, rhs);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_sub:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_sub(lhs, rhs);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_mod:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_xor(rdx, rdx);
                        w->emit_div(rhs);

                        w->emit_mov(lhs, rdx);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_div:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_xor(rdx, rdx);
                        w->emit_div(rhs);

                        w->emit_mov(lhs, rax);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_mul:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_mul(rhs);

                        w->emit_mov(lhs, rax);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_imod:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_xor(rdx, rdx);
                        w->emit_idiv(rhs);

                        w->emit_mov(lhs, rdx);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_idiv:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_xor(rdx, rdx);
                        w->emit_idiv(rhs);

                        w->emit_mov(lhs, rax);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_imul:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rax, lhs);
                        w->emit_imul(rhs);

                        w->emit_mov(lhs, rax);

                        pool.free(rhs);
                        pool.push(lhs);

                        break;
                    }
                case ir_opcode::op_negate:
                    {
                        auto r = pool.pop();

                        w->emit_neg(r);

                        pool.push(r);

                        break;
                    }

                case ir_opcode::op_cast:
                    {
                        const auto& raw_target = std::get<std::string>(code.operand);
                        auto norm_it = cast_keyword_to_typename.find(raw_target);
                        const std::string& target_type = (norm_it != cast_keyword_to_typename.end()) ? norm_it->second : raw_target;
                        const bool target_is_float32 = (target_type == "float32");
                        const bool target_is_float64 = (target_type == "float64");
                        const bool target_is_float = target_is_float32 || target_is_float64;

                        // Determine source float type from IR type annotation (may be empty for unknown)
                        auto src_norm_it = cast_keyword_to_typename.find(code.type);
                        const std::string& src_type = (src_norm_it != cast_keyword_to_typename.end()) ? src_norm_it->second : code.type;
                        const bool source_is_f64 = (src_type == "float64");
                        const bool source_is_f32 = (src_type == "float32");
                        // Use the IR type annotation when available; fall back to pool heuristic
                        const bool source_is_float = (src_type.empty()) ? (pool.empty() && !simd_pool.empty()) : (source_is_f32 || source_is_f64);

                        if (source_is_float && target_is_float) {
                            // float <-> float conversion
                            auto src = simd_pool.pop();
                            auto dst = simd_pool.alloc();
                            if (target_is_float64) {
                                w->emit_cvtss2sd(dst, src); // float32 -> float64
                            }
                            else {
                                w->emit_cvtsd2ss(dst, src); // float64 -> float32
                            }
                            simd_pool.free(src);
                            simd_pool.push(dst);
                        }
                        else if (source_is_float && !target_is_float) {
                            // float -> int: use float64 path if source is f64, else float32
                            auto src = simd_pool.pop();
                            auto dst = pool.alloc();
                            if (source_is_f64) {
                                w->emit_cvttsd2si(dst, src);
                            }
                            else {
                                w->emit_cvttss2si(dst, src); // float32 -> int (default)
                            }
                            simd_pool.free(src);
                            pool.push(dst);
                        }
                        else if (!source_is_float && target_is_float) {
                            // int -> float conversion
                            auto src = pool.pop();
                            auto dst = simd_pool.alloc();
                            if (target_is_float32) {
                                w->emit_cvtsi2ss(dst, src);
                            }
                            else {
                                w->emit_cvtsi2sd(dst, src);
                            }
                            pool.free(src);
                            simd_pool.push(dst);
                        }
                        else {
                            // int -> int conversion: sign/zero extend to proper width
                            auto r = pool.pop();
                            if (target_type == "int8") {
                                w->emit_movsx(r, r, true); // sign-extend 8->64
                            }
                            else if (target_type == "uint8") {
                                w->emit_movzx(r, r, true); // zero-extend 8->64
                            }
                            else if (target_type == "int16") {
                                w->emit_movsx(r, r, false); // sign-extend 16->64
                            }
                            else if (target_type == "uint16") {
                                w->emit_movzx(r, r, false); // zero-extend 16->64
                            }
                            else if (target_type == "int32") {
                                w->emit_movsxd(r, r); // sign-extend 32->64
                            }
                            else if (target_type == "uint32") {
                                w->emit_mov(as_32(r), as_32(r)); // zero-extend 32->64
                            }
                            // int64/uint64: no-op
                            pool.push(r);
                        }

                        break;
                    }

                case ir_opcode::op_bitcast:
                    {
                        const auto& target_type = std::get<std::string>(code.operand);
                        const auto& source_type = code.type;

                        const bool target_is_float = (target_type == "float32" || target_type == "float64");
                        const bool source_is_float = (source_type == "float32" || source_type == "float64");

                        if (source_is_float && !target_is_float) {
                            // float -> int: movq gp_reg, xmm_reg (bit-preserving)
                            auto src = simd_pool.pop();
                            auto dst = pool.alloc();
                            w->emit_movq(dst, src);
                            simd_pool.free(src);
                            pool.push(dst);
                        }
                        else if (!source_is_float && target_is_float) {
                            // int -> float: movq xmm_reg, gp_reg (bit-preserving)
                            auto src = pool.pop();
                            auto dst = simd_pool.alloc();
                            w->emit_movq(dst, src);
                            pool.free(src);
                            simd_pool.push(dst);
                        }
                        // same-type bitcast is a no-op (value already in correct pool)

                        break;
                    }

                /* floating point arith */
                case ir_opcode::op_addf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_addss(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_addf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_addsd(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_subf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_subss(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_subf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_subsd(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_divf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_divss(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_divf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_divsd(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_mulf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_mulss(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_mulf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_mulsd(lhs, rhs);

                        simd_pool.push(lhs);
                        simd_pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_modf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

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
                case ir_opcode::op_modf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

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
                case ir_opcode::op_bitwise_and:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_and(lhs, rhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_bitwise_or:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_or(lhs, rhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_bitwise_xor:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_xor(lhs, rhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_bitwise_not:
                    {
                        auto r = pool.pop();

                        w->emit_not(r);

                        pool.push(r);

                        break;
                    }
                case ir_opcode::op_bitwise_lshift:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rcx, rhs);
                        w->emit_shl(lhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_bitwise_rshift:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rcx, rhs);
                        w->emit_shr(lhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }
                case ir_opcode::op_ibitwise_rshift:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_mov(rcx, rhs);
                        w->emit_sar(lhs);

                        pool.push(lhs);
                        pool.free(rhs);

                        break;
                    }

                case ir_opcode::op_ret:
                    {
                        bool is_float_return = (func.type == "float32" || func.type == "float64");

                        if (!pushing_for_ret) {
                            if (is_float_return) {
                                if (!simd_pool.empty()) {
                                    auto result = simd_pool.pop();
                                    if (func.type == "float64") {
                                        w->emit_movsd(xmm0, result);
                                    }
                                    else if (func.type == "float32") {
                                        w->emit_movss(xmm0, result);
                                    }
                                    simd_pool.free(result);
                                }
                            }
                            else {
                                if (!pool.empty()) {
                                    auto result = pool.pop();
                                    w->emit_mov(rax, result);
                                    pool.free(result);
                                }
                            }
                        }

                        if (save_callee_saved) {
                            w->emit_mov(rbx, mem{rbp, -8});
                            w->emit_mov(r12, mem{rbp, -16});
                            w->emit_mov(r13, mem{rbp, -24});
                            w->emit_mov(r14, mem{rbp, -32});
                            w->emit_mov(r15, mem{rbp, -40});
                        }

                        if (!func.uses_shellcode) {
                            if (!use_jit && is_main) {
                                w->emit_pop(rax);
                                w->emit_function_epilogue();
                                w->emit_mov(rdi, rax);
                                w->emit_mov(rax, int64_t(60));
                                w->emit_syscall();
                            }
                            else {
                                w->emit_function_epilogue();
                                w->emit_ret();
                            }
                        }
                        else {
                            w->emit_ret();
                        }

                        break;
                    }

                /* logical */
                case ir_opcode::label:
                    {
                        // we realloc the label location in here
                        auto current_location = w->get_code().size();
                        auto label_name = std::get<std::string>(code.operand);

                        label_map[label_name] = current_location; // update label location

                        break;
                    }
                case ir_opcode::op_jmp:
                    {
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
                case ir_opcode::op_jnz:
                    {
                        auto jump_instr = code;
                        jump_instr.operand = std::get<std::string>(code.operand);

                        w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});

                        jump_instructions.emplace_back(jump_instr, w->get_code().size() - 6);

                        break;
                    }
                case ir_opcode::op_cmpf64:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_comisd(lhs, rhs);

                        simd_pool.free(rhs);
                        simd_pool.free(lhs);

                        break;
                    }
                case ir_opcode::op_cmpf32:
                    {
                        auto rhs = simd_pool.pop();
                        auto lhs = simd_pool.pop();

                        w->emit_comiss(lhs, rhs);

                        simd_pool.free(rhs);
                        simd_pool.free(lhs);

                        break;
                    }
                case ir_opcode::op_cmp:
                    {
                        auto rhs = pool.pop();
                        auto lhs = pool.pop();

                        w->emit_cmp(lhs, rhs);

                        pool.free(rhs);
                        pool.free(lhs);

                        break;
                    }
                case ir_opcode::op_setz:
                    {
                        auto target = pool.alloc();

                        w->emit_setz(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_setnz:
                    {
                        auto target = pool.alloc();

                        w->emit_setnz(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_setl:
                    {
                        auto target = pool.alloc();

                        w->emit_setl(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_setle:
                    {
                        auto target = pool.alloc();

                        w->emit_setle(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_setg:
                    {
                        auto target = pool.alloc();

                        w->emit_setnle(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_setge:
                    {
                        auto target = pool.alloc();

                        w->emit_setnl(target);
                        w->emit_mov(target, target);

                        pool.push(target);

                        break;
                    }
                case ir_opcode::op_not:
                    {
                        auto r = pool.pop();

                        w->emit_cmp(r, 0);
                        w->emit_setz(r); // set r to 1 if equal (i.e., was 0), else 0
                        pool.push(r);

                        break;
                    }

                /* function calls */
                case ir_opcode::op_call:
                    {
                        std::string func_name = std::get<std::string>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Calling: " << YELLOW << "\"" << func_name << "\"\n" << RESET;
                        }

                        auto it = std::ranges::find_if(ir_funcs, [&](const ir_function& f) { return f.name == func_name; });

                        if (it != ir_funcs.end()) {
                            compile_function(*it, use_jit);
                        }

                        if (!function_map.contains(func_name)) {
                            std::cout << RED << "[CODEGEN ERROR] Function \"" << func_name << "\" not found." << RESET << std::endl;
                            return;
                        }

                        int arg_count = it->args.size();
                        bool is_variadic_call = it->is_variadic && !code.type.empty();
                        bool is_external_call = it->is_external && !code.type.empty();
                        if (is_variadic_call || is_external_call) {
                            arg_count = std::stoi(code.type);
                        }
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Argument count: " << RESET << arg_count << std::endl;
                        }

                        // collect arguments with their types
                        std::vector<std::pair<std::variant<grp, simd128>, bool>> args; // pair<reg, is_fp>

                        for (int i1 = arg_count - 1; i1 >= 0; i1--) {
                            bool is_fp = false;
                            if (static_cast<std::size_t>(i1) < it->args.size()) {
                                const auto& arg_type = it->args[i1].type;
                                is_fp = (arg_type == "float32" || arg_type == "float64");
                            }

                            if (is_fp) {
                                if (simd_pool.empty()) {
                                    std::cout << RED
                                              << "[CODEGEN ERROR] Not enough FP arguments on stack for "
                                                 "function call: "
                                              << func_name << RESET << std::endl;
                                    // FIX 8: Free already collected arguments before returning
                                    for (auto& [reg_var, is_arg_fp] : args) {
                                        if (is_arg_fp) {
                                            simd_pool.free(std::get<simd128>(reg_var));
                                        }
                                        else {
                                            pool.free(std::get<grp>(reg_var));
                                        }
                                    }
                                    return;
                                }
                                args.emplace_back(simd_pool.pop(), true);
                            }
                            else {
                                if (pool.empty()) {
                                    std::cout << RED
                                              << "[CODEGEN ERROR] Not enough GP arguments on stack for "
                                                 "function call: "
                                              << func_name << RESET << std::endl;
                                    // FIX 9: Free already collected arguments before returning
                                    for (auto& [reg_var, is_arg_fp] : args) {
                                        if (is_arg_fp) {
                                            simd_pool.free(std::get<simd128>(reg_var));
                                        }
                                        else {
                                            pool.free(std::get<grp>(reg_var));
                                        }
                                    }
                                    return;
                                }
                                args.emplace_back(pool.pop(), false);
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
                        }
                        else {
                            constexpr simd128 win_caller_saved[] = {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5};
                            auto active_simd = simd_pool.get_active_registers();
                            for (auto xmm : win_caller_saved) {
                                if (std::find(active_simd.begin(), active_simd.end(), xmm) != active_simd.end()) {
                                    w->emit_sub(rsp, 16);
                                    w->emit_movsd(mem{rsp, 0}, xmm);
                                    caller_saved_simd.push_back(xmm);
                                }
                            }
                        }

                        std::size_t gp_reg_idx = 0;
                        std::size_t fp_reg_idx = 0;
                        const std::size_t caller_saved_bytes = caller_saved_gp.size() * 8 + caller_saved_simd.size() * 16;
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
                                    }
                                    else {
                                        stack_args.emplace_back(reg_var, is_fp);
                                    }
                                }
                                else {
                                    if (gp_reg_idx < 6) {
                                        grp arg_reg = std::get<grp>(reg_var);
                                        w->emit_mov(sysv_regs[gp_reg_idx], arg_reg);
                                        pool.free(arg_reg);
                                        gp_reg_idx++;
                                    }
                                    else {
                                        stack_args.emplace_back(reg_var, is_fp);
                                    }
                                }
                            }
                            else {
                                constexpr grp win_regs[] = {rcx, rdx, r8, r9};
                                constexpr simd128 win_regs_simd[] = {xmm0, xmm1, xmm2, xmm3};

                                if (arg_idx < 4) {
                                    if (is_fp) {
                                        simd128 arg_reg = std::get<simd128>(reg_var);
                                        w->emit_movsd(win_regs_simd[arg_idx], arg_reg);
                                        simd_pool.free(arg_reg);
                                    }
                                    else {
                                        grp arg_reg = std::get<grp>(reg_var);
                                        w->emit_mov(win_regs[arg_idx], arg_reg);
                                        pool.free(arg_reg);
                                    }
                                }
                                else {
                                    stack_args.emplace_back(reg_var, is_fp);
                                }
                            }
                        }

                        // pushing to stack reverse order
                        for (auto it_stack = stack_args.rbegin(); it_stack != stack_args.rend(); ++it_stack) {
                            auto [reg_var, is_fp] = *it_stack;
                            if (is_fp) {
                                simd128 arg_reg = std::get<simd128>(reg_var);
                                w->emit_sub(rsp, 8);
                                w->emit_movsd(mem{rsp, 0}, arg_reg);
                                simd_pool.free(arg_reg);
                            }
                            else {
                                grp arg_reg = std::get<grp>(reg_var);
                                w->emit_push(arg_reg);
                                pool.free(arg_reg);
                            }
                        }

                        const std::size_t stack_args_bytes = stack_args.size() * 8;
                        const std::size_t current_mod = (totalsizes + caller_saved_bytes + stack_args_bytes) % 16;
                        const std::size_t alignment_padding = (8 + 16 - current_mod) % 16;
                        if (alignment_padding != 0) {
                            w->emit_sub(rsp, static_cast<std::int32_t>(alignment_padding));
                        }

                        auto target_fn = function_map[func_name];
                        w->emit_mov(rax, reinterpret_cast<std::int64_t>(target_fn));
                        w->emit_call(rax);

                        if (alignment_padding != 0) {
                            w->emit_add(rsp, static_cast<std::int32_t>(alignment_padding));
                        }

                        if (!stack_args.empty()) {
                            w->emit_add(rsp, static_cast<std::int32_t>(stack_args_bytes));
                        }

                        // restore xmm
                        for (auto it_simd = caller_saved_simd.rbegin(); it_simd != caller_saved_simd.rend(); ++it_simd) {
                            w->emit_movsd(*it_simd, mem{rsp, 0});
                            w->emit_add(rsp, 16);
                        }

                        // restore gpr
                        if (is_systemv) {
                            for (auto it_gp = caller_saved_gp.rbegin(); it_gp != caller_saved_gp.rend(); ++it_gp) {
                                w->emit_pop(*it_gp);
                            }
                        }

                        // windows shadow space cleanup
                        if (!is_systemv) {
                            w->emit_add(rsp, 32);
                        }

                        auto ret_it = function_return_types.find(func_name);
                        const std::string ret_type = (ret_it != function_return_types.end()) ? ret_it->second : "";
                        bool ret_is_fp = (ret_type == "float32" || ret_type == "float64");

                        // Determine whether this call's return value will be consumed.
                        // We simulate the register pool depth from this point forward:
                        // - ops that push values (push, load, call-with-return) increase depth
                        // - ops that consume values (add, store, etc.) decrease depth
                        // If the depth ever goes below 0, it means this call's return
                        // value was consumed as part of an expression (e.g., f(a) + f(b)).
                        // If we reach a label/jump or the depth never goes negative,
                        // the return value is not consumed (standalone call).
                        bool should_push_return = false;
                        if (!ret_type.empty() && i + 1 < func.code.size()) {
                            auto next_op = func.code.at(i + 1).op;
                            switch (next_op) {
                            // Opcodes that immediately consume the return value
                            case ir_opcode::op_store:
                            case ir_opcode::op_store_at_addr:
                            case ir_opcode::op_array_store_element:
                            case ir_opcode::op_member_store:
                            case ir_opcode::op_add:
                            case ir_opcode::op_sub:
                            case ir_opcode::op_mul:
                            case ir_opcode::op_div:
                            case ir_opcode::op_mod:
                            case ir_opcode::op_imul:
                            case ir_opcode::op_idiv:
                            case ir_opcode::op_imod:
                            case ir_opcode::op_negate:
                            case ir_opcode::op_addf32:
                            case ir_opcode::op_subf32:
                            case ir_opcode::op_mulf32:
                            case ir_opcode::op_divf32:
                            case ir_opcode::op_modf32:
                            case ir_opcode::op_addf64:
                            case ir_opcode::op_subf64:
                            case ir_opcode::op_mulf64:
                            case ir_opcode::op_divf64:
                            case ir_opcode::op_modf64:
                            case ir_opcode::op_cmp:
                            case ir_opcode::op_cmpf32:
                            case ir_opcode::op_cmpf64:
                            case ir_opcode::op_logical_and:
                            case ir_opcode::op_logical_or:
                            case ir_opcode::op_not:
                            case ir_opcode::op_bitwise_and:
                            case ir_opcode::op_bitwise_or:
                            case ir_opcode::op_bitwise_xor:
                            case ir_opcode::op_bitwise_not:
                            case ir_opcode::op_bitwise_lshift:
                            case ir_opcode::op_bitwise_rshift:
                            case ir_opcode::op_ibitwise_rshift:
                            case ir_opcode::op_dereference:
                            case ir_opcode::op_dereference_assign:
                            case ir_opcode::op_reference:
                            case ir_opcode::op_array_access_element:
                            case ir_opcode::op_ret:
                            case ir_opcode::op_call: // nested function call uses return as
                                                     // argument
                            case ir_opcode::op_struct_store:
                            case ir_opcode::op_struct_decl:
                                should_push_return = true;
                                break;
                            // When the next op is a value-producing op (push/load), do a
                            // depth-tracking scan to determine if this call's return value
                            // is eventually consumed as part of an expression like f(a)+f(b).
                            case ir_opcode::op_push:
                            case ir_opcode::op_push_single:
                            case ir_opcode::op_load:
                                {
                                    // depth tracks the number of values added to the pool AFTER
                                    // this call's return value. If a consuming op would make
                                    // depth go negative, it means our return value is needed.
                                    int depth = 0;
                                    for (std::size_t la = i + 1; la < func.code.size(); ++la) {
                                        auto la_op = func.code.at(la).op;
                                        // value-producing ops
                                        if (la_op == ir_opcode::op_push || la_op == ir_opcode::op_push_single || la_op == ir_opcode::op_push_for_ret || la_op == ir_opcode::op_load || la_op == ir_opcode::op_member_access ||
                                            la_op == ir_opcode::op_array_access_element) {
                                            depth++;
                                        }
                                        // call: consumes its arguments and produces one return value.
                                        // Net change = 1 - arg_count. For a 1-arg call: net 0.
                                        // For the pattern f(a)+f(b): push a → depth 1, call f(1 arg)
                                        // → depth 1 (1-1+1=1). Then add consumes 2 → depth -1 < 0,
                                        // meaning our return value is needed.
                                        else if (la_op == ir_opcode::op_call) {
                                            std::string call_name = std::get<std::string>(func.code.at(la).operand);
                                            auto call_it = std::ranges::find_if(ir_funcs, [&](const ir_function& f) { return f.name == call_name; });
                                            if (call_it != ir_funcs.end()) {
                                                int call_arg_count = call_it->args.size();
                                                depth -= call_arg_count; // consumed arguments
                                                if (depth < 0) {
                                                    should_push_return = true;
                                                    break;
                                                }
                                                depth += 1; // produced return value
                                            }
                                            else {
                                                // External/native function — can't determine arg count,
                                                // conservatively stop the scan here.
                                                break;
                                            }
                                        }
                                        // binary ops: consume 2, produce 1 → net -1
                                        else if (la_op == ir_opcode::op_add || la_op == ir_opcode::op_sub || la_op == ir_opcode::op_mul || la_op == ir_opcode::op_div || la_op == ir_opcode::op_mod || la_op == ir_opcode::op_imul ||
                                                 la_op == ir_opcode::op_idiv || la_op == ir_opcode::op_imod || la_op == ir_opcode::op_addf32 || la_op == ir_opcode::op_subf32 || la_op == ir_opcode::op_mulf32 ||
                                                 la_op == ir_opcode::op_divf32 || la_op == ir_opcode::op_modf32 || la_op == ir_opcode::op_addf64 || la_op == ir_opcode::op_subf64 || la_op == ir_opcode::op_mulf64 ||
                                                 la_op == ir_opcode::op_divf64 || la_op == ir_opcode::op_modf64 || la_op == ir_opcode::op_cmp || la_op == ir_opcode::op_cmpf32 || la_op == ir_opcode::op_cmpf64 ||
                                                 la_op == ir_opcode::op_logical_and || la_op == ir_opcode::op_logical_or || la_op == ir_opcode::op_bitwise_and || la_op == ir_opcode::op_bitwise_or || la_op == ir_opcode::op_bitwise_xor ||
                                                 la_op == ir_opcode::op_bitwise_lshift || la_op == ir_opcode::op_bitwise_rshift || la_op == ir_opcode::op_ibitwise_rshift) {
                                            depth -= 1; // consumes 2, produces 1
                                            if (depth < 0) {
                                                should_push_return = true;
                                                break;
                                            }
                                        }
                                        // store/ret consume 1, produce 0
                                        else if (la_op == ir_opcode::op_store || la_op == ir_opcode::op_store_at_addr || la_op == ir_opcode::op_ret || la_op == ir_opcode::op_member_store || la_op == ir_opcode::op_array_store_element ||
                                                 la_op == ir_opcode::op_dereference_assign) {
                                            depth -= 1;
                                            if (depth < 0) {
                                                should_push_return = true;
                                                break;
                                            }
                                            break; // store/ret ends the expression
                                        }
                                        // unary ops: consume 1, produce 1 → net 0
                                        else if (la_op == ir_opcode::op_negate || la_op == ir_opcode::op_not || la_op == ir_opcode::op_bitwise_not || la_op == ir_opcode::op_dereference || la_op == ir_opcode::op_reference) {
                                            // net 0 change
                                        }
                                        // control flow breaks the analysis
                                        else if (la_op == ir_opcode::op_jmp || la_op == ir_opcode::op_jz || la_op == ir_opcode::op_jnz || la_op == ir_opcode::op_jl || la_op == ir_opcode::op_jle || la_op == ir_opcode::op_jg ||
                                                 la_op == ir_opcode::op_jge) {
                                            break; // can't analyze across jumps
                                        }
                                        // no-op markers: skip them
                                        // (label, mark_for_array_access, array_decl, etc.)
                                    }
                                    break;
                                }
                            default:
                                should_push_return = false;
                                break;
                            }
                        }

                        if (should_push_return) {
                            if (ret_is_fp) {
                                auto ret_reg = simd_pool.alloc();
                                w->emit_movsd(ret_reg, xmm0);
                                simd_pool.push(ret_reg);
                            }
                            else {
                                auto ret_reg = pool.alloc();
                                w->emit_mov(ret_reg, rax);
                                pool.push(ret_reg);
                            }
                        }

                        break;
                    }

                /* arrays */
                case ir_opcode::op_array_decl:
                    {
                        auto name = std::get<std::string>(code.operand);
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Got name of array: " << RESET << name << std::endl;
                        }

                        auto type = std::get<std::string>(func.code.at(++i).operand);
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Array type: " << RESET << type << std::endl;
                        }

                        auto dimensions = std::get<std::uint64_t>(func.code.at(++i).operand);
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Dimensions count: " << RESET << dimensions << std::endl;
                        }

                        std::vector<std::uint64_t> dimensions_vec;
                        for (std::size_t j = 0; j < dimensions; j++) {
                            dimensions_vec.emplace_back(std::get<std::uint64_t>(func.code.at(++i).operand));
                        }

                        if (debug) {
                            int j = 0;
                            for (const auto& e : dimensions_vec) {
                                std::cout << BLUE << "[CODEGEN INFO] Size in dimension (" << ++j << "): " << RESET << e << std::endl;
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
                        // don't emit sub rsp here - stack space is pre-allocated via
                        // placeholder patching
                        std::int32_t base_offset = -totalsizes - total_mem;
                        totalsizes += total_mem;

                        local_array_metadata[name] = {type, dimensions_vec, total_mem};
                        local_variable_map[name] = base_offset; // store base address as negative offset from rbp

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Allocated array: " << RESET << name << "\n"
                                      << BLUE << "               totalsizes before: " << RESET << (totalsizes - total_mem) << "\n"
                                      << BLUE << "               total_mem: " << RESET << total_mem << "\n"
                                      << BLUE << "               base_offset: " << RESET << base_offset << "\n"
                                      << BLUE << "               totalsizes after: " << RESET << totalsizes << std::endl;
                        }

                        break;
                    }
                case ir_opcode::op_declare_where_to_store:
                    {
                        auto index = std::get<std::uint64_t>(code.operand);
                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Declaring store index: " << RESET << index << std::endl;
                        }

                        index_to_push = index;

                        break;
                    }
                case ir_opcode::op_array_store_element:
                    {
                        auto array_name = std::get<std::string>(func.code.at(i).operand);
                        auto it = local_variable_map.find(array_name);

                        if (it == local_variable_map.end()) {
                            std::cerr << RED << "[CODEGEN ERROR] Array " << array_name << " not found" << RESET << std::endl;
                            // FIX 10: Clean up any values that might be on the stack
                            if (!pool.empty()) {
                                pool.pop(); // value
                                if (!pool.empty()) {
                                    pool.free(pool.pop()); // potential index
                                }
                            }
                            return;
                        }

                        std::int32_t array_base_offset = it->second; // negative offset from rbp where array starts

                        // scan backwards to determine if we have a constant index from
                        // op_declare_where_to_store if we find op_mark_for_array_access first,
                        // then it's a dynamic index
                        bool has_constant_index = false;
                        for (std::size_t j = 1; j <= 50 && i >= j; j++) { // 50 instr lookback for larger right hand side expr
                            auto prev_op = func.code.at(i - j).op;
                            if (prev_op == ir_opcode::op_declare_where_to_store) {
                                has_constant_index = true;
                                break;
                            }
                            if (prev_op == ir_opcode::op_mark_for_array_access) {
                                has_constant_index = false;
                                break;
                            }
                        }

                        // treat it as a constant index even if op_declare_where_to_store wasn't
                        // emitted for pattern push constant, mark_for_array_access,
                        // array_store_element
                        std::optional<std::uint64_t> inline_const_index;
                        if (!has_constant_index && i >= 2 && func.code.at(i - 1).op == ir_opcode::op_mark_for_array_access && func.code.at(i - 2).op == ir_opcode::op_push) {
                            const auto& opnd = func.code.at(i - 2).operand;
                            if (std::holds_alternative<std::int64_t>(opnd)) {
                                inline_const_index = static_cast<std::uint64_t>(std::get<std::int64_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint64_t>(opnd)) {
                                inline_const_index = std::get<std::uint64_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int32_t>(opnd)) {
                                inline_const_index = static_cast<std::uint64_t>(std::get<std::int32_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint32_t>(opnd)) {
                                inline_const_index = std::get<std::uint32_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int16_t>(opnd)) {
                                inline_const_index = static_cast<std::uint64_t>(std::get<std::int16_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint16_t>(opnd)) {
                                inline_const_index = std::get<std::uint16_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int8_t>(opnd)) {
                                inline_const_index = static_cast<std::uint64_t>(std::get<std::int8_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint8_t>(opnd)) {
                                inline_const_index = std::get<std::uint8_t>(opnd);
                            }
                        }

                        if (has_constant_index) {
                            auto content = pool.pop();

                            std::uint64_t index = index_to_push;
                            std::int32_t element_offset = array_base_offset + 8 * index;

                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Array store (constant) to " << array_name << "[" << index << "]" << RESET << std::endl;
                                std::cout << "\tBase offset: " << array_base_offset << std::endl;
                                std::cout << "\tElement offset: " << element_offset << std::endl;
                            }

                            w->emit_mov(mem{rbp, element_offset}, content);
                            index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                            pool.free(content);
                        }
                        else {
                            if (inline_const_index.has_value()) {
                                auto content = pool.pop();
                                // discard the pushed index register
                                auto idx_reg = pool.pop();
                                pool.free(idx_reg);

                                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * inline_const_index.value());

                                if (debug) {
                                    std::cout << BLUE << "[CODEGEN INFO] Array store (inline constant) to " << array_name << "[" << inline_const_index.value() << "]" << RESET << std::endl;
                                    std::cout << "\tBase offset: " << array_base_offset << std::endl;
                                    std::cout << "\tElement offset: " << element_offset << std::endl;
                                }

                                w->emit_mov(mem{rbp, element_offset}, content);
                                pool.free(content);
                                break;
                            }
                            if (index_to_push != (std::numeric_limits<std::uint64_t>::max)()) {
                                auto content = pool.pop();
                                std::uint64_t index = index_to_push;
                                std::int32_t element_offset = array_base_offset + 8 * index;

                                if (debug) {
                                    std::cout << BLUE << "[CODEGEN INFO] Array store (pending constant) to " << array_name << "[" << index << "]" << RESET << std::endl;
                                    std::cout << "\tBase offset: " << array_base_offset << std::endl;
                                    std::cout << "\tElement offset: " << element_offset << std::endl;
                                }

                                w->emit_mov(mem{rbp, element_offset}, content);
                                index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                                pool.free(content);
                                break;
                            }

                            auto content = pool.pop();
                            auto index_reg = pool.pop();

                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Array store (dynamic) to " << array_name << "[index_reg]" << RESET << std::endl;
                                std::cout << "\tBase offset: " << array_base_offset << std::endl;
                            }

                            // calculate address, lea addr, [rbp + base_offset] (addr += index *
                            // 8)
                            auto addr_reg = pool.alloc();
                            w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                            w->emit_shl(index_reg, 3); // index_reg = index * 8
                            w->emit_add(addr_reg, index_reg);

                            // store, [addr_reg] = content
                            w->emit_mov(mem{addr_reg}, content);

                            pool.free(addr_reg);
                            pool.free(index_reg);
                            pool.free(content);
                        }

                        break;
                    }
                case ir_opcode::op_array_access_element:
                    {
                        auto array_name = std::get<std::string>(code.operand);

                        auto it = local_variable_map.find(array_name);
                        if (it == local_variable_map.end()) {
                            std::cerr << RED << "[CODEGEN ERROR] Array " << array_name << " not found" << RESET << std::endl;
                            return;
                        }

                        auto& local_metadata = local_array_metadata[array_name];

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Array access: " << RESET << array_name << std::endl;
                            std::cout << BLUE << "[CODEGEN INFO] Array access total dims: " << RESET << local_metadata.dimensions.size() << std::endl;
                        }

                        std::int32_t array_base_offset = it->second; // negative offset from rbp where array starts

                        // inline constant index push constant, mark_for_array_access,
                        // array_access_element
                        if (i >= 2 && func.code.at(i - 1).op == ir_opcode::op_mark_for_array_access && func.code.at(i - 2).op == ir_opcode::op_push) {
                            const auto& opnd = func.code.at(i - 2).operand;
                            std::optional<std::uint64_t> inline_idx;
                            if (std::holds_alternative<std::int64_t>(opnd)) {
                                inline_idx = static_cast<std::uint64_t>(std::get<std::int64_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint64_t>(opnd)) {
                                inline_idx = std::get<std::uint64_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int32_t>(opnd)) {
                                inline_idx = static_cast<std::uint64_t>(std::get<std::int32_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint32_t>(opnd)) {
                                inline_idx = std::get<std::uint32_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int16_t>(opnd)) {
                                inline_idx = static_cast<std::uint64_t>(std::get<std::int16_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint16_t>(opnd)) {
                                inline_idx = std::get<std::uint16_t>(opnd);
                            }
                            else if (std::holds_alternative<std::int8_t>(opnd)) {
                                inline_idx = static_cast<std::uint64_t>(std::get<std::int8_t>(opnd));
                            }
                            else if (std::holds_alternative<std::uint8_t>(opnd)) {
                                inline_idx = std::get<std::uint8_t>(opnd);
                            }

                            if (inline_idx.has_value()) {
                                // drop the pushed index register from the pool
                                auto idx_reg = pool.pop();
                                pool.free(idx_reg);

                                auto true_index = pool.alloc();
                                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * inline_idx.value());

                                w->emit_mov(true_index, mem{rbp, element_offset});
                                pool.push(true_index);
                                break;
                            }
                        }

                        // pop all indices from the stack (they're in reverse order, last
                        // dimension first)
                        std::vector<grp> indices;
                        for (std::size_t dims = 0; dims < local_metadata.dimensions.size(); dims++) {
                            indices.push_back(pool.pop());
                        }

                        // reverse to get them in correct order
                        std::reverse(indices.begin(), indices.end());

                        auto true_index = pool.alloc();
                        w->emit_xor(true_index, true_index); // zero out

                        for (std::size_t dim = 0; dim < local_metadata.dimensions.size(); dim++) {
                            std::uint64_t stride = 1;
                            for (std::size_t next_dim = dim + 1; next_dim < local_metadata.dimensions.size(); next_dim++) {
                                stride *= local_metadata.dimensions[next_dim];
                            }

                            if (debug) {
                                std::cout << BLUE << "[CODEGEN INFO] Dimension " << dim << " stride: " << RESET << stride << std::endl;
                            }

                            if (stride == 1) {
                                w->emit_add(true_index, indices[dim]);
                            }
                            else {
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
                                }
                                else {
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

                        // calculate element offset base + true_index * 8
                        // for multi-dimensional arrays, true_index already contains the
                        // linearized index

                        // we need to compute [rbp + base + index*8]
                        // since we can't do this directly with scaled indexed addressing when
                        // base is not 0, we'll compute base + index*8 into a register first
                        auto addr_reg = pool.alloc();
                        w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                        w->emit_shl(true_index, 3); // true_index = index * 8
                        w->emit_add(addr_reg, true_index);

                        w->emit_mov(true_index, mem{addr_reg}); // load from computed address
                        pool.free(addr_reg);
                        pool.push(true_index);

                        break;
                    }
                /* structures */
                case ir_opcode::op_struct_decl:
                    {
                        pending_struct_type = std::get<std::string>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Struct declaration of type: " << RESET << pending_struct_type << std::endl;
                        }

                        break;
                    }
                case ir_opcode::op_struct_store:
                    {
                        auto var_name = std::get<std::string>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Struct store variable: " << RESET << var_name << " of type: " << pending_struct_type << std::endl;
                        }

                        // check if this is a pointer to struct type
                        bool is_struct_ptr = false;
                        std::string base_struct_type = pending_struct_type;
                        const std::string ptr_suffix = kStructPtrSuffix;
                        if (pending_struct_type.ends_with(ptr_suffix)) {
                            is_struct_ptr = true;
                            base_struct_type = pending_struct_type.substr(0, pending_struct_type.size() - ptr_suffix.size());
                        }

                        auto it = local_variable_map.find(var_name);
                        if (it == local_variable_map.end()) {
                            if (is_struct_ptr) {
                                // if pointer to struct type, just allocate 8 bytes like a regular var
                                totalsizes += 8;

                                if (!pool.empty()) {
                                    grp r = pool.pop();
                                    w->emit_mov(mem{rbp, -totalsizes}, r);
                                    pool.free(r);
                                }

                                local_variable_map.insert({var_name, totalsizes});
                                local_variable_map_types.insert({var_name, pending_struct_type});
                            }
                            else {
                                // allocate space
                                auto layout_it = struct_layouts.find(pending_struct_type);
                                if (layout_it == struct_layouts.end()) {
                                    // ukn, just allocate 8 bytes like a regular var
                                    totalsizes += 8;

                                    if (!pool.empty()) {
                                        grp r = pool.pop();
                                        w->emit_mov(mem{rbp, -totalsizes}, r);
                                        pool.free(r);
                                    }

                                    local_variable_map.insert({var_name, totalsizes});
                                    local_variable_map_types.insert({var_name, pending_struct_type});
                                }
                                else {
                                    // full struct, allocate space for all members
                                    std::int32_t struct_size = layout_it->second.total_size;
                                    // Align to 16 bytes
                                    struct_size = ((struct_size + 15) / 16) * 16;
                                    totalsizes += struct_size;

                                    // if there's a value on the register pool from
                                    // an assignment or whatever it's a pointer to a source
                                    // struct. then copy data
                                    if (!pool.empty()) {
                                        grp src = pool.pop();
                                        grp tmp = pool.alloc();

                                        // copy entire struct 8 bytes at a time
                                        std::int32_t actual_size = layout_it->second.total_size;
                                        for (std::int32_t off = 0; off < actual_size; off += 8) {
                                            w->emit_mov(tmp, mem{src, off});
                                            w->emit_mov(mem{rbp, -totalsizes + off}, tmp);
                                        }

                                        pool.free(tmp);
                                        pool.free(src);
                                    }

                                    local_variable_map.insert({var_name, totalsizes});
                                    local_variable_map_types.insert({var_name, pending_struct_type});
                                    local_struct_var_types.insert({var_name, pending_struct_type});
                                }
                            }
                        }
                        else {
                            // bariable already exists, just update its value
                            if (!pool.empty()) {
                                grp r = pool.pop();
                                std::int32_t offset = -it->second;
                                w->emit_mov(mem{rbp, offset}, r);
                                pool.free(r);
                            }
                        }

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Struct variable " << RESET << var_name << " at offset -" << local_variable_map[var_name] << std::endl;
                        }

                        break;
                    }

                case ir_opcode::op_member_access:
                    {
                        auto member_name = std::get<std::string>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Member access: " << RESET << member_name << std::endl;
                        }

                        // the base address of the struct should be on the register pool
                        grp base = pool.pop();

                        // we need to figure out which struct type this variable is ^^^
                        // look through all struct layouts to find which one has this member.
                        std::int32_t member_offset = -1;
                        std::string member_struct_type;
                        std::string member_type_name;
                        for (const auto& [sname, slayout] : struct_layouts) {
                            auto off = slayout.get_member_offset(member_name);
                            if (off >= 0) {
                                member_offset = off;
                                member_struct_type = sname;
                                // Find the member's type
                                for (const auto& m : slayout.members) {
                                    if (m.name == member_name) {
                                        member_type_name = m.type;
                                        break;
                                    }
                                }
                                break;
                            }
                        }

                        if (member_offset < 0) {
                            std::cerr << RED << "[CODEGEN ERROR] Member " << member_name << " not found in any struct" << RESET << std::endl;
                            pool.push(base);
                            break;
                        }

                        // determine if this member is itself a struct type (not a pointer)
                        // if it is, we emit lea to compute its address
                        // if it's a scalar or pointer, we emit mov to load its value
                        bool member_is_nested_struct = struct_layouts.contains(member_type_name);

                        if (member_is_nested_struct) {
                            // nested struct by value, compute address via LEA
                            w->emit_lea(base, mem{base, member_offset});
                        }
                        else {
                            // scalar or pointer, load the actual value
                            w->emit_mov(base, mem{base, member_offset});
                        }

                        pool.push(base);

                        break;
                    }

                case ir_opcode::op_member_store:
                    {
                        auto member_name = std::get<std::string>(code.operand);

                        if (debug) {
                            std::cout << BLUE << "[CODEGEN INFO] Member store: " << RESET << member_name << std::endl;
                        }

                        // value on top, then base address
                        grp val = pool.pop();
                        grp base = pool.pop();

                        // find the member offset
                        std::int32_t member_offset = -1;
                        for (const auto& [sname, slayout] : struct_layouts) {
                            auto off = slayout.get_member_offset(member_name);
                            if (off >= 0) {
                                member_offset = off;
                                break;
                            }
                        }

                        if (member_offset < 0) {
                            std::cerr << RED << "[CODEGEN ERROR] Member " << member_name << " not found in any struct for store" << RESET << std::endl;
                            pool.free(val);
                            pool.free(base);
                            break;
                        }

                        w->emit_mov(mem{base, member_offset}, val);

                        pool.free(val);
                        pool.free(base);

                        break;
                    }
                default:
                    {
                        break;
                    }
                }
            }

            if (debug) {
                std::cout << BLUE << "[CODEGEN INFO] Total jump instructions to backpatch: " << RESET << jump_instructions.size() << std::endl;
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
                        std::cout << "\tOffset will be: " << (label_map[label_name] - (snd + ((jump_type == ir_opcode::op_jmp) ? 5 : 6))) << RESET << std::endl;
                    }
                    else {
                        std::cout << RED << "\tERROR: Label not found!" << RESET << std::endl;
                    }
                }

                if (label_map.contains(label_name)) {
                    backpatch_jump(jump_type, snd, label_map[label_name], w);
                }
                else {
                    std::cout << RED << "[CODEGEN ERROR] Label \"" << label_name << "\" not found." << RESET << std::endl;
                    return;
                }
            }

            // patch the stack allocation placeholder with actual totalsizes
            if (!func.uses_shellcode && stack_alloc_placeholder_location > 0) {
                auto& code = w->get_code();
                // the sub rsp, imm32 instruction format is: 48 81 EC [imm32]
                // we need to patch the imm32 value (4 bytes starting at offset +3)
                std::int32_t alloc_size = totalsizes;
                for (int i = 0; i < 4; ++i) {
                    code[stack_alloc_placeholder_location + 3 + i] = static_cast<std::uint8_t>((alloc_size >> (i * 8)) & 0xFF);
                }

                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Patched stack allocation: sub rsp, " << totalsizes << RESET << std::endl;
                }
            }

            // patch the initial stack allocation (sub rsp, imm32) to the final
            // totalsizes value
            if (!func.uses_shellcode && stack_alloc_placeholder_location > 0) {
                // ensure stack is 16-byte aligned for function calls per System V ABI
                // after call + push rbp, rsp is 16-byte aligned, so keep the frame size a
                // multiple of 16
                if (totalsizes % 16 != 0) {
                    std::int32_t alignment_padding = 16 - (totalsizes % 16);
                    totalsizes += alignment_padding;
                    if (debug && alignment_padding > 0) {
                        std::cout << BLUE << "[CODEGEN INFO] Added " << alignment_padding << " bytes stack alignment padding (totalsizes was " << (totalsizes - alignment_padding) << ")" << RESET << std::endl;
                    }
                }

                auto& code = w->get_code();
                std::int32_t alloc_size = totalsizes;
                for (int i = 0; i < 4; ++i) {
                    code[stack_alloc_placeholder_location + 3 + i] = static_cast<std::uint8_t>((alloc_size >> (i * 8)) & 0xFF);
                }
                if (debug) {
                    std::cout << BLUE << "[CODEGEN INFO] Patched stack alloc to " << alloc_size << " bytes" << RESET << std::endl;
                }
            }
        }

        void compile_function(ir_function& func, const bool use_jit) {
            static std::unordered_set<std::string> compiling_functions;

            if (!func.type.empty()) {
                function_return_types[func.name] = func.type;
            }

            if (func.is_external && function_map.contains(func.name)) {
                return;
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
                std::cout << BLUE << "[CODEGEN INFO] Compiling function: " << YELLOW << "\"" << func.name << "\"\n" << RESET;
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
                std::cout << GREEN << "[CODEGEN SUCCESS] Generated code for " << YELLOW << "\"" << func.name << "\"\n" << RESET;
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
        std::unordered_map<std::uint64_t, std::string> string_literals;

        explicit codegen(const std::vector<ir_function>& ir_funcs, const std::vector<ir_struct>& ir_structs, const bool debug = false) : ir_funcs(ir_funcs), ir_structs(ir_structs), debug(debug) {
            // create all layouts with preliminary sizes first
            for (const auto& s : ir_structs) {
                struct_layout layout;
                layout.name = s.datatype;
                layout.total_size = 0;
                for (const auto& m : s.members) {
                    layout.members.push_back({m.name, m.datatype, 0});
                }
                struct_layouts[s.datatype] = layout;
            }

            // resolve sizes (nested structs get their actual size) 2nd
            constexpr int kMaxStructLayoutPasses = 10;
            bool changed = true;
            for (int pass = 0; pass < kMaxStructLayoutPasses && changed; ++pass) {
                changed = false;
                for (auto& [name, layout] : struct_layouts) {
                    std::int32_t offset = 0;
                    for (auto& member : layout.members) {
                        member.offset = offset;
                        auto nested_it = struct_layouts.find(member.type);
                        if (nested_it != struct_layouts.end() && nested_it->second.total_size > 0) {
                            offset += nested_it->second.total_size;
                        }
                        else {
                            offset += 8; // scalar types are 8-byte aligned
                        }
                    }
                    if (offset != layout.total_size) {
                        layout.total_size = offset;
                        changed = true;
                    }
                }
            }

            // ensure minimum size
            for (auto& [name, layout] : struct_layouts) {
                if (layout.total_size == 0) {
                    layout.total_size = 8;
                }
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
