#pragma once
#include "x86_64_writer.hpp"
#include "ir_gen.hpp"
#include "x86_64_assembler.hpp"

#include <cstring>
#include <optional>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

// a renewed stack codegen, more organized and stable

namespace occult::x86_64 {

    struct array_metadata {
        std::string type;
        std::vector<std::uint64_t> dimensions;
        std::int32_t total_size{};
    };

    struct struct_member_info {
        std::string name;
        std::string type;
        std::int32_t offset;
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

    static const std::unordered_map<std::string, std::string> cast_keyword_to_typename = {
        {"i8", "int8"}, {"i16", "int16"}, {"i32", "int32"}, {"i64", "int64"},
        {"u8", "uint8"}, {"u16", "uint16"}, {"u32", "uint32"}, {"u64", "uint64"},
        {"f32", "float32"}, {"f64", "float64"},
    };

    enum class call_abi { sysv, win64 };

    constexpr call_abi host_abi() {
#ifdef _WIN64
        return call_abi::win64;
#else
        return call_abi::sysv;
#endif
    }

    class codegen_v2 {    
        bool debug; 
        call_abi abi = host_abi();

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

            std::vector<grp> allocation_order_grp = {
                rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
            };

            std::vector<simd128> allocation_order_simd128 = {
                xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15
            };

            struct stack_entry {
                bool is_spilled = false;
                std::variant<grp, simd128> reg{};
                std::int32_t offset = 0;
            };

            std::vector<stack_entry> register_stack;

            x86_64_writer* writer = nullptr;
            std::int32_t* stack_size_tracker = nullptr;
        public:
            register_allocator() = default;

            void configure(x86_64_writer* writer_, std::int32_t* stack_size_tracker_, call_abi abi_) {
                writer = writer_;
                stack_size_tracker = stack_size_tracker_;
                if (abi_ == call_abi::win64) {
                    allocation_order_grp = {rax, rcx, rdx, r8, r9, r10, r11};
                    allocation_order_simd128 = {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5};
                }
            }

            template<typename RegT>
            bool check(RegT reg) {
                auto it = register_liveliness.find(reg);

                return it != register_liveliness.end() && it->second;
            }

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

            void release(const std::variant<grp, simd128>& reg) {
                std::visit([this](auto&& r) { register_liveliness[r] = false; }, reg);
            }

            void force_evict(grp reg) {
                if (!check(reg)) {
                    return;
                }

                for (auto& entry : register_stack) {
                    if (entry.is_spilled || !std::holds_alternative<grp>(entry.reg) || std::get<grp>(entry.reg) != reg) {
                        continue;
                    }

                    if (!writer || !stack_size_tracker) {
                        throw std::runtime_error("register_allocator: force_evict() requires configure() to have been called");
                    }

                    *stack_size_tracker += 8;
                    std::int32_t off = -*stack_size_tracker;
                    writer->emit_mov(mem{rbp, off}, reg);
                    entry.is_spilled = true;
                    entry.offset = off;
                    release(reg);
                    return;
                }

                release(reg);
            }

            template<typename RegT>
            RegT allocate(std::initializer_list<grp> excluded = {}) {
                auto is_excluded = [&](const RegT& r) {
                    if constexpr (std::is_same_v<RegT, grp>) {
                        for (grp e : excluded) {
                            if (e == r) return true;
                        }
                    }
                    return false;
                };

                if constexpr (std::is_same_v<RegT, grp>) {
                    for (auto& reg : allocation_order_grp) {
                        if (is_excluded(reg)) continue;
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

                if (writer && stack_size_tracker) {
                    for (auto& entry : register_stack) {
                        if (entry.is_spilled || !std::holds_alternative<RegT>(entry.reg)) {
                            continue;
                        }

                        RegT r = std::get<RegT>(entry.reg);
                        if (is_excluded(r)) {
                            continue;
                        }

                        *stack_size_tracker += 8;
                        std::int32_t off = -*stack_size_tracker;

                        if constexpr (std::is_same_v<RegT, grp>) {
                            writer->emit_mov(mem{rbp, off}, r);
                        }
                        else {
                            writer->emit_movsd(mem{rbp, off}, r);
                        }

                        entry.is_spilled = true;
                        entry.offset = off;
                        release(r);

                        if (auto taken = take(r); taken.has_value()) {
                            return taken.value();
                        }
                    }
                }

                throw std::runtime_error("Invalid type for register allocator (no free registers, nothing left to spill)");
            }

            template<typename RegT>
            void push(RegT reg) { register_stack.push_back(stack_entry{false, reg, 0}); }

            template<typename RegT>
            RegT pop() {
                if (register_stack.empty()) {
                    throw std::runtime_error("register_allocator::pop() on empty stack");
                }

                auto entry = register_stack.back();
                register_stack.pop_back();

                if (entry.is_spilled) {
                    RegT r = allocate<RegT>();
                    if constexpr (std::is_same_v<RegT, grp>) {
                        writer->emit_mov(r, mem{rbp, entry.offset});
                    }
                    else {
                        writer->emit_movsd(r, mem{rbp, entry.offset});
                    }
                    return r;
                }

                return std::get<RegT>(entry.reg);
            }

            std::variant<grp, simd128> pop_variant() {
                if (register_stack.empty()) {
                    throw std::runtime_error("register_allocator::pop_variant() on empty stack");
                }

                auto entry = register_stack.back();
                register_stack.pop_back();

                if (entry.is_spilled) {
                    if (std::holds_alternative<grp>(entry.reg)) {
                        grp r = allocate<grp>();
                        writer->emit_mov(r, mem{rbp, entry.offset});
                        return r;
                    }
                    else {
                        simd128 r = allocate<simd128>();
                        writer->emit_movsd(r, mem{rbp, entry.offset});
                        return r;
                    }
                }

                return entry.reg;
            }

            template<typename RegT>
            RegT top() {
                if (register_stack.empty()) {
                    throw std::runtime_error("register_allocator::top() on empty stack");
                }

                auto& entry = register_stack.back();

                if (entry.is_spilled) {
                    RegT r = allocate<RegT>();
                    if constexpr (std::is_same_v<RegT, grp>) {
                        writer->emit_mov(r, mem{rbp, entry.offset});
                    }
                    else {
                        writer->emit_movsd(r, mem{rbp, entry.offset});
                    }
                    entry.is_spilled = false;
                    entry.reg = r;
                }

                return std::get<RegT>(entry.reg);
            }

            std::vector<std::variant<grp, simd128>> snapshot() const {
                std::vector<std::variant<grp, simd128>> result;
                for (auto& entry : register_stack) {
                    if (!entry.is_spilled) {
                        result.push_back(entry.reg);
                    }
                }
                return result;
            }

            bool empty() const { return register_stack.empty(); }

            void spill_all() {
                for (auto& entry : register_stack) {
                    if (entry.is_spilled) {
                        continue;
                    }
                    *stack_size_tracker += 8;
                    std::int32_t off = -*stack_size_tracker;
                    if (std::holds_alternative<grp>(entry.reg)) {
                        writer->emit_mov(mem{rbp, off}, std::get<grp>(entry.reg));
                    }
                    else {
                        writer->emit_movsd(mem{rbp, off}, std::get<simd128>(entry.reg));
                    }
                    entry.is_spilled = true;
                    entry.offset = off;
                    release(entry.reg);
                }
            }

            struct entry_view {
                bool is_spilled;
                std::variant<grp, simd128> reg;
                std::int32_t offset;
            };

            std::vector<entry_view> state() const {
                std::vector<entry_view> s;
                s.reserve(register_stack.size());
                for (auto& e : register_stack) {
                    s.push_back({e.is_spilled, e.reg, e.offset});
                }
                return s;
            }

            void restore(const std::vector<entry_view>& s) {
                for (auto& [reg, live] : register_liveliness) {
                    live = false;
                }
                register_stack.clear();
                for (auto& e : s) {
                    register_stack.push_back(stack_entry{e.is_spilled, e.reg, e.offset});
                    if (!e.is_spilled) {
                        std::visit([this](auto&& r) { register_liveliness[r] = true; }, e.reg);
                    }
                }
            }

            entry_view pop_spilled() {
                if (register_stack.empty()) {
                    throw std::runtime_error("register_allocator::pop_spilled() on empty stack");
                }
                auto e = register_stack.back();
                register_stack.pop_back();
                if (!e.is_spilled) {
                    throw std::runtime_error("register_allocator::pop_spilled() on a register-resident entry");
                }
                return {e.is_spilled, e.reg, e.offset};
            }

            static bool same_location(const entry_view& a, const entry_view& b) {
                if (a.is_spilled != b.is_spilled) return false;
                if (a.is_spilled) return a.offset == b.offset;
                return a.reg == b.reg;
            }

            bool top_is_simd() const {
                return !register_stack.empty() && std::holds_alternative<simd128>(register_stack.back().reg);
            }
        };

        std::unique_ptr<x86_64_writer> w; 
        std::vector<ir_function> stack_ir; 
        std::vector<ir_struct> ir_structs; 
        public:
        std::unordered_map<std::string, std::uint32_t> function_locs;
        private:
        std::unordered_map<std::string, std::string> function_return_types;
        std::unordered_map<std::string, std::vector<std::string>> function_arg_types;
        std::unordered_map<std::string, std::vector<std::string>> function_arg_names;
        std::unordered_set<std::string> variable_arity_functions;
        std::unordered_map<std::string, struct_layout> struct_layouts;
        std::vector<std::unique_ptr<std::uint8_t[]>> literal_pool;
        std::vector<std::pair<std::string, std::size_t>> call_patch_list;
        std::vector<std::pair<std::string, std::size_t>> func_addr_patch_list; // function, offset of lea disp32

        // lea dst, [rip + disp32] to a function in this blob; disp backpatched
        // once every function has a location (position independent)
        void emit_function_addr(grp dst, const std::string& fname) {
            const auto v = static_cast<std::uint8_t>(dst);
            const std::uint8_t idx = (v >= static_cast<std::uint8_t>(r8) && v <= static_cast<std::uint8_t>(r15)) ? static_cast<std::uint8_t>(8 + (v - static_cast<std::uint8_t>(r8))) : static_cast<std::uint8_t>(v & 7);
            w->push_bytes({static_cast<std::uint8_t>(0x48 | (idx >= 8 ? 0x04 : 0)), 0x8D, static_cast<std::uint8_t>(((idx & 7) << 3) | 0x05), 0, 0, 0, 0});
            func_addr_patch_list.emplace_back(fname, w->get_code().size() - 4);
        }

        // globals, one shared, writable slot each (8 bytes), addressed by
        // absolute address baked in with movabs same scheme as string
        // literals, since the JIT code blob itself is mapped execute only.
        std::unordered_map<std::string, std::int32_t> global_offsets;
        std::unordered_map<std::string, std::string> global_types;   
        std::unique_ptr<std::uint8_t[]> globals_storage;

        // loads the absolute address of global name into dst and records
        // a relocation so the AOT linker can repoint it into the data segment
        void emit_global_addr(register_allocator& reg_alloc, grp dst, const std::string& name) {
            (void)reg_alloc;
            std::int32_t off = global_offsets.at(name);
            std::int64_t abs = reinterpret_cast<std::int64_t>(globals_storage.get() + off);
            w->emit_mov(dst, abs);
            global_relocs.emplace_back(w->get_code().size() - 8, off);
        }

        public:
        std::unordered_map<std::uint64_t, std::string> string_literals;
        std::vector<std::pair<std::size_t, std::uint64_t>> string_relocs;
        std::vector<std::pair<std::size_t, std::int32_t>> global_relocs; // (imm64 code offset, global offset)
        std::int32_t globals_size() const { return static_cast<std::int32_t>(global_offsets.size()) * 8; }
        private:

        template<typename OperandT>
        static std::optional<std::uint64_t> extract_uint_operand(const OperandT& opnd) {
            if (std::holds_alternative<std::int64_t>(opnd))  return static_cast<std::uint64_t>(std::get<std::int64_t>(opnd));
            if (std::holds_alternative<std::uint64_t>(opnd)) return std::get<std::uint64_t>(opnd);
            if (std::holds_alternative<std::int32_t>(opnd))  return static_cast<std::uint64_t>(std::get<std::int32_t>(opnd));
            if (std::holds_alternative<std::uint32_t>(opnd)) return std::get<std::uint32_t>(opnd);
            if (std::holds_alternative<std::int16_t>(opnd))  return static_cast<std::uint64_t>(std::get<std::int16_t>(opnd));
            if (std::holds_alternative<std::uint16_t>(opnd)) return std::get<std::uint16_t>(opnd);
            if (std::holds_alternative<std::int8_t>(opnd))   return static_cast<std::uint64_t>(std::get<std::int8_t>(opnd));
            if (std::holds_alternative<std::uint8_t>(opnd))  return std::get<std::uint8_t>(opnd);
            return std::nullopt;
        }

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
                const std::string& str = std::get<std::string>(instr.operand);
                constexpr std::size_t header_size = 8;
                const std::size_t storage_size = header_size + str.size() + 8;

                auto buf = std::make_unique<std::uint8_t[]>(storage_size);
                std::memset(buf.get(), 0, storage_size);
                *reinterpret_cast<std::uint64_t*>(buf.get()) = str.size();
                std::memcpy(buf.get() + header_size, str.data(), str.size());

                auto literal_ptr = buf.get() + header_size;
                std::uint64_t host_addr = reinterpret_cast<std::uint64_t>(literal_ptr);

                string_literals[host_addr] = str;
                literal_pool.push_back(std::move(buf));

                auto reg = reg_alloc.allocate<grp>();
                w->emit_mov(reg, reinterpret_cast<std::int64_t>(literal_ptr));
                string_relocs.emplace_back(w->get_code().size() - 8, host_addr);
                reg_alloc.push<grp>(reg);
            }
            else if constexpr (std::is_integral_v<T>) {
                T val = std::get<T>(instr.operand);
                auto reg = reg_alloc.allocate<grp>();

                if constexpr (sizeof(T) < 4 && std::is_signed_v<T>) {
                    w->emit_mov(reg, static_cast<std::int64_t>(val));
                }
                else if constexpr (sizeof(T) < 4) {
                    w->emit_mov(reg, static_cast<std::uint64_t>(val));
                }
                else {
                    w->emit_mov(reg, val);
                }

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

        void handle_store(register_allocator& reg_alloc, ir_instr& instr,
                           std::unordered_map<std::string, std::int64_t>& local_variable_map,
                           std::unordered_map<std::string, std::string>& local_variable_map_types,
                           std::unordered_map<std::string, std::string>& local_struct_var_types,
                           std::int32_t& totalsizes) {
            auto var_name = std::get<std::string>(instr.operand);
            auto var_type = instr.type;
            auto it = local_variable_map.find(var_name);

            if (it == local_variable_map.end() && global_offsets.contains(var_name)) {
                const std::string& gtype = global_types[var_name];
                const bool fstore = (gtype == "float32" || gtype == "float64") || reg_alloc.top_is_simd();
                if (fstore) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    grp gbase = reg_alloc.allocate<grp>();
                    emit_global_addr(reg_alloc, gbase, var_name);
                    if (gtype == "float32") w->emit_movss(mem{gbase}, simd_reg);
                    else                    w->emit_movsd(mem{gbase}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                    reg_alloc.release<grp>(gbase);
                }
                else {
                    grp val = reg_alloc.pop<grp>();
                    grp gbase = reg_alloc.allocate<grp>();
                    emit_global_addr(reg_alloc, gbase, var_name);
                    w->emit_mov(mem{gbase}, val); // 8-byte slot; typed load narrows on read
                    reg_alloc.release<grp>(val);
                    reg_alloc.release<grp>(gbase);
                }
                return;
            }

            bool is_float_store = (var_type == "float32" || var_type == "float64");

            if (!is_float_store && it != local_variable_map.end()) {
                auto type_it = local_variable_map_types.find(var_name);
                if (type_it != local_variable_map_types.end() && (type_it->second == "float32" || type_it->second == "float64")) {
                    is_float_store = true;
                    var_type = type_it->second;
                }
            }

            if (!is_float_store && reg_alloc.top_is_simd()) {
                is_float_store = true;
                var_type = "float64";
            }

            if (is_float_store) {
                auto simd_reg = reg_alloc.pop<simd128>();
                std::int32_t offset;

                if (it == local_variable_map.end()) {
                    totalsizes += 8;
                    offset = -totalsizes;
                    local_variable_map.insert({var_name, totalsizes});
                    local_variable_map_types.insert({var_name, var_type});
                }
                else {
                    offset = -it->second;
                }

                if (var_type == "float32") w->emit_movss(mem{rbp, offset}, simd_reg);
                else                       w->emit_movsd(mem{rbp, offset}, simd_reg);

                reg_alloc.release<simd128>(simd_reg);
                return;
            }

            grp r = reg_alloc.pop<grp>();
            std::int32_t offset;

            if (it == local_variable_map.end()) {
                totalsizes += 8;
                offset = -totalsizes;
                local_variable_map.insert({var_name, totalsizes});
                local_variable_map_types.insert({var_name, var_type});
            }
            else {
                offset = -it->second;

                auto struct_it = local_struct_var_types.find(var_name);
                if (struct_it != local_struct_var_types.end()) {
                    auto layout_it = struct_layouts.find(struct_it->second);
                    if (layout_it != struct_layouts.end()) {
                        grp tmp = reg_alloc.allocate<grp>();
                        std::int32_t actual_size = layout_it->second.total_size;
                        for (std::int32_t off = 0; off < actual_size; off += 8) {
                            w->emit_mov(tmp, mem{r, off});
                            w->emit_mov(mem{rbp, offset + off}, tmp);
                        }
                        reg_alloc.release<grp>(tmp);
                        reg_alloc.release<grp>(r);
                        return;
                    }
                }
            }

            if (var_type == "int8" || var_type == "uint8" || var_type == "bool") {
                w->emit_mov(mem{bpl, offset}, as_8(r));
            }
            else if (var_type == "int16" || var_type == "uint16") {
                w->emit_mov(mem{bp, offset}, as_16(r));
            }
            else if (var_type == "int32" || var_type == "uint32") {
                w->emit_mov(mem{ebp, offset}, as_32(r));
            }
            else {
                w->emit_mov(mem{rbp, offset}, r);
            }

            reg_alloc.release<grp>(r);
        }

        void handle_load(register_allocator& reg_alloc, ir_instr& instr,
                          std::unordered_map<std::string, std::int64_t>& local_variable_map,
                          std::unordered_map<std::string, std::string>& local_variable_map_types,
                          std::unordered_map<std::string, std::string>& local_struct_var_types,
                          bool& is_reference_next,
                          bool& is_dereference_next, std::size_t& deref_count_normal,
                          bool& is_dereference_assign_next, std::size_t& deref_count_assign) {
            const auto& var_name = std::get<std::string>(instr.operand);
            auto it = local_variable_map.find(var_name);

            if (it == local_variable_map.end()) {
                if (global_offsets.contains(var_name)) {
                    const std::string& gtype = global_types[var_name];
                    grp gbase = reg_alloc.allocate<grp>();
                    emit_global_addr(reg_alloc, gbase, var_name);

                    if (is_reference_next) {
                        is_reference_next = false;
                        reg_alloc.push<grp>(gbase); // the global's address itself
                        return;
                    }
                    if (is_dereference_next) {
                        w->emit_mov(gbase, mem{gbase}); // the global's value (a pointer)
                        for (std::size_t s = 0; s < deref_count_normal; s++) w->emit_mov(gbase, mem{gbase});
                        is_dereference_next = false;
                        deref_count_normal = 0;
                        reg_alloc.push<grp>(gbase);
                        return;
                    }
                    if (is_dereference_assign_next) {
                        w->emit_mov(gbase, mem{gbase});
                        for (std::size_t s = 1; s < deref_count_assign; s++) w->emit_mov(gbase, mem{gbase});
                        deref_count_assign = 0;
                        is_dereference_assign_next = false;
                        reg_alloc.push<grp>(gbase);
                        return;
                    }
                    if (gtype == "float32" || gtype == "float64") {
                        auto simd_reg = reg_alloc.allocate<simd128>();
                        if (gtype == "float64") w->emit_movsd(simd_reg, mem{gbase});
                        else                    w->emit_movss(simd_reg, mem{gbase});
                        reg_alloc.release<grp>(gbase);
                        reg_alloc.push<simd128>(simd_reg);
                        return;
                    }
                    if (gtype == "int8" || gtype == "bool") w->emit_movsx(gbase, mem{gbase});
                    else if (gtype == "uint8")              w->emit_movzx(gbase, mem{gbase});
                    else if (gtype == "int16")              w->emit_movsx(gbase, mem{gbase}, false);
                    else if (gtype == "uint16")             w->emit_movzx(gbase, mem{gbase}, false);
                    else if (gtype == "int32")              w->emit_movsxd(gbase, mem{gbase});
                    else if (gtype == "uint32")             w->emit_mov(as_32(gbase), mem{gbase});
                    else                                    w->emit_mov(gbase, mem{gbase}); // int64/uint64/string/ptr
                    reg_alloc.push<grp>(gbase);
                    return;
                }
                if (function_arg_types.contains(var_name)) {
                    if (!is_reference_next) {
                        throw std::runtime_error("Use @" + var_name + " to take the address of function " + var_name);
                    }
                    is_reference_next = false;
                    grp r = reg_alloc.allocate<grp>();
                    emit_function_addr(r, var_name);
                    reg_alloc.push<grp>(r);
                    return;
                }
                throw std::runtime_error("Attempted to load undeclared variable: " + var_name);
            }

            const auto& var_type = local_variable_map_types[var_name];
            std::int32_t offset = -it->second;

            if (is_reference_next) {
                auto r = reg_alloc.allocate<grp>();

                const bool is_reference_typed_var = var_type.ends_with("_reference") && !struct_layouts.contains(var_type);
                if (is_reference_typed_var) {
                    w->emit_mov(r, mem{rbp, offset});
                }
                else {
                    w->emit_lea(r, mem{rbp, offset});
                }

                is_reference_next = false;
                reg_alloc.push<grp>(r);
                return;
            }

            if (is_dereference_next) {
                auto r = reg_alloc.allocate<grp>();

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
                reg_alloc.push<grp>(r);
                return;
            }

            if (is_dereference_assign_next) {
                auto r = reg_alloc.allocate<grp>();

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
                reg_alloc.push<grp>(r);
                return;
            }

            if (var_type == "float32" || var_type == "float64") {
                auto simd_reg = reg_alloc.allocate<simd128>();
                if (var_type == "float64") w->emit_movsd(simd_reg, mem{rbp, offset});
                else                       w->emit_movss(simd_reg, mem{rbp, offset});
                reg_alloc.push<simd128>(simd_reg);
                return;
            }

            auto r = reg_alloc.allocate<grp>();
            bool loaded = false;

            if (var_type == "int8" || var_type == "bool")      { w->emit_movsx(r, mem{rbp, offset}); loaded = true; }
            else if (var_type == "uint8")                       { w->emit_movzx(r, mem{rbp, offset}); loaded = true; }
            else if (var_type == "int16")                       { w->emit_movsx(r, mem{rbp, offset}, false); loaded = true; }
            else if (var_type == "uint16")                      { w->emit_movzx(r, mem{rbp, offset}, false); loaded = true; }
            else if (var_type == "int32")                       { w->emit_movsxd(r, mem{rbp, offset}); loaded = true; }
            else if (var_type == "uint32")                      { w->emit_mov(as_32(r), mem{rbp, offset}); loaded = true; }
            else if (var_type == "int64" || var_type == "uint64" || var_type == "string") { w->emit_mov(r, mem{rbp, offset}); loaded = true; }

            if (!loaded && local_struct_var_types.contains(var_name)) {
                w->emit_lea(r, mem{rbp, offset});
                loaded = true;
            }

            if (!loaded) {
                w->emit_mov(r, mem{rbp, offset});
            }

            reg_alloc.push<grp>(r);
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
                    auto pop_or_zero = [&]() -> grp {
                        if (!reg_alloc.empty()) {
                            return reg_alloc.pop<grp>();
                        }
                        grp z = reg_alloc.allocate<grp>();
                        w->emit_xor(z, z);
                        return z;
                    };

                    auto rhs = pop_or_zero();
                    auto lhs = pop_or_zero();

                    w->emit_cmp(lhs, rhs);

                    reg_alloc.release<grp>(rhs);
                    reg_alloc.release<grp>(lhs);

                    break;
                }
                default:
                    break;
            }
        }

        void handle_setcc(register_allocator& reg_alloc, ir_instr& instr) {
            auto target = reg_alloc.allocate<grp>();

            switch (instr.op) {
                case ir_opcode::op_setz:  w->emit_setz(target); break;
                case ir_opcode::op_setnz: w->emit_setnz(target); break;
                case ir_opcode::op_setl:  w->emit_setl(target); break;
                case ir_opcode::op_setle: w->emit_setle(target); break;
                case ir_opcode::op_setg:  w->emit_setnle(target); break;
                case ir_opcode::op_setge: w->emit_setnl(target); break;
                default: break;
            }

            w->emit_movzx(target, target, true);

            reg_alloc.push<grp>(target);
        }

        void handle_not(register_allocator& reg_alloc, ir_instr& instr) {
            (void)instr;
            auto r = reg_alloc.pop<grp>();
            w->emit_cmp(r, 0);
            w->emit_setz(r);
            w->emit_movzx(r, r, true);
            reg_alloc.push<grp>(r);
        }

        void handle_cast(register_allocator& reg_alloc, ir_instr& instr) {
            if (instr.op == ir_opcode::op_bitcast) {
                const auto& target_type = std::get<std::string>(instr.operand);
                const auto& source_type = instr.type;

                bool target_is_float = (target_type == "float32" || target_type == "float64");
                bool source_is_float = (source_type == "float32" || source_type == "float64");

                if (source_is_float && !target_is_float) {
                    auto src = reg_alloc.pop<simd128>();
                    auto dst = reg_alloc.allocate<grp>();
                    w->emit_movq(dst, src);
                    reg_alloc.release<simd128>(src);
                    reg_alloc.push<grp>(dst);
                }
                else if (!source_is_float && target_is_float) {
                    auto src = reg_alloc.pop<grp>();
                    auto dst = reg_alloc.allocate<simd128>();
                    w->emit_movq(dst, src);
                    reg_alloc.release<grp>(src);
                    reg_alloc.push<simd128>(dst);
                }
                return;
            }

            const auto& raw_target = std::get<std::string>(instr.operand);
            auto norm_it = cast_keyword_to_typename.find(raw_target);
            const std::string& target_type = (norm_it != cast_keyword_to_typename.end()) ? norm_it->second : raw_target;

            const bool target_is_float32 = (target_type == "float32");
            const bool target_is_float64 = (target_type == "float64");
            const bool target_is_float = target_is_float32 || target_is_float64;

            auto src_norm_it = cast_keyword_to_typename.find(instr.type);
            const std::string& src_type = (src_norm_it != cast_keyword_to_typename.end()) ? src_norm_it->second : instr.type;
            const bool source_is_f64 = (src_type == "float64");
            const bool source_is_f32 = (src_type == "float32");
            const bool source_is_float = src_type.empty() ? reg_alloc.top_is_simd() : (source_is_f32 || source_is_f64);

            if (source_is_float && target_is_float) {
                auto src = reg_alloc.pop<simd128>();
                auto dst = reg_alloc.allocate<simd128>();
                if (target_is_float64) w->emit_cvtss2sd(dst, src);
                else                   w->emit_cvtsd2ss(dst, src);
                reg_alloc.release<simd128>(src);
                reg_alloc.push<simd128>(dst);
            }
            else if (source_is_float && !target_is_float) {
                auto src = reg_alloc.pop<simd128>();
                auto dst = reg_alloc.allocate<grp>();
                if (source_is_f64) w->emit_cvttsd2si(dst, src);
                else                w->emit_cvttss2si(dst, src);
                reg_alloc.release<simd128>(src);
                reg_alloc.push<grp>(dst);
            }
            else if (!source_is_float && target_is_float) {
                auto src = reg_alloc.pop<grp>();
                auto dst = reg_alloc.allocate<simd128>();
                if (target_is_float32) w->emit_cvtsi2ss(dst, src);
                else                    w->emit_cvtsi2sd(dst, src);
                reg_alloc.release<grp>(src);
                reg_alloc.push<simd128>(dst);
            }
            else {
                auto r = reg_alloc.pop<grp>();
                if (target_type == "int8")        w->emit_movsx(r, r, true);
                else if (target_type == "uint8")  w->emit_movzx(r, r, true);
                else if (target_type == "int16")  w->emit_movsx(r, r, false);
                else if (target_type == "uint16") w->emit_movzx(r, r, false);
                else if (target_type == "int32")  w->emit_movsxd(r, r);
                else if (target_type == "uint32") w->emit_mov(as_32(r), as_32(r));
                reg_alloc.push<grp>(r);
            }
        }

        void handle_arith(register_allocator& reg_alloc, ir_instr& instr) {
            switch (instr.op) {
                case ir_opcode::op_add: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();
                    w->emit_add(lhs, rhs);
                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(lhs);
                    break;
                }
                case ir_opcode::op_sub: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();
                    w->emit_sub(lhs, rhs);
                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(lhs);
                    break;
                }
                case ir_opcode::op_bitwise_and: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();
                    w->emit_and(lhs, rhs);
                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(lhs);
                    break;
                }
                case ir_opcode::op_bitwise_or: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();
                    w->emit_or(lhs, rhs);
                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(lhs);
                    break;
                }
                case ir_opcode::op_bitwise_xor: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();
                    w->emit_xor(lhs, rhs);
                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(lhs);
                    break;
                }
                case ir_opcode::op_negate: {
                    auto r = reg_alloc.pop<grp>();
                    w->emit_neg(r);
                    reg_alloc.push<grp>(r);
                    break;
                }
                case ir_opcode::op_bitwise_not: {
                    auto r = reg_alloc.pop<grp>();
                    w->emit_not(r);
                    reg_alloc.push<grp>(r);
                    break;
                }

                case ir_opcode::op_bitwise_lshift:
                case ir_opcode::op_bitwise_rshift:
                case ir_opcode::op_ibitwise_rshift: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();

                    if (lhs == rcx) {
                        grp tmp = reg_alloc.allocate<grp>({rcx});
                        w->emit_mov(tmp, lhs);
                        reg_alloc.release<grp>(lhs);
                        lhs = tmp;
                    }
                    if (rhs != rcx) {
                        reg_alloc.force_evict(rcx);
                        reg_alloc.take<grp>(rcx);
                        w->emit_mov(rcx, rhs);
                        reg_alloc.release<grp>(rhs);
                    }

                    if (instr.op == ir_opcode::op_bitwise_lshift)      w->emit_shl(lhs);
                    else if (instr.op == ir_opcode::op_bitwise_rshift) w->emit_shr(lhs);
                    else                                                w->emit_sar(lhs);

                    reg_alloc.release<grp>(rcx);
                    reg_alloc.push<grp>(lhs);

                    break;
                }

                case ir_opcode::op_mul:
                case ir_opcode::op_imul: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();

                    if (rhs == rax || rhs == rdx) {
                        grp tmp = reg_alloc.allocate<grp>({rax, rdx});
                        w->emit_mov(tmp, rhs);
                        reg_alloc.release<grp>(rhs);
                        rhs = tmp;
                    }

                    if (lhs != rax) {
                        reg_alloc.force_evict(rax);
                        reg_alloc.take<grp>(rax);
                        w->emit_mov(rax, lhs);
                        reg_alloc.release<grp>(lhs);
                    }

                    reg_alloc.force_evict(rdx);

                    if (instr.op == ir_opcode::op_mul) w->emit_mul(rhs);
                    else                                w->emit_imul(rhs);

                    reg_alloc.release<grp>(rhs);
                    reg_alloc.push<grp>(rax);

                    break;
                }

                case ir_opcode::op_mod:
                case ir_opcode::op_div:
                case ir_opcode::op_imod:
                case ir_opcode::op_idiv: {
                    auto rhs = reg_alloc.pop<grp>();
                    auto lhs = reg_alloc.pop<grp>();

                    if (rhs == rax || rhs == rdx) {
                        grp tmp = reg_alloc.allocate<grp>({rax, rdx});
                        w->emit_mov(tmp, rhs);
                        reg_alloc.release<grp>(rhs);
                        rhs = tmp;
                    }

                    if (lhs != rax) {
                        reg_alloc.force_evict(rax);
                        reg_alloc.take<grp>(rax);
                        w->emit_mov(rax, lhs);
                        reg_alloc.release<grp>(lhs);
                    }

                    reg_alloc.force_evict(rdx);
                    if (!reg_alloc.check(rdx)) {
                        reg_alloc.take<grp>(rdx);
                    }

                    w->emit_cqo();
                    w->emit_idiv(rhs);

                    bool want_quotient = (instr.op == ir_opcode::op_div || instr.op == ir_opcode::op_idiv);
                    grp result = want_quotient ? rax : rdx;
                    grp discard = want_quotient ? rdx : rax;

                    reg_alloc.release<grp>(rhs);
                    reg_alloc.release<grp>(discard);
                    reg_alloc.push<grp>(result);

                    break;
                }

                default:
                    break;
            }
        }

        void handle_arith_float(register_allocator& reg_alloc, ir_instr& instr) {
            switch (instr.op) {
                case ir_opcode::op_addf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_addss(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_addf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_addsd(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_subf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_subss(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_subf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_subsd(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_mulf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_mulss(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_mulf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_mulsd(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_divf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_divss(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_divf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    w->emit_divsd(lhs, rhs);
                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.push<simd128>(lhs);
                    break;
                }
                case ir_opcode::op_modf32: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    auto tmp1 = reg_alloc.allocate<simd128>();
                    auto tmp2 = reg_alloc.allocate<simd128>();
                    auto tmp_gpr = reg_alloc.allocate<grp>();

                    w->emit_fmod_float(lhs, lhs, rhs, tmp1, tmp2, tmp_gpr);

                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.release<simd128>(tmp1);
                    reg_alloc.release<simd128>(tmp2);
                    reg_alloc.release<grp>(tmp_gpr);
                    reg_alloc.push<simd128>(lhs);

                    break;
                }
                case ir_opcode::op_modf64: {
                    auto rhs = reg_alloc.pop<simd128>();
                    auto lhs = reg_alloc.pop<simd128>();
                    auto tmp1 = reg_alloc.allocate<simd128>();
                    auto tmp2 = reg_alloc.allocate<simd128>();
                    auto tmp_gpr = reg_alloc.allocate<grp>();

                    w->emit_fmod_double(lhs, lhs, rhs, tmp1, tmp2, tmp_gpr);

                    reg_alloc.release<simd128>(rhs);
                    reg_alloc.release<simd128>(tmp1);
                    reg_alloc.release<simd128>(tmp2);
                    reg_alloc.release<grp>(tmp_gpr);
                    reg_alloc.push<simd128>(lhs);

                    break;
                }
                case ir_opcode::op_negatef32: {
                    auto val = reg_alloc.pop<simd128>();
                    auto zero = reg_alloc.allocate<simd128>();
                    w->emit_xorps(zero, zero);
                    w->emit_subss(zero, val);
                    reg_alloc.release<simd128>(val);
                    reg_alloc.push<simd128>(zero);
                    break;
                }
                case ir_opcode::op_negatef64: {
                    auto val = reg_alloc.pop<simd128>();
                    auto zero = reg_alloc.allocate<simd128>();
                    w->emit_xorps(zero, zero);
                    w->emit_subsd(zero, val);
                    reg_alloc.release<simd128>(val);
                    reg_alloc.push<simd128>(zero);
                    break;
                }
                default:
                    break;
            }
        }

        enum push_mode {
            single,
            ret,
            normal
        };

        void handle_return(ir_function& func, register_allocator& reg_alloc, push_mode push_mode) {
            bool is_float_return = (func.type == "float32" || func.type == "float64");

            if (push_mode != ret) {
                if (is_float_return) {
                    if (!reg_alloc.empty()) {
                        auto result = reg_alloc.pop<simd128>();

                        if (func.type == "float64") {
                            w->emit_movsd(xmm0, result);
                        }
                        else if (func.type == "float32") {
                            w->emit_movss(xmm0, result);
                        }

                        reg_alloc.release<simd128>(result);
                    }
                }
                else {
                    if (!reg_alloc.empty()) {
                        auto result = reg_alloc.pop<grp>();

                        w->emit_mov(rax, result);

                        const auto& t = func.type;
                        if (t == "int8")                        w->emit_movsx(rax, rax, true);
                        else if (t == "uint8" || t == "bool")   w->emit_movzx(rax, rax, true);
                        else if (t == "int16")                  w->emit_movsx(rax, rax, false);
                        else if (t == "uint16")                 w->emit_movzx(rax, rax, false);
                        else if (t == "int32")                  w->emit_movsxd(rax, rax);
                        else if (t == "uint32")                 w->emit_mov(as_32(rax), as_32(rax));

                        reg_alloc.release<grp>(result);
                    }
                }
            }

            if (!func.uses_shellcode && !func.uses_assembly) {
                w->emit_function_epilogue();
                w->emit_ret();
            }
            else {
                w->emit_ret();
            }
        }

        struct arg_location {
            bool in_register;
            std::variant<grp, simd128> reg;
            std::size_t stack_index;
        };

        std::int32_t shadow_space() const { return abi == call_abi::win64 ? 32 : 0; }

        std::vector<arg_location> classify_args(const std::vector<bool>& is_float) const {
            std::vector<arg_location> out;
            out.reserve(is_float.size());
            std::size_t stack_index = 0;

            if (abi == call_abi::win64) {
                constexpr grp gp[] = {rcx, rdx, r8, r9};
                constexpr simd128 fp[] = {xmm0, xmm1, xmm2, xmm3};
                for (std::size_t k = 0; k < is_float.size(); k++) {
                    if (k < 4) out.push_back({true, is_float[k] ? std::variant<grp, simd128>(fp[k]) : std::variant<grp, simd128>(gp[k]), 0});
                    else       out.push_back({false, {}, stack_index++});
                }
                return out;
            }

            constexpr grp gp[] = {rdi, rsi, rdx, rcx, r8, r9};
            constexpr simd128 fp[] = {xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7};
            std::size_t gp_idx = 0, fp_idx = 0;
            for (bool f : is_float) {
                if (f && fp_idx < 8)       out.push_back({true, fp[fp_idx++], 0});
                else if (!f && gp_idx < 6) out.push_back({true, gp[gp_idx++], 0});
                else                       out.push_back({false, {}, stack_index++});
            }
            return out;
        }

        void handle_prologue_args(ir_function& func,
                                   std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                   std::unordered_map<std::string, std::string>& local_variable_map_types,
                                   std::unordered_map<std::string, array_metadata>& local_array_metadata,
                                   std::int32_t& totalsizes) {
            if (func.uses_shellcode || func.uses_assembly || func.args.empty()) {
                return;
            }

            std::vector<bool> is_float;
            for (auto& arg : func.args) {
                is_float.push_back(arg.type == "float32" || arg.type == "float64");
            }
            const auto locs = classify_args(is_float);

            for (std::size_t k = 0; k < func.args.size(); k++) {
                auto& arg = func.args[k];

                totalsizes += 8;
                std::int32_t offset = -totalsizes;
                const auto& loc = locs[k];

                if (loc.in_register) {
                    if (is_float[k]) w->emit_movsd(mem{rbp, offset}, std::get<simd128>(loc.reg));
                    else             w->emit_mov(mem{rbp, offset}, std::get<grp>(loc.reg));
                }
                else {
                    const auto src = static_cast<std::int32_t>(16 + shadow_space() + 8 * loc.stack_index);
                    if (is_float[k]) {
                        w->emit_movsd(xmm0, mem{rbp, src});
                        w->emit_movsd(mem{rbp, offset}, xmm0);
                    }
                    else {
                        w->emit_mov(rax, mem{rbp, src});
                        w->emit_mov(mem{rbp, offset}, rax);
                    }
                }

                local_variable_map.insert({arg.name, totalsizes});
                local_variable_map_types.insert({arg.name, arg.type});
            }

            if (func.is_variadic) {
                std::vector<std::string> va_arg_names;
                for (const auto& arg : func.args) {
                    if (arg.name.size() >= 4 && arg.name.substr(0, 4) == "__va") {
                        va_arg_names.push_back(arg.name);
                    }
                }

                if (!va_arg_names.empty()) {
                    std::size_t va_count = va_arg_names.size();
                    std::int32_t total_mem = static_cast<std::int32_t>(va_count * 8);
                    total_mem = ((total_mem + 15) / 16) * 16;

                    std::int32_t base_offset = -totalsizes - total_mem;
                    totalsizes += total_mem;

                    for (std::size_t j = 0; j < va_count; j++) {
                        auto va_offset = local_variable_map[va_arg_names[j]];
                        std::int32_t element_offset = base_offset + static_cast<std::int32_t>(j * 8);
                        w->emit_mov(rax, mem{rbp, -static_cast<std::int32_t>(va_offset)});
                        w->emit_mov(mem{rbp, element_offset}, rax);
                    }

                    local_variable_map["__varargs"] = base_offset;
                    local_variable_map_types["__varargs"] = "int64";
                    local_array_metadata["__varargs"] = {"int64", {va_count}, total_mem};
                }
            }
        }

        std::size_t get_call_arg_count(const std::string& name, ir_instr& instr) {
            auto it = function_arg_types.find(name);
            if (it != function_arg_types.end()) {
                if (variable_arity_functions.contains(name) && !instr.type.empty()) {
                    return static_cast<std::size_t>(std::stoi(instr.type));
                }
                return it->second.size();
            }
            if (!instr.type.empty()) {
                return static_cast<std::size_t>(std::stoi(instr.type));
            }

            throw std::runtime_error("Call to \"" + name + "\" has unknown arity");
        }

        // "fn(int64,fn(int64)int64)float64" -> params {int64, fn(int64)int64}, ret float64
        static void split_fnptr_sig(const std::string& sig, std::vector<std::string>& params, std::string& ret) {
            std::size_t i = 3; // past "fn("
            int depth = 0;
            std::string cur;
            for (; i < sig.size(); i++) {
                const char c = sig[i];
                if (c == '(') depth++;
                if (c == ')') {
                    if (depth == 0) break;
                    depth--;
                }
                if (c == ',' && depth == 0) {
                    params.push_back(cur);
                    cur.clear();
                    continue;
                }
                cur += c;
            }
            if (!cur.empty()) params.push_back(cur);
            ret = (i + 1 < sig.size()) ? sig.substr(i + 1) : "int64";
        }

        void handle_call(register_allocator& reg_alloc, ir_instr& instr) {
            const bool indirect = (instr.op == ir_opcode::op_call_indirect);
            std::string name;
            std::vector<std::string> ind_params;
            std::string ind_ret;
            std::size_t arg_count;

            if (indirect) {
                split_fnptr_sig(std::get<std::string>(instr.operand), ind_params, ind_ret);
                arg_count = ind_params.size();
            }
            else {
                name = std::get<std::string>(instr.operand);
                arg_count = get_call_arg_count(name, instr);
            }

            reg_alloc.spill_all();

            std::vector<register_allocator::entry_view> args(arg_count);
            for (std::size_t k = 0; k < arg_count; k++) {
                args[arg_count - 1 - k] = reg_alloc.pop_spilled();
            }

            // indirect: the callee address was pushed before the args
            register_allocator::entry_view callee{};
            if (indirect) {
                callee = reg_alloc.pop_spilled();
            }

            const std::vector<std::string>* param_types = nullptr;
            if (indirect) {
                param_types = &ind_params;
            }
            else if (auto it = function_arg_types.find(name); it != function_arg_types.end()) {
                param_types = &it->second;
            }

            std::vector<bool> is_float(arg_count);
            for (std::size_t k = 0; k < arg_count; k++) {
                auto& a = args[k];
                bool arg_is_float = std::holds_alternative<simd128>(a.reg);

                if (param_types && k < param_types->size()) {
                    const auto& type = (*param_types)[k];
                    const bool param_is_float = (type == "float32" || type == "float64");
                    const bool is_va_slot = !indirect && function_arg_names[name][k].starts_with("__va");

                    if (arg_is_float && !param_is_float) {
                        if (!is_va_slot) {
                            w->emit_movsd(xmm0, mem{rbp, a.offset});
                            w->emit_cvttsd2si(rax, xmm0);
                            w->emit_mov(mem{rbp, a.offset}, rax);
                        }
                        arg_is_float = false;
                    }
                    else if (!arg_is_float && param_is_float && !is_va_slot) {
                        w->emit_mov(rax, mem{rbp, a.offset});
                        if (type == "float32") w->emit_cvtsi2ss(xmm0, rax);
                        else                   w->emit_cvtsi2sd(xmm0, rax);
                        w->emit_movsd(mem{rbp, a.offset}, xmm0);
                        arg_is_float = true;
                    }
                }

                is_float[k] = arg_is_float;
            }

            const auto locs = classify_args(is_float);

            std::vector<std::size_t> stack_args;
            for (std::size_t k = 0; k < arg_count; k++) {
                if (!locs[k].in_register) stack_args.push_back(k);
            }

            const bool padded = (stack_args.size() % 2) != 0;
            if (padded) {
                w->emit_sub(rsp, 8);
            }

            for (auto it = stack_args.rbegin(); it != stack_args.rend(); ++it) {
                w->emit_push(mem{rbp, args[*it].offset});
            }

            for (std::size_t k = 0; k < arg_count; k++) {
                if (!locs[k].in_register) continue;
                if (std::holds_alternative<simd128>(locs[k].reg)) w->emit_movsd(std::get<simd128>(locs[k].reg), mem{rbp, args[k].offset});
                else                                              w->emit_mov(std::get<grp>(locs[k].reg), mem{rbp, args[k].offset});
            }

            if (shadow_space()) {
                w->emit_sub(rsp, shadow_space());
            }

            if (indirect) {
                // r11 is scratch and never an argument register in either ABI
                w->emit_mov(r11, mem{rbp, callee.offset});
                w->push_bytes({0x41, 0xFF, 0xD3}); // call r11
            }
            else {
                w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});
                call_patch_list.emplace_back(name, w->get_code().size() - 5);
            }

            const std::int32_t cleanup = static_cast<std::int32_t>(stack_args.size() * 8) + shadow_space() + (padded ? 8 : 0);
            if (cleanup) {
                w->emit_add(rsp, cleanup);
            }

            const std::string* ret_ptr = nullptr;
            if (indirect) {
                ret_ptr = &ind_ret;
            }
            else if (auto ret_it = function_return_types.find(name); ret_it != function_return_types.end()) {
                ret_ptr = &ret_it->second;
            }
            if (ret_ptr && instr.result_used) {
                const std::string& ret_type = *ret_ptr;
                if (ret_type == "float32" || ret_type == "float64") {
                    auto r = reg_alloc.allocate<simd128>();
                    w->emit_movsd(r, xmm0);
                    reg_alloc.push<simd128>(r);
                }
                else {
                    auto r = reg_alloc.allocate<grp>();
                    w->emit_mov(r, rax);
                    reg_alloc.push<grp>(r);
                }
            }
        }

        void backpatch_calls() {
            auto& code = w->get_code();

            for (auto& [name, disp_loc] : func_addr_patch_list) {
                auto it = function_locs.find(name);
                if (it == function_locs.end()) {
                    throw std::runtime_error("Address taken of function \"" + name + "\" which was never compiled");
                }
                const std::int32_t disp = static_cast<std::int32_t>(it->second) - static_cast<std::int32_t>(disp_loc + 4);
                for (int i = 0; i < 4; ++i) {
                    code[disp_loc + i] = static_cast<std::uint8_t>((disp >> (i * 8)) & 0xFF);
                }
            }

            for (auto& [name, loc] : call_patch_list) {
                auto it = function_locs.find(name);
                if (it == function_locs.end()) {
                    throw std::runtime_error("Call target \"" + name + "\" was never compiled");
                }

                std::int32_t offset = static_cast<std::int32_t>(it->second) - static_cast<std::int32_t>(loc + 5);

                code[loc] = opcode::CALL_rel32;
                for (int i = 0; i < 4; ++i) {
                    code[loc + 1 + i] = static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
                }
            }
        }

        static void backpatch_jump(const ir_opcode op, const std::size_t location, std::size_t label_location, x86_64_writer* w) {
            const std::int32_t offset = static_cast<std::int32_t>(label_location) - static_cast<std::int32_t>(location + ((op == ir_opcode::op_jmp) ? 5 : 6));
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
            case ir_opcode::op_jb:
                code[location] = k2ByteOpcodePrefix;
                code[location + 1] = opcode_2b::JB_rel32;
                break;
            case ir_opcode::op_jbe:
                code[location] = k2ByteOpcodePrefix;
                code[location + 1] = opcode_2b::JBE_rel32;
                break;
            case ir_opcode::op_ja:
                code[location] = k2ByteOpcodePrefix;
                code[location + 1] = opcode_2b::JNBE_rel32;
                break;
            case ir_opcode::op_jae:
                code[location] = k2ByteOpcodePrefix;
                code[location + 1] = opcode_2b::JNB_rel32;
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

        void handle_array_decl(ir_function& func, std::size_t& i, ir_instr& instr,
                                std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                std::unordered_map<std::string, array_metadata>& local_array_metadata,
                                std::int32_t& totalsizes) {
            auto name = std::get<std::string>(instr.operand);
            auto type = std::get<std::string>(func.code.at(++i).operand);
            auto dimensions = std::get<std::uint64_t>(func.code.at(++i).operand);

            std::vector<std::uint64_t> dimensions_vec;
            for (std::size_t j = 0; j < dimensions; j++) {
                dimensions_vec.emplace_back(std::get<std::uint64_t>(func.code.at(++i).operand));
            }

            std::size_t element_size = 8;
            std::size_t element_count = 1;
            for (const auto& dim : dimensions_vec) {
                element_count *= dim;
            }

            std::int32_t total_mem = static_cast<std::int32_t>(element_count * element_size);
            total_mem = ((total_mem + 15) / 16) * 16;

            std::int32_t base_offset = -totalsizes - total_mem;
            totalsizes += total_mem;

            local_array_metadata[name] = {type, dimensions_vec, total_mem};
            local_variable_map[name] = base_offset;
        }

        void handle_array_store_element(register_allocator& reg_alloc, ir_function& func, std::size_t i,
                                         std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                         std::unordered_map<std::string, array_metadata>& local_array_metadata,
                                         std::uint64_t& index_to_push) {
            auto array_name = std::get<std::string>(func.code.at(i).operand);
            auto it = local_variable_map.find(array_name);

            if (it == local_variable_map.end()) {
                throw std::runtime_error("Array \"" + array_name + "\" not found");
            }

            std::int32_t array_base_offset = it->second;
            auto& store_meta = local_array_metadata[array_name];
            bool is_float_array = (store_meta.type == "float32" || store_meta.type == "float64");

            bool has_constant_index = false;
            for (std::size_t j = 1; j <= 50 && i >= j; j++) {
                auto prev_op = func.code.at(i - j).op;
                if (prev_op == ir_opcode::op_declare_where_to_store) { has_constant_index = true; break; }
                if (prev_op == ir_opcode::op_mark_for_array_access)  { has_constant_index = false; break; }
            }

            std::optional<std::uint64_t> inline_const_index;
            if (!has_constant_index && i >= 2 && func.code.at(i - 1).op == ir_opcode::op_mark_for_array_access && func.code.at(i - 2).op == ir_opcode::op_push) {
                inline_const_index = extract_uint_operand(func.code.at(i - 2).operand);
            }

            if (has_constant_index) {
                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * index_to_push);

                if (is_float_array) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    if (store_meta.type == "float32") w->emit_cvtss2sd(simd_reg, simd_reg);
                    w->emit_movsd(mem{rbp, element_offset}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                }
                else {
                    auto content = reg_alloc.pop<grp>();
                    w->emit_mov(mem{rbp, element_offset}, content);
                    reg_alloc.release<grp>(content);
                }
                index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                return;
            }

            if (inline_const_index.has_value()) {
                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * inline_const_index.value());

                if (is_float_array) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);
                    if (store_meta.type == "float32") w->emit_cvtss2sd(simd_reg, simd_reg);
                    w->emit_movsd(mem{rbp, element_offset}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                }
                else {
                    auto content = reg_alloc.pop<grp>();
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);
                    w->emit_mov(mem{rbp, element_offset}, content);
                    reg_alloc.release<grp>(content);
                }
                return;
            }

            if (index_to_push != (std::numeric_limits<std::uint64_t>::max)()) {
                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * index_to_push);

                if (is_float_array) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    if (store_meta.type == "float32") w->emit_cvtss2sd(simd_reg, simd_reg);
                    w->emit_movsd(mem{rbp, element_offset}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                }
                else {
                    auto content = reg_alloc.pop<grp>();
                    w->emit_mov(mem{rbp, element_offset}, content);
                    reg_alloc.release<grp>(content);
                }
                index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                return;
            }

            if (is_float_array) {
                auto simd_reg = reg_alloc.pop<simd128>();
                auto index_reg = reg_alloc.pop<grp>();

                auto addr_reg = reg_alloc.allocate<grp>();
                w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                w->emit_shl(index_reg, 3);
                w->emit_add(addr_reg, index_reg);

                if (store_meta.type == "float32") w->emit_cvtss2sd(simd_reg, simd_reg);
                w->emit_movsd(mem{addr_reg}, simd_reg);

                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(index_reg);
                reg_alloc.release<simd128>(simd_reg);
            }
            else {
                auto content = reg_alloc.pop<grp>();
                auto index_reg = reg_alloc.pop<grp>();

                auto addr_reg = reg_alloc.allocate<grp>();
                w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                w->emit_shl(index_reg, 3);
                w->emit_add(addr_reg, index_reg);

                w->emit_mov(mem{addr_reg}, content);

                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(index_reg);
                reg_alloc.release<grp>(content);
            }
        }

        void handle_array_access_element(register_allocator& reg_alloc, ir_function& func, std::size_t i, ir_instr& instr,
                                          std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                          std::unordered_map<std::string, array_metadata>& local_array_metadata) {
            auto array_name = std::get<std::string>(instr.operand);
            auto it = local_variable_map.find(array_name);
            if (it == local_variable_map.end()) {
                throw std::runtime_error("Array \"" + array_name + "\" not found");
            }

            auto& local_metadata = local_array_metadata[array_name];
            bool is_float_array = (local_metadata.type == "float32" || local_metadata.type == "float64");
            std::int32_t array_base_offset = it->second;

            if (i >= 2 && func.code.at(i - 1).op == ir_opcode::op_mark_for_array_access && func.code.at(i - 2).op == ir_opcode::op_push) {
                auto inline_idx = extract_uint_operand(func.code.at(i - 2).operand);

                if (inline_idx.has_value()) {
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);

                    std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(8 * inline_idx.value());

                    if (is_float_array) {
                        auto simd_reg = reg_alloc.allocate<simd128>();
                        w->emit_movsd(simd_reg, mem{rbp, element_offset});
                        if (local_metadata.type == "float32") w->emit_cvtsd2ss(simd_reg, simd_reg);
                        reg_alloc.push<simd128>(simd_reg);
                    }
                    else {
                        auto r = reg_alloc.allocate<grp>();
                        w->emit_mov(r, mem{rbp, element_offset});
                        reg_alloc.push<grp>(r);
                    }
                    return;
                }
            }

            std::vector<grp> indices;
            for (std::size_t dims = 0; dims < local_metadata.dimensions.size(); dims++) {
                indices.push_back(reg_alloc.pop<grp>());
            }
            std::reverse(indices.begin(), indices.end());

            for (auto& idx : indices) {
                if (idx == rax || idx == rdx) {
                    grp tmp = reg_alloc.allocate<grp>({rax, rdx});
                    w->emit_mov(tmp, idx);
                    reg_alloc.release<grp>(idx);
                    idx = tmp;
                }
            }

            auto true_index = reg_alloc.allocate<grp>({rax, rdx});
            w->emit_xor(true_index, true_index);

            for (std::size_t dim = 0; dim < local_metadata.dimensions.size(); dim++) {
                std::uint64_t stride = 1;
                for (std::size_t next_dim = dim + 1; next_dim < local_metadata.dimensions.size(); next_dim++) {
                    stride *= local_metadata.dimensions[next_dim];
                }

                if (stride == 1) {
                    w->emit_add(true_index, indices[dim]);
                }
                else {
                    grp idx_src = indices[dim];

                    if (idx_src == rax || idx_src == rdx) {
                        grp tmp = reg_alloc.allocate<grp>({rax, rdx});
                        w->emit_mov(tmp, idx_src);
                        reg_alloc.release<grp>(idx_src);
                        idx_src = tmp;
                        indices[dim] = tmp;
                    }

                    bool true_index_is_rax = (true_index == rax);
                    if (!true_index_is_rax) {
                        reg_alloc.force_evict(rax);
                        reg_alloc.take<grp>(rax);
                    }
                    reg_alloc.force_evict(rdx);

                    w->emit_mov(rax, idx_src);

                    if (stride <= 0x7FFFFFFF) {
                        w->emit_imul(rax, rax, static_cast<std::int32_t>(stride));
                    }
                    else {
                        auto temp = reg_alloc.allocate<grp>();
                        w->emit_mov(temp, stride);
                        w->emit_mul(temp);
                        reg_alloc.release<grp>(temp);
                    }

                    w->emit_add(true_index, rax);

                    if (!true_index_is_rax) {
                        reg_alloc.release<grp>(rax);
                    }
                }

                reg_alloc.release<grp>(indices[dim]);
            }

            auto addr_reg = reg_alloc.allocate<grp>();
            w->emit_lea(addr_reg, mem{rbp, array_base_offset});
            w->emit_shl(true_index, 3);
            w->emit_add(addr_reg, true_index);

            if (is_float_array) {
                auto simd_reg = reg_alloc.allocate<simd128>();
                w->emit_movsd(simd_reg, mem{addr_reg});
                if (local_metadata.type == "float32") w->emit_cvtsd2ss(simd_reg, simd_reg);
                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(true_index);
                reg_alloc.push<simd128>(simd_reg);
            }
            else {
                w->emit_mov(true_index, mem{addr_reg});
                reg_alloc.release<grp>(addr_reg);
                reg_alloc.push<grp>(true_index);
            }
        }

        void handle_struct_store(register_allocator& reg_alloc, ir_instr& instr,
                                  std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                  std::unordered_map<std::string, std::string>& local_variable_map_types,
                                  std::unordered_map<std::string, std::string>& local_struct_var_types,
                                  std::string& pending_struct_type,
                                  std::int32_t& totalsizes) {
            auto var_name = std::get<std::string>(instr.operand);

            bool is_struct_ptr = false;
            const std::string ptr_suffix = kStructPtrSuffix;
            if (pending_struct_type.ends_with(ptr_suffix)) {
                is_struct_ptr = true;
            }

            auto it = local_variable_map.find(var_name);
            if (it == local_variable_map.end()) {
                if (is_struct_ptr) {
                    totalsizes += 8;
                    if (!reg_alloc.empty()) {
                        auto r = reg_alloc.pop<grp>();
                        w->emit_mov(mem{rbp, -totalsizes}, r);
                        reg_alloc.release<grp>(r);
                    }
                    local_variable_map.insert({var_name, totalsizes});
                    local_variable_map_types.insert({var_name, pending_struct_type});
                }
                else {
                    auto layout_it = struct_layouts.find(pending_struct_type);
                    if (layout_it == struct_layouts.end()) {
                        totalsizes += 8;
                        if (!reg_alloc.empty()) {
                            auto r = reg_alloc.pop<grp>();
                            w->emit_mov(mem{rbp, -totalsizes}, r);
                            reg_alloc.release<grp>(r);
                        }
                        local_variable_map.insert({var_name, totalsizes});
                        local_variable_map_types.insert({var_name, pending_struct_type});
                    }
                    else {
                        std::int32_t struct_size = layout_it->second.total_size;
                        struct_size = ((struct_size + 15) / 16) * 16;
                        totalsizes += struct_size;

                        if (!reg_alloc.empty()) {
                            auto src = reg_alloc.pop<grp>();
                            auto tmp = reg_alloc.allocate<grp>();
                            std::int32_t actual_size = layout_it->second.total_size;
                            for (std::int32_t off = 0; off < actual_size; off += 8) {
                                w->emit_mov(tmp, mem{src, off});
                                w->emit_mov(mem{rbp, -totalsizes + off}, tmp);
                            }
                            reg_alloc.release<grp>(tmp);
                            reg_alloc.release<grp>(src);
                        }

                        local_variable_map.insert({var_name, totalsizes});
                        local_variable_map_types.insert({var_name, pending_struct_type});
                        local_struct_var_types.insert({var_name, pending_struct_type});
                    }
                }
            }
            else {
                if (!reg_alloc.empty()) {
                    auto r = reg_alloc.pop<grp>();
                    std::int32_t offset = -it->second;
                    w->emit_mov(mem{rbp, offset}, r);
                    reg_alloc.release<grp>(r);
                }
            }
        }

        void handle_member_access(register_allocator& reg_alloc, ir_instr& instr) {
            auto member_name = std::get<std::string>(instr.operand);
            auto base = reg_alloc.pop<grp>();

            std::int32_t member_offset = -1;
            std::string member_type_name;
            for (const auto& [sname, slayout] : struct_layouts) {
                auto off = slayout.get_member_offset(member_name);
                if (off >= 0) {
                    member_offset = off;
                    for (const auto& m : slayout.members) {
                        if (m.name == member_name) { member_type_name = m.type; break; }
                    }
                    break;
                }
            }

            if (member_offset < 0) {
                throw std::runtime_error("Member \"" + member_name + "\" not found in any struct");
            }

            bool member_is_nested_struct = struct_layouts.contains(member_type_name);

            if (member_is_nested_struct) {
                w->emit_lea(base, mem{base, member_offset});
                reg_alloc.push<grp>(base);
            }
            else if (member_type_name == "float32" || member_type_name == "float64") {
                auto simd_reg = reg_alloc.allocate<simd128>();
                w->emit_movsd(simd_reg, mem{base, member_offset});
                if (member_type_name == "float32") w->emit_cvtsd2ss(simd_reg, simd_reg);
                reg_alloc.push<simd128>(simd_reg);
                reg_alloc.release<grp>(base);
            }
            else {
                w->emit_mov(base, mem{base, member_offset});
                reg_alloc.push<grp>(base);
            }
        }

        void handle_member_store(register_allocator& reg_alloc, ir_instr& instr) {
            auto member_name = std::get<std::string>(instr.operand);
            auto member_type = instr.type;

            std::int32_t member_offset = -1;
            for (const auto& [sname, slayout] : struct_layouts) {
                auto off = slayout.get_member_offset(member_name);
                if (off >= 0) { member_offset = off; break; }
            }

            if (member_offset < 0) {
                throw std::runtime_error("Member \"" + member_name + "\" not found in any struct for store");
            }

            if (member_type == "float32" || member_type == "float64") {
                auto fval = reg_alloc.pop<simd128>();
                auto base = reg_alloc.pop<grp>();

                if (member_type == "float32") w->emit_cvtss2sd(fval, fval);
                w->emit_movsd(mem{base, member_offset}, fval);

                reg_alloc.release<simd128>(fval);
                reg_alloc.release<grp>(base);
            }
            else {
                auto val = reg_alloc.pop<grp>();
                auto base = reg_alloc.pop<grp>();

                w->emit_mov(mem{base, member_offset}, val);

                reg_alloc.release<grp>(val);
                reg_alloc.release<grp>(base);
            }
        }

        bool reconcile_stack(register_allocator& reg_alloc, const std::vector<register_allocator::entry_view>& target) {
            auto cur = reg_alloc.state();
            if (cur.size() != target.size()) {
                if (debug) {
                    std::cout << YELLOW << "[CODEGEN] value stack depth mismatch at join (" << cur.size() << " vs " << target.size() << ")" << RESET << "\n";
                }
                return false;
            }

            std::vector<std::size_t> moves;
            for (std::size_t k = 0; k < cur.size(); k++) {
                if (!register_allocator::same_location(cur[k], target[k])) {
                    moves.push_back(k);
                }
            }

            for (auto k : moves) {
                const auto& c = cur[k];
                if (c.is_spilled) {
                    w->emit_push(mem{rbp, c.offset});
                }
                else if (std::holds_alternative<grp>(c.reg)) {
                    w->emit_push(std::get<grp>(c.reg));
                }
                else {
                    w->emit_lea(rsp, mem{rsp, -8});
                    w->emit_movsd(mem{rsp, 0}, std::get<simd128>(c.reg));
                }
            }

            for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
                const auto& t = target[*it];
                if (t.is_spilled) {
                    w->emit_pop(mem{rbp, t.offset});
                }
                else if (std::holds_alternative<grp>(t.reg)) {
                    w->emit_pop(std::get<grp>(t.reg));
                }
                else {
                    w->emit_movsd(std::get<simd128>(t.reg), mem{rsp, 0});
                    w->emit_lea(rsp, mem{rsp, 8});
                }
            }

            reg_alloc.restore(target);
            return true;
        }

        std::int32_t generate_code(ir_function& func) {
            register_allocator reg_alloc;

            std::unordered_map<std::string, std::int64_t> local_variable_map;
            std::unordered_map<std::string, std::string> local_variable_map_types;
            std::unordered_map<std::string, array_metadata> local_array_metadata;
            std::unordered_map<std::string, std::string> local_struct_var_types;
            std::string pending_struct_type;
            std::uint64_t index_to_push = (std::numeric_limits<std::uint64_t>::max)();
            std::int32_t totalsizes = 0;

            reg_alloc.configure(w.get(), &totalsizes, abi);

            std::unordered_map<std::string, std::size_t> label_map;
            std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;

            std::unordered_map<std::string, std::vector<register_allocator::entry_view>> label_states;
            bool reachable = true;

            auto sync_to_label = [&](const std::string& label) {
                if (auto it = label_states.find(label); it != label_states.end()) {
                    reconcile_stack(reg_alloc, it->second);
                }
                else {
                    label_states[label] = reg_alloc.state();
                }
            };

            bool is_reference_next = false;
            bool is_dereference_next = false;
            bool is_dereference_assign_next = false;

            std::size_t deref_count_normal = 0;
            std::size_t deref_count_assign = 0;

            push_mode push_mode = normal;

            handle_prologue_args(func, local_variable_map, local_variable_map_types, local_array_metadata, totalsizes);

            for (std::size_t i = 0; i < func.code.size(); i++) {
                auto& instr = func.code.at(i);

                switch(instr.op) {
                    case ir_opcode::op_push_shellcode:
                    {
                        if (std::holds_alternative<std::uint8_t>(instr.operand)) {
                            std::uint8_t val = std::get<std::uint8_t>(instr.operand);
                            w->push_byte(val);
                        }

                        break;
                    }
                    case ir_opcode::op_asm_code:
                    {
                        if (std::holds_alternative<std::string>(instr.operand)) {
                            std::string val = std::get<std::string>(instr.operand);

                            assembler a(val);
                            w->push_bytes(a.assemble());
                        }

                        break;
                    }
                    case ir_opcode::op_push_for_ret: {
                        if (std::holds_alternative<std::int64_t>(instr.operand)) {
                            std::int64_t val = std::get<std::int64_t>(instr.operand);
                            
                            w->emit_mov(rax, val);

                            push_mode = ret;
                        }

                        break;
                    }
                    case ir_opcode::op_push_single: {
                        if (std::holds_alternative<std::int64_t>(instr.operand)) {
                            std::int64_t val = std::get<std::int64_t>(instr.operand);
                            grp r = reg_alloc.allocate<grp>();
                            w->emit_mov(r, val);
                            reg_alloc.push<grp>(r);
                        }

                        break;
                    }
                    case ir_opcode::op_push: {
                        std::visit(push_visitor{this, reg_alloc, instr}, instr.operand);

                        break;
                    }
                    case ir_opcode::op_load: {
                        handle_load(reg_alloc, instr, local_variable_map, local_variable_map_types, local_struct_var_types,
                                    is_reference_next, is_dereference_next, deref_count_normal,
                                    is_dereference_assign_next, deref_count_assign);
                        break;
                    }
                    case ir_opcode::op_store: {
                        handle_store(reg_alloc, instr, local_variable_map, local_variable_map_types, local_struct_var_types, totalsizes);
                        break;
                    }
                    case ir_opcode::op_reference: {
                        is_reference_next = true;
                        break;
                    }
                    case ir_opcode::op_dereference: {
                        auto operand = std::get<std::int64_t>(instr.operand);
                        auto addr = reg_alloc.pop<grp>();
                        for (std::int64_t count = 0; count < operand; count++) {
                            w->emit_mov(addr, mem{addr});
                        }
                        reg_alloc.push<grp>(addr);
                        break;
                    }
                    case ir_opcode::op_dereference_assign: {
                        auto operand = std::get<std::int64_t>(instr.operand);
                        is_dereference_assign_next = true;
                        deref_count_assign = static_cast<std::size_t>(operand);
                        break;
                    }
                    case ir_opcode::op_store_at_addr: {
                        auto val = reg_alloc.pop<grp>();
                        auto addr = reg_alloc.pop<grp>();
                        w->emit_mov(mem{addr}, val);
                        reg_alloc.release<grp>(val);
                        reg_alloc.release<grp>(addr);
                        is_dereference_next = false;
                        break;
                    }
                    case ir_opcode::op_call:
                    case ir_opcode::op_call_indirect: {
                        handle_call(reg_alloc, instr);

                        break;
                    }
                    case ir_opcode::op_cmp:
                    case ir_opcode::op_cmpf32:
                    case ir_opcode::op_cmpf64: {
                        handle_cmp(reg_alloc, instr);

                        break;
                    }
                    case ir_opcode::op_setz:
                    case ir_opcode::op_setnz:
                    case ir_opcode::op_setl:
                    case ir_opcode::op_setle:
                    case ir_opcode::op_setg:
                    case ir_opcode::op_setge: {
                        handle_setcc(reg_alloc, instr);
                        break;
                    }
                    case ir_opcode::op_not: {
                        handle_not(reg_alloc, instr);
                        break;
                    }
                    case ir_opcode::op_cast:
                    case ir_opcode::op_bitcast: {
                        handle_cast(reg_alloc, instr);
                        break;
                    }
                    case ir_opcode::op_add:
                    case ir_opcode::op_sub:
                    case ir_opcode::op_mul:
                    case ir_opcode::op_div:
                    case ir_opcode::op_mod:
                    case ir_opcode::op_imul:
                    case ir_opcode::op_idiv:
                    case ir_opcode::op_imod:
                    case ir_opcode::op_negate:
                    case ir_opcode::op_bitwise_and:
                    case ir_opcode::op_bitwise_or:
                    case ir_opcode::op_bitwise_xor:
                    case ir_opcode::op_bitwise_not:
                    case ir_opcode::op_bitwise_lshift:
                    case ir_opcode::op_bitwise_rshift:
                    case ir_opcode::op_ibitwise_rshift: {
                        handle_arith(reg_alloc, instr);

                        break;
                    }
                    case ir_opcode::op_addf32:
                    case ir_opcode::op_addf64:
                    case ir_opcode::op_subf32:
                    case ir_opcode::op_subf64:
                    case ir_opcode::op_mulf32:
                    case ir_opcode::op_mulf64:
                    case ir_opcode::op_divf32:
                    case ir_opcode::op_divf64:
                    case ir_opcode::op_modf32:
                    case ir_opcode::op_modf64:
                    case ir_opcode::op_negatef32:
                    case ir_opcode::op_negatef64: {
                        handle_arith_float(reg_alloc, instr);

                        break;
                    }
                    case ir_opcode::op_array_decl: {
                        handle_array_decl(func, i, instr, local_variable_map, local_array_metadata, totalsizes);
                        break;
                    }
                    case ir_opcode::op_declare_where_to_store: {
                        index_to_push = std::get<std::uint64_t>(instr.operand);
                        break;
                    }
                    case ir_opcode::op_array_store_element: {
                        handle_array_store_element(reg_alloc, func, i, local_variable_map, local_array_metadata, index_to_push);
                        break;
                    }
                    case ir_opcode::op_array_access_element: {
                        handle_array_access_element(reg_alloc, func, i, instr, local_variable_map, local_array_metadata);
                        break;
                    }
                    case ir_opcode::op_struct_decl: {
                        pending_struct_type = std::get<std::string>(instr.operand);
                        break;
                    }
                    case ir_opcode::op_struct_store: {
                        handle_struct_store(reg_alloc, instr, local_variable_map, local_variable_map_types,
                                             local_struct_var_types, pending_struct_type, totalsizes);
                        break;
                    }
                    case ir_opcode::op_member_access: {
                        handle_member_access(reg_alloc, instr);
                        break;
                    }
                    case ir_opcode::op_member_store: {
                        handle_member_store(reg_alloc, instr);
                        break;
                    }
                    case ir_opcode::op_ret: {
                        handle_return(func, reg_alloc, push_mode);
                        push_mode = normal;
                        reachable = false;

                        break;
                    }
                    case ir_opcode::label: {
                        auto label_name = std::get<std::string>(instr.operand);

                        if (auto it = label_states.find(label_name); it != label_states.end()) {
                            if (!reachable) reg_alloc.restore(it->second);
                            else            reconcile_stack(reg_alloc, it->second);
                        }
                        else {
                            if (!reachable) reg_alloc.restore({});
                            label_states[label_name] = reg_alloc.state();
                        }
                        reachable = true;

                        label_map[label_name] = w->get_code().size();

                        break;
                    }
                    case ir_opcode::op_jmp: {
                        auto jump_instr = instr;
                        sync_to_label(std::get<std::string>(instr.operand));

                        w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});
                        jump_instructions.emplace_back(jump_instr, w->get_code().size() - 5);
                        reachable = false;

                        break;
                    }
                    case ir_opcode::op_jle:
                    case ir_opcode::op_jl:
                    case ir_opcode::op_jg:
                    case ir_opcode::op_jz:
                    case ir_opcode::op_jge:
                    case ir_opcode::op_jnz:
                    case ir_opcode::op_jb:
                    case ir_opcode::op_jbe:
                    case ir_opcode::op_ja:
                    case ir_opcode::op_jae: {
                        auto jump_instr = instr;
                        sync_to_label(std::get<std::string>(instr.operand));

                        w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});
                        jump_instructions.emplace_back(jump_instr, w->get_code().size() - 6);

                        break;
                    }
                    default: {
                        break;
                    }
                }
            }

            for (auto& [jinstr, loc] : jump_instructions) {
                auto label_name = std::get<std::string>(jinstr.operand);
                auto lit = label_map.find(label_name);

                if (lit == label_map.end()) {
                    throw std::runtime_error("Label \"" + label_name + "\" not found");
                }

                backpatch_jump(jinstr.op, loc, lit->second, w.get());
            }

            return totalsizes;
        }

        void compile_function(ir_function& func) {
            if (function_locs.contains(func.name) || func.is_external) {
                if (debug) {
                    std::cout << BLUE << "[CODEGEN] Already registered (skipping): " << YELLOW << func.name << RESET << "\n";
                }
                return;
            }

            if (debug) {
                std::cout << BLUE << "[CODEGEN] Compiling: " << YELLOW << func.name << RESET << "\n";
            }

            auto loc = w->get_code().size();
            function_locs.insert({func.name, loc});

            std::size_t stack_alloc_placeholder_location = 0;
            bool has_stack_alloc = false;

            if (!func.uses_shellcode && !func.uses_assembly) {
                w->emit_function_prologue();

                stack_alloc_placeholder_location = w->get_code().size();
                w->emit_sub(rsp, 0);
                has_stack_alloc = true;
            }

            std::int32_t totalsizes = generate_code(func);

            if (has_stack_alloc) {
                if (totalsizes % 16 != 0) {
                    totalsizes += 16 - (totalsizes % 16);
                }

                auto& code = w->get_code();
                for (int i = 0; i < 4; ++i) {
                    code[stack_alloc_placeholder_location + 3 + i] = static_cast<std::uint8_t>((totalsizes >> (i * 8)) & 0xFF);
                }

                if (debug) {
                    std::cout << BLUE << "[CODEGEN] Patched stack allocation: sub rsp, " << totalsizes << RESET << std::endl;
                }
            }

            if (debug) {
                std::cout << GREEN << "[CODEGEN] Compiled: " << YELLOW << func.name << GREEN << " (loc: " << loc << ")" RESET << "\n";
            }
        }

        public:

        codegen_v2(const std::vector<ir_function>& stack_ir, const std::vector<ir_struct>& ir_structs, const bool debug = false, const call_abi abi = host_abi(),
                   const std::vector<std::pair<std::string, std::string>>& globals = {})
            : debug(debug), abi(abi), w(new x86_64_writer{}), stack_ir(stack_ir), ir_structs(ir_structs) {
            // one 8-byte, zero-initialized slot per global, in a writable host
            // buffer (the code blob is execute-only). __global_init sets values.
            if (!globals.empty()) {
                std::int32_t off = 0;
                for (const auto& [gname, gtype] : globals) {
                    global_offsets[gname] = off;
                    global_types[gname] = gtype;
                    off += 8;
                }
                globals_storage = std::make_unique<std::uint8_t[]>(static_cast<std::size_t>(off));
                std::memset(globals_storage.get(), 0, static_cast<std::size_t>(off));
            }
            for (const auto& s : this->ir_structs) {
                struct_layout layout;
                layout.name = s.datatype;
                layout.total_size = 0;
                for (const auto& m : s.members) {
                    layout.members.push_back({m.name, m.datatype, 0});
                }
                struct_layouts[s.datatype] = layout;
            }

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
                            offset += 8;
                        }
                    }
                    if (offset != layout.total_size) {
                        layout.total_size = offset;
                        changed = true;
                    }
                }
            }

            for (auto& [name, layout] : struct_layouts) {
                if (layout.total_size == 0) {
                    layout.total_size = 8;
                }
            }

            for (const auto& func : this->stack_ir) {
                if (!func.type.empty()) {
                    function_return_types[func.name] = func.type;
                }

                std::vector<std::string> arg_types;
                std::vector<std::string> arg_names;
                arg_types.reserve(func.args.size());
                arg_names.reserve(func.args.size());
                for (auto& a : func.args) {
                    arg_types.push_back(a.type);
                    arg_names.push_back(a.name);
                }
                function_arg_types[func.name] = std::move(arg_types);
                function_arg_names[func.name] = std::move(arg_names);

                if (func.is_variadic || func.is_external) {
                    variable_arity_functions.insert(func.name);
                }
            }
        }

        template<typename FuncT>
        FuncT get_function(const std::string& name) const {
            auto it = function_locs.find(name);
            if (it == function_locs.end()) {
                throw std::runtime_error("Function \"" + name + "\" was never compiled");
            }
 
            return reinterpret_cast<FuncT>(w->setup_function(it->second));
        }

        const std::vector<std::uint8_t>& get_code() const { return w->get_code(); }

        void compile(const bool use_jit = true) {
            for (auto& func : stack_ir) {
                compile_function(func);
            }

            backpatch_calls();

            if (debug) {
                std::cout << GREEN << "[CODEGEN] Blob: " << RESET << "\n";
                w->print_bytes();
            }
        }
    };
}