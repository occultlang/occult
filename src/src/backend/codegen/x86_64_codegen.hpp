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

#ifdef __linux__
// dlsym, for an extern fn under the JIT
#include <dlfcn.h>
#endif
#ifdef _WIN64
#include <windows.h>
#include <psapi.h>
#endif

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
        std::int32_t union_group = 0;
        std::int32_t element_count = 0;
    };

    struct struct_layout {
        std::string name;
        std::vector<struct_member_info> members;
        std::int32_t total_size{};
        // the widest alignment any member needs, which is the struct's own
        std::int32_t alignment{1};
        // declared `extern struct Name;`: C owns the layout and never told us
        bool opaque{};

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
        {"f32", "float32"}, {"f64", "float64"}, {"char", "int8"},
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

            // the two sides of the value stack are not interchangeable; says
            // which way round, rather than letting std::get report an index
            template<typename RegT>
            static std::string wrong_side() {
                if constexpr (std::is_same_v<RegT, grp>) {
                    return "an integer operation was given a floating-point operand; "
                           "cast it to an integer type first";
                }
                else {
                    return "a floating-point operation was given an integer operand; "
                           "cast it to f32 or f64 first";
                }
            }

            template<typename RegT>
            RegT pop() {
                if (register_stack.empty()) {
                    throw std::runtime_error("register_allocator::pop() on empty stack");
                }

                auto entry = register_stack.back();
                if (!std::holds_alternative<RegT>(entry.reg)) {
                    throw std::runtime_error(wrong_side<RegT>());
                }
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
        // declared with extern fn: the signature is known, the address is not
        std::unordered_set<std::string> extern_functions;
        // resolve an extern fn through the loader instead of refusing it (JIT only)
        bool dynamic_externs = false;
        // thunk written for each extern reached, by offset into the code buffer
        std::unordered_map<std::string, std::uint32_t> extern_thunks;
        // the library a module's `link "..."` names, for each extern fn in it
        std::unordered_map<std::string, std::string> extern_library;
        public:
        struct import_use {
            std::string library;
            std::string name;
            std::size_t field;
        };
        bool image_imports = false;
        std::vector<import_use> import_thunks;
        // what the JIT opened for each library a module links
        std::unordered_map<std::string, void*> library_handles;
        std::vector<std::string> used_libraries;
        private:
        std::unordered_map<std::string, struct_layout> struct_layouts;
        // where this function stashed the hidden destination pointer a caller
        // passes in rdi for a return too big for registers; 0 when there is none
        std::int32_t sret_offset = 0;

        // C's width and alignment for a type; an unknown name answers eight
        static std::int32_t primitive_size(const std::string& t) {
            static const std::unordered_map<std::string, std::int32_t> m = {
                {"int8", 1}, {"uint8", 1}, {"bool", 1}, {"char", 1},
                {"int16", 2}, {"uint16", 2},
                {"int32", 4}, {"uint32", 4}, {"float32", 4},
                {"int64", 8}, {"uint64", 8}, {"float64", 8}, {"string", 8}};
            auto it = m.find(t);
            return it == m.end() ? 0 : it->second;
        }

        [[noreturn]] static void opaque_struct(const std::string& t) {
            throw std::runtime_error(
                "struct \"" + t + "\" is opaque: it was declared `extern struct " + t +
                ";`, so its size is not known here. Use " + t + "* and let C do the rest, "
                "or write out the members.");
        }

        std::int32_t type_size(const std::string& t) const {
            // a pointer or a reference is a machine word however it is spelled
            if (t.ends_with('*') || t.ends_with(kStructPtrSuffix) || t.ends_with("_reference")) {
                return 8;
            }
            if (const std::int32_t p = primitive_size(t); p != 0) {
                return p;
            }
            if (auto it = struct_layouts.find(t); it != struct_layouts.end()) {
                if (it->second.opaque) {
                    opaque_struct(t);
                }
                return it->second.total_size;
            }
            return 8;
        }

        std::int32_t type_align(const std::string& t) const {
            if (t.ends_with('*') || t.ends_with(kStructPtrSuffix) || t.ends_with("_reference")) {
                return 8;
            }
            if (const std::int32_t p = primitive_size(t); p != 0) {
                return p; // every scalar here aligns to its own width
            }
            if (auto it = struct_layouts.find(t); it != struct_layouts.end()) {
                if (it->second.opaque) {
                    opaque_struct(t);
                }
                return it->second.alignment;
            }
            return 8;
        }

        static std::int32_t align_up(std::int32_t v, std::int32_t a) {
            return a <= 1 ? v : ((v + a - 1) / a) * a;
        }
        std::vector<std::unique_ptr<std::uint8_t[]>> literal_pool;
        std::vector<std::pair<std::string, std::size_t>> call_patch_list;
        // function, offset of the lea disp32, and the + n of [rip + function + n]
        struct func_addr_patch {
            std::string name;
            std::size_t at;
            std::int64_t addend;
            std::size_t end;
        };
        std::vector<func_addr_patch> func_addr_patch_list;
        std::vector<std::pair<std::string, std::size_t>> asm_jmp_patch_list;   // jmp to an Occult function from an asm block (offset of the E9 opcode)

        void emit_function_addr(grp dst, const std::string& fname) {
            const auto v = static_cast<std::uint8_t>(dst);
            const std::uint8_t idx = (v >= static_cast<std::uint8_t>(r8) && v <= static_cast<std::uint8_t>(r15)) ? static_cast<std::uint8_t>(8 + (v - static_cast<std::uint8_t>(r8))) : static_cast<std::uint8_t>(v & 7);
            w->push_bytes({static_cast<std::uint8_t>(0x48 | (idx >= 8 ? 0x04 : 0)), 0x8D, static_cast<std::uint8_t>(((idx & 7) << 3) | 0x05), 0, 0, 0, 0});
            func_addr_patch_list.emplace_back(fname, w->get_code().size() - 4, 0, w->get_code().size());
        }

        std::unordered_map<std::string, std::int32_t> global_offsets;
        std::unordered_map<std::string, std::string> global_types;   
        std::unique_ptr<std::uint8_t[]> globals_storage;

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
        // where each global sits in the data blob, so an object file can name it
        const std::unordered_map<std::string, std::int32_t>& globals() const { return global_offsets; }
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
                        copy_bytes(rbp, offset, r, 0, layout_it->second.total_size, tmp);
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
                          std::unordered_map<std::string, array_metadata>& local_array_metadata,
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
            // a scalar records its size and the offset is the negation of it;
            // an array records the offset already
            const bool is_array = local_array_metadata.contains(var_name);
            std::int32_t offset = is_array ? static_cast<std::int32_t>(it->second) : -it->second;

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

                // a struct's value is its address, as in the load path below
                const bool base_is_struct = local_struct_var_types.contains(var_name);

                for (std::size_t size = 1; size <= deref_count_assign; size++) {
                    if (size == 1) {
                        if (base_is_struct) w->emit_lea(r, mem{rbp, offset});
                        else                w->emit_mov(r, mem{rbp, offset});
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
                    // an f32 is four bytes; the four above it are whatever was there
                    if (source_type == "float32") {
                        w->emit_mov(as_32(dst), as_32(dst));
                    }
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
                if ((target_is_float64 && source_is_f64) ||
                    (target_is_float32 && source_is_f32)) {
                    return;
                }
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

            // sixteen bytes or less in registers, one eightbyte each; bigger,
            // copy into the caller's space and return its address in rax
            if (push_mode != ret && returns_in_memory(func.type) && sret_offset != 0
                && !reg_alloc.empty()) {
                auto src = reg_alloc.pop<grp>();
                w->emit_mov(r11, src);
                w->emit_mov(rax, mem{rbp, sret_offset});
                grp tmp = reg_alloc.allocate<grp>();
                copy_bytes(rax, 0, r11, 0, type_size(func.type), tmp);
                reg_alloc.release<grp>(tmp);
                reg_alloc.release<grp>(src);
                w->emit_mov(rax, mem{rbp, sret_offset});

                if (!func.uses_shellcode && !func.uses_assembly) {
                    w->emit_function_epilogue();
                }
                w->emit_ret();
                return;
            }

            const bool struct_in_registers =
                abi == call_abi::sysv && push_mode != ret && is_struct_type(func.type)
                && type_size(func.type) <= 16 && !reg_alloc.empty();
            if (struct_in_registers) {
                auto src = reg_alloc.pop<grp>();
                // through r11, because rax is one of the registers the result goes into
                w->emit_mov(r11, src);
                const auto classes = struct_classes(func.type);
                std::size_t gp_i = 0, fp_i = 0;
                constexpr grp gpr[] = {rax, rdx};
                constexpr simd128 fpr[] = {xmm0, xmm1};
                for (std::size_t e = 0; e < classes.size() && e < 2; e++) {
                    const auto at = static_cast<std::int32_t>(e * 8);
                    if (classes[e]) w->emit_movsd(fpr[fp_i++], mem{r11, at});
                    else            w->emit_mov(gpr[gp_i++], mem{r11, at});
                }
                reg_alloc.release<grp>(src);
            }

            if (push_mode != ret && !struct_in_registers) {
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
            // sixteen bytes or less in one or two registers, reg then reg2;
            // larger goes on the stack and takes stack_slots of it
            std::int32_t struct_size = 0; // 0: not a struct
            bool two_parts = false;
            std::variant<grp, simd128> reg2;
            std::size_t stack_slots = 1;
        };

        void eightbyte_classes(const std::string& type, std::int32_t base,
                               std::vector<bool>& sse) const {
            auto it = struct_layouts.find(type);
            if (it == struct_layouts.end()) {
                // a scalar covers SSE only if it is itself a float
                const bool is_float = (type == "float32" || type == "float64");
                const std::int32_t size = type_size(type);
                for (std::int32_t at = base; at < base + size; at++) {
                    const std::size_t eb = static_cast<std::size_t>(at / 8);
                    if (eb < sse.size() && !is_float) {
                        sse[eb] = false;
                    }
                }
                return;
            }
            for (const auto& m : it->second.members) {
                eightbyte_classes(m.type, base + m.offset, sse);
            }
        }

        std::vector<bool> struct_classes(const std::string& type) const {
            const std::int32_t size = type_size(type);
            std::vector<bool> sse(static_cast<std::size_t>((size + 7) / 8), true);
            eightbyte_classes(type, 0, sse);
            return sse;
        }

        bool is_struct_type(const std::string& t) const {
            return struct_layouts.count(t) != 0;
        }

        bool returns_in_memory(const std::string& t) const {
            return abi == call_abi::sysv && is_struct_type(t) && type_size(t) > 16;
        }

        std::int32_t shadow_space() const { return abi == call_abi::win64 ? 32 : 0; }

        std::vector<arg_location> classify_args(const std::vector<bool>& is_float,
                                                const std::vector<std::string>& types = {},
                                                bool hidden_return_pointer = false) const {
            std::vector<arg_location> out;
            out.reserve(is_float.size());
            std::size_t stack_index = 0;

            auto type_of = [&](std::size_t k) -> std::string {
                return k < types.size() ? types[k] : std::string();
            };

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
            std::size_t gp_idx = hidden_return_pointer ? 1 : 0; // rdi is the destination
            std::size_t fp_idx = 0;

            for (std::size_t k = 0; k < is_float.size(); k++) {
                const std::string t = type_of(k);

                if (!t.empty() && is_struct_type(t)) {
                    arg_location loc{};
                    loc.struct_size = type_size(t);
                    const auto classes = struct_classes(t);

                    std::size_t need_gp = 0, need_fp = 0;
                    for (bool is_sse : classes) {
                        (is_sse ? need_fp : need_gp)++;
                    }
                    const bool fits = loc.struct_size <= 16 && classes.size() <= 2
                                      && gp_idx + need_gp <= 6 && fp_idx + need_fp <= 8;
                    if (!fits) {
                        loc.in_register = false;
                        loc.stack_index = stack_index;
                        loc.stack_slots = static_cast<std::size_t>((loc.struct_size + 7) / 8);
                        stack_index += loc.stack_slots;
                        out.push_back(loc);
                        continue;
                    }

                    loc.in_register = true;
                    for (std::size_t e = 0; e < classes.size(); e++) {
                        std::variant<grp, simd128> r =
                            classes[e] ? std::variant<grp, simd128>(fp[fp_idx++])
                                       : std::variant<grp, simd128>(gp[gp_idx++]);
                        if (e == 0) loc.reg = r;
                        else      { loc.reg2 = r; loc.two_parts = true; }
                    }
                    out.push_back(loc);
                    continue;
                }

                const bool f = is_float[k];
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
                                   std::unordered_map<std::string, std::string>& local_struct_var_types,
                                   std::int32_t& totalsizes) {
            if (func.uses_shellcode || func.uses_assembly) {
                return;
            }

            const bool sret = returns_in_memory(func.type);
            if (sret) {
                totalsizes += 8;
                sret_offset = -totalsizes;
                w->emit_mov(mem{rbp, sret_offset}, rdi);
            }

            if (func.args.empty()) {
                return;
            }

            std::vector<bool> is_float;
            std::vector<std::string> arg_types;
            for (auto& arg : func.args) {
                is_float.push_back(arg.type == "float32" || arg.type == "float64");
                arg_types.push_back(arg.type);
            }
            const auto locs = classify_args(is_float, arg_types, sret);

            for (std::size_t k = 0; k < func.args.size(); k++) {
                auto& arg = func.args[k];
                const auto& loc = locs[k];

                if (loc.struct_size != 0) {
                    const std::int32_t slot = ((loc.struct_size + 7) / 8) * 8;
                    totalsizes += slot;
                    const std::int32_t offset = -totalsizes;

                    if (loc.in_register) {
                        auto put = [&](const std::variant<grp, simd128>& r, std::int32_t at) {
                            if (std::holds_alternative<simd128>(r)) w->emit_movsd(mem{rbp, offset + at}, std::get<simd128>(r));
                            else                                    w->emit_mov(mem{rbp, offset + at}, std::get<grp>(r));
                        };
                        put(loc.reg, 0);
                        if (loc.two_parts) put(loc.reg2, 8);
                    }
                    else {
                        const auto src = static_cast<std::int32_t>(16 + shadow_space() + 8 * loc.stack_index);
                        for (std::size_t e = 0; e < loc.stack_slots; e++) {
                            const auto at = static_cast<std::int32_t>(e * 8);
                            w->emit_mov(rax, mem{rbp, src + at});
                            w->emit_mov(mem{rbp, offset + at}, rax);
                        }
                    }

                    local_variable_map.insert({arg.name, totalsizes});
                    local_variable_map_types.insert({arg.name, arg.type});
                    local_struct_var_types.insert({arg.name, arg.type});
                    continue;
                }

                totalsizes += 8;
                std::int32_t offset = -totalsizes;

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

        void handle_call(register_allocator& reg_alloc, ir_instr& instr, std::int32_t* totalsizes = nullptr) {
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
                            arg_is_float = false;
                        }
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

            // a struct argument's spill slot holds its address, not its bytes
            std::vector<std::string> arg_type_names(arg_count);
            for (std::size_t k = 0; k < arg_count; k++) {
                if (param_types && k < param_types->size()) {
                    arg_type_names[k] = (*param_types)[k];
                }
            }

            // what the callee returns decides whether rdi is already spoken for
            std::string call_ret;
            if (indirect) {
                call_ret = ind_ret;
            }
            else if (auto rt = function_return_types.find(name); rt != function_return_types.end()) {
                call_ret = rt->second;
            }
            const bool sret = returns_in_memory(call_ret) && totalsizes != nullptr;
            std::int32_t sret_at = 0;
            if (sret) {
                const std::int32_t slot = ((type_size(call_ret) + 7) / 8) * 8;
                *totalsizes += slot;
                sret_at = -*totalsizes;
            }

            const auto locs = classify_args(is_float, arg_type_names, sret);

            std::vector<std::size_t> stack_args;
            std::size_t stack_slots = 0;
            for (std::size_t k = 0; k < arg_count; k++) {
                if (!locs[k].in_register) {
                    stack_args.push_back(k);
                    stack_slots += locs[k].stack_slots;
                }
            }

            const bool padded = (stack_slots % 2) != 0;
            if (padded) {
                w->emit_sub(rsp, 8);
            }

            for (auto it = stack_args.rbegin(); it != stack_args.rend(); ++it) {
                const auto& loc = locs[*it];
                if (loc.struct_size == 0) {
                    w->emit_push(mem{rbp, args[*it].offset});
                    continue;
                }
                // the bytes themselves, highest eightbyte first
                w->emit_mov(rax, mem{rbp, args[*it].offset});
                for (std::size_t e = loc.stack_slots; e-- > 0;) {
                    w->emit_push(mem{rax, static_cast<std::int32_t>(e * 8)});
                }
            }

            for (std::size_t k = 0; k < arg_count; k++) {
                if (!locs[k].in_register) continue;
                const auto& loc = locs[k];

                if (loc.struct_size != 0) {
                    // each eightbyte into the register its class was assigned
                    w->emit_mov(rax, mem{rbp, args[k].offset});
                    auto take = [&](const std::variant<grp, simd128>& r, std::int32_t at) {
                        if (std::holds_alternative<simd128>(r)) w->emit_movsd(std::get<simd128>(r), mem{rax, at});
                        else                                    w->emit_mov(std::get<grp>(r), mem{rax, at});
                    };
                    take(loc.reg, 0);
                    if (loc.two_parts) take(loc.reg2, 8);
                    continue;
                }

                if (std::holds_alternative<simd128>(loc.reg)) w->emit_movsd(std::get<simd128>(loc.reg), mem{rbp, args[k].offset});
                else                                          w->emit_mov(std::get<grp>(loc.reg), mem{rbp, args[k].offset});
            }

            if (shadow_space()) {
                w->emit_sub(rsp, shadow_space());
            }

            if (abi == call_abi::sysv && variable_arity_functions.count(name)) {
                std::int32_t vector_args = 0;
                for (std::size_t k = 0; k < arg_count; k++) {
                    if (locs[k].in_register && std::holds_alternative<simd128>(locs[k].reg)) {
                        ++vector_args;
                    }
                }
                w->emit_mov(eax, vector_args);
            }

            if (sret) {
                w->emit_lea(rdi, mem{rbp, sret_at});
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

            const std::int32_t cleanup = static_cast<std::int32_t>(stack_slots * 8) + shadow_space() + (padded ? 8 : 0);
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
            if (sret && instr.result_used) {
                auto r = reg_alloc.allocate<grp>();
                w->emit_lea(r, mem{rbp, sret_at});
                reg_alloc.push<grp>(r);
            }
            else if (ret_ptr && instr.result_used && abi == call_abi::sysv
                && is_struct_type(*ret_ptr) && type_size(*ret_ptr) <= 16 && totalsizes != nullptr) {
                const std::int32_t slot = ((type_size(*ret_ptr) + 7) / 8) * 8;
                *totalsizes += slot;
                const std::int32_t at = -*totalsizes;

                const auto classes = struct_classes(*ret_ptr);
                std::size_t gp_i = 0, fp_i = 0;
                constexpr grp gpr[] = {rax, rdx};
                constexpr simd128 fpr[] = {xmm0, xmm1};
                for (std::size_t e = 0; e < classes.size() && e < 2; e++) {
                    const auto off = static_cast<std::int32_t>(e * 8);
                    if (classes[e]) w->emit_movsd(mem{rbp, at + off}, fpr[fp_i++]);
                    else            w->emit_mov(mem{rbp, at + off}, gpr[gp_i++]);
                }

                auto r = reg_alloc.allocate<grp>();
                w->emit_lea(r, mem{rbp, at});
                reg_alloc.push<grp>(r);
            }
            else if (ret_ptr && instr.result_used) {
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

        public:
        enum class reloc_kind { pc32, plt32, abs64 };
        enum class reloc_against { text, rodata, data };
        struct obj_reloc {
            // The symbol to resolve against, or empty for a section.
            std::string symbol;
            reloc_against section;
            std::size_t at;        // offset in .text of the field
            reloc_kind kind;
            std::int64_t addend;
        };
        std::vector<obj_reloc> object_relocs;
        std::vector<std::string> undefined_symbols;

        void relocate_for_object() {
            auto& code = w->get_code();
            auto undef = [&](const std::string& n) -> std::string {
                if (function_locs.count(n)) {
                    return n;
                }
                if (!extern_functions.count(n)) {
                    throw std::runtime_error("\"" + n + "\" is neither defined here nor declared `extern fn`");
                }
                const std::string sym = link_name(n);
                note_library(n);
                if (std::find(undefined_symbols.begin(), undefined_symbols.end(), sym)
                    == undefined_symbols.end()) {
                    undefined_symbols.push_back(sym);
                }
                return sym;
            };
            auto clear32 = [&](std::size_t at) {
                for (int i = 0; i < 4; ++i) {
                    code[at + static_cast<std::size_t>(i)] = 0;
                }
            };

            for (auto& [name, at, addend, insn_end] : func_addr_patch_list) {
                const std::string sym = undef(name);
                clear32(at);
                object_relocs.push_back({sym, reloc_against::text, at, reloc_kind::pc32,
                                         addend - static_cast<std::int64_t>(insn_end - at)});
            }
            for (auto& [name, loc] : asm_jmp_patch_list) {
                const std::string sym = undef(name);
                code[loc] = 0xE9;
                clear32(loc + 1);
                object_relocs.push_back({sym, reloc_against::text, loc + 1, reloc_kind::plt32, -4});
            }
            for (auto& [name, loc] : call_patch_list) {
                const std::string sym = undef(name);
                code[loc] = opcode::CALL_rel32;
                clear32(loc + 1);
                object_relocs.push_back({sym, reloc_against::text, loc + 1, reloc_kind::plt32, -4});
            }
        }
        private:

        // an extern fn has no address until a linker gives it one
        std::string extern_undefined(const std::string& name, const char* what) const {
            if (extern_functions.count(name)) {
                return std::string(what) + " \"" + name + "\" is extern; build an object file (-c) and link it";
            }
            return std::string(what) + " \"" + name + "\" which was never compiled";
        }

        static std::string link_name(const std::string& name) {
            const auto at = name.rfind("::");
            return at == std::string::npos ? name : name.substr(at + 2);
        }

        void note_library(const std::string& name) {
            if (auto lib = extern_library.find(name); lib != extern_library.end() &&
                std::find(used_libraries.begin(), used_libraries.end(), lib->second) == used_libraries.end()) {
                used_libraries.push_back(lib->second);
            }
        }

        // a name in one library the JIT opened, or null
        static void* find_in(void* handle, const std::string& name) {
#ifdef __linux__
            return dlsym(handle, name.c_str());
#elif defined(_WIN64)
            return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
            (void)handle;
            (void)name;
            return nullptr;
#endif
        }

        static void* find_extern(const std::string& name) {
#ifdef __linux__
            return dlsym(RTLD_DEFAULT, name.c_str());
#elif defined(_WIN64)
            HMODULE mods[512];
            DWORD needed = 0;
            if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) {
                const DWORD n = std::min<DWORD>(needed / sizeof(HMODULE), 512);
                for (DWORD i = 0; i < n; ++i) {
                    if (FARPROC p = GetProcAddress(mods[i], name.c_str())) {
                        return reinterpret_cast<void*>(p);
                    }
                }
            }
            return nullptr;
#else
            (void)name;
            return nullptr;
#endif
        }

        std::uint32_t extern_thunk(const std::string& name) {
            if (auto it = extern_thunks.find(name); it != extern_thunks.end()) {
                return it->second;
            }
            if (auto lib = extern_library.find(name); lib != extern_library.end()) {
                auto handle = library_handles.find(lib->second);
                void* in_lib = handle == library_handles.end() ? nullptr : find_in(handle->second, link_name(name));
                if (in_lib == nullptr) {
                    throw std::runtime_error("extern \"" + link_name(name) + "\" is not in \"" + lib->second +
                                             "\", the library its module links");
                }
                return write_extern_thunk(name, in_lib);
            }
            void* addr = find_extern(link_name(name));
            if (addr == nullptr) {
                throw std::runtime_error(
                    "extern \"" + link_name(name) + "\" is not in this process; load the "
                    "library that defines it with -l, or build an object file "
                    "(-c) and link it");
            }

            return write_extern_thunk(name, addr);
        }

        std::uint32_t write_extern_thunk(const std::string& name, void* addr) {
            auto& code = w->get_code();
            const auto off = static_cast<std::uint32_t>(code.size());
            const std::uint8_t stub[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
            code.insert(code.end(), std::begin(stub), std::end(stub));
            const auto target = reinterpret_cast<std::uint64_t>(addr);
            for (int i = 0; i < 8; ++i) {
                code.push_back(static_cast<std::uint8_t>((target >> (i * 8)) & 0xFF));
            }
            extern_thunks.emplace(name, off);
            return off;
        }

        std::uint32_t import_thunk(const std::string& name) {
            if (auto it = extern_thunks.find(name); it != extern_thunks.end()) {
                return it->second;
            }
            auto& code = w->get_code();
            const auto off = static_cast<std::uint32_t>(code.size());
            const std::uint8_t stub[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
            code.insert(code.end(), std::begin(stub), std::end(stub));
            auto lib = extern_library.find(name);
            import_thunks.push_back({lib == extern_library.end() ? std::string() : lib->second, link_name(name), off + 2});
            note_library(name);
            extern_thunks.emplace(name, off);
            return off;
        }

        void backpatch_calls() {
            auto resolve = [this](const std::string& name, const char* what) -> std::uint32_t {
                if (auto it = function_locs.find(name); it != function_locs.end()) {
                    return it->second;
                }
                if (dynamic_externs && extern_functions.count(name)) {
                    return extern_thunk(name);
                }
                if (image_imports && extern_functions.count(name)) {
                    return import_thunk(name);
                }
                throw std::runtime_error(extern_undefined(name, what));
            };
            auto& code = w->get_code();

            for (auto& [name, disp_loc, addend, insn_end] : func_addr_patch_list) {
                const std::uint32_t at = resolve(name, "Address taken of function");
                const std::int32_t disp = static_cast<std::int32_t>(static_cast<std::int64_t>(at) + addend)
                                          - static_cast<std::int32_t>(insn_end);
                for (int i = 0; i < 4; ++i) {
                    code[disp_loc + i] = static_cast<std::uint8_t>((disp >> (i * 8)) & 0xFF);
                }
            }

            for (auto& [name, loc] : asm_jmp_patch_list) {
                const std::uint32_t at = resolve(name, "asm block jumps to");
                const std::int32_t offset = static_cast<std::int32_t>(at) - static_cast<std::int32_t>(loc + 5);
                code[loc] = 0xE9;
                for (int i = 0; i < 4; ++i) {
                    code[loc + 1 + i] = static_cast<std::uint8_t>((offset >> (i * 8)) & 0xFF);
                }
            }

            for (auto& [name, loc] : call_patch_list) {
                const std::uint32_t at = resolve(name, "Call target");
                std::int32_t offset = static_cast<std::int32_t>(at) - static_cast<std::int32_t>(loc + 5);

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

            std::size_t element_size = static_cast<std::size_t>(type_size(type));
            if (element_size == 0) element_size = 8;
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
            std::int32_t elem_size = type_size(store_meta.type);
            if (elem_size == 0) elem_size = 8;
            auto layout_it = struct_layouts.find(store_meta.type);
            const bool is_struct_elem = layout_it != struct_layouts.end()
                                     && !layout_it->second.opaque;

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

            auto store_struct_at_offset = [&](std::int32_t element_offset) {
                auto src = reg_alloc.pop<grp>();
                auto tmp = reg_alloc.allocate<grp>();
                copy_bytes(rbp, element_offset, src, 0, layout_it->second.total_size, tmp);
                reg_alloc.release<grp>(tmp);
                reg_alloc.release<grp>(src);
            };

            auto store_at_offset = [&](std::int32_t element_offset) {
                if (is_struct_elem) {
                    store_struct_at_offset(element_offset);
                    return;
                }
                if (is_float_array) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    if (store_meta.type == "float32") w->emit_movss(mem{rbp, element_offset}, simd_reg);
                    else                              w->emit_movsd(mem{rbp, element_offset}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                }
                else {
                    auto content = reg_alloc.pop<grp>();
                    store_scalar(mem{rbp, element_offset}, content, store_meta.type);
                    reg_alloc.release<grp>(content);
                }
            };

            if (has_constant_index) {
                store_at_offset(array_base_offset + static_cast<std::int32_t>(elem_size * index_to_push));
                index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                return;
            }

            if (inline_const_index.has_value()) {
                std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(elem_size * inline_const_index.value());

                if (is_struct_elem) {
                    auto src = reg_alloc.pop<grp>();
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);
                    auto tmp = reg_alloc.allocate<grp>();
                    copy_bytes(rbp, element_offset, src, 0, layout_it->second.total_size, tmp);
                    reg_alloc.release<grp>(tmp);
                    reg_alloc.release<grp>(src);
                    return;
                }
                if (is_float_array) {
                    auto simd_reg = reg_alloc.pop<simd128>();
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);
                    if (store_meta.type == "float32") w->emit_movss(mem{rbp, element_offset}, simd_reg);
                    else                              w->emit_movsd(mem{rbp, element_offset}, simd_reg);
                    reg_alloc.release<simd128>(simd_reg);
                }
                else {
                    auto content = reg_alloc.pop<grp>();
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);
                    store_scalar(mem{rbp, element_offset}, content, store_meta.type);
                    reg_alloc.release<grp>(content);
                }
                return;
            }

            if (index_to_push != (std::numeric_limits<std::uint64_t>::max)()) {
                store_at_offset(array_base_offset + static_cast<std::int32_t>(elem_size * index_to_push));
                index_to_push = (std::numeric_limits<std::uint64_t>::max)();
                return;
            }

            auto scale_index = [&](grp index_reg) {
                switch (elem_size) {
                case 1: break;
                case 2: w->emit_shl(index_reg, 1); break;
                case 4: w->emit_shl(index_reg, 2); break;
                case 8: w->emit_shl(index_reg, 3); break;
                default: w->emit_imul(index_reg, index_reg, elem_size); break;
                }
            };

            if (is_struct_elem) {
                auto src = reg_alloc.pop<grp>();
                auto index_reg = reg_alloc.pop<grp>();

                auto addr_reg = reg_alloc.allocate<grp>();
                w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                scale_index(index_reg);
                w->emit_add(addr_reg, index_reg);

                auto tmp = reg_alloc.allocate<grp>();
                copy_bytes(addr_reg, 0, src, 0, layout_it->second.total_size, tmp);
                reg_alloc.release<grp>(tmp);
                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(index_reg);
                reg_alloc.release<grp>(src);
            }
            else if (is_float_array) {
                auto simd_reg = reg_alloc.pop<simd128>();
                auto index_reg = reg_alloc.pop<grp>();

                auto addr_reg = reg_alloc.allocate<grp>();
                w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                scale_index(index_reg);
                w->emit_add(addr_reg, index_reg);

                if (store_meta.type == "float32") w->emit_movss(mem{addr_reg}, simd_reg);
                else                              w->emit_movsd(mem{addr_reg}, simd_reg);

                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(index_reg);
                reg_alloc.release<simd128>(simd_reg);
            }
            else {
                auto content = reg_alloc.pop<grp>();
                auto index_reg = reg_alloc.pop<grp>();

                auto addr_reg = reg_alloc.allocate<grp>();
                w->emit_lea(addr_reg, mem{rbp, array_base_offset});
                scale_index(index_reg);
                w->emit_add(addr_reg, index_reg);

                store_scalar(mem{addr_reg}, content, store_meta.type);

                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(index_reg);
                reg_alloc.release<grp>(content);
            }
        }

        void handle_array_access_element(register_allocator& reg_alloc, ir_function& func, std::size_t i, ir_instr& instr,
                                          std::unordered_map<std::string, std::int64_t>& local_variable_map,
                                          std::unordered_map<std::string, array_metadata>& local_array_metadata,
                                          bool& is_reference_next) {
            auto array_name = std::get<std::string>(instr.operand);
            auto it = local_variable_map.find(array_name);
            if (it == local_variable_map.end()) {
                throw std::runtime_error("Array \"" + array_name + "\" not found");
            }

            auto& local_metadata = local_array_metadata[array_name];
            bool is_float_array = (local_metadata.type == "float32" || local_metadata.type == "float64");
            std::int32_t array_base_offset = it->second;
            std::int32_t elem_size = type_size(local_metadata.type);
            if (elem_size == 0) elem_size = 8;
            auto layout_it = struct_layouts.find(local_metadata.type);
            const bool is_struct_elem = layout_it != struct_layouts.end()
                                     && !layout_it->second.opaque;

            if (i >= 2 && func.code.at(i - 1).op == ir_opcode::op_mark_for_array_access && func.code.at(i - 2).op == ir_opcode::op_push) {
                auto inline_idx = extract_uint_operand(func.code.at(i - 2).operand);

                if (inline_idx.has_value()) {
                    auto idx_reg = reg_alloc.pop<grp>();
                    reg_alloc.release<grp>(idx_reg);

                    std::int32_t element_offset = array_base_offset + static_cast<std::int32_t>(elem_size * inline_idx.value());

                    if (is_reference_next || is_struct_elem) {
                        is_reference_next = false;
                        auto r = reg_alloc.allocate<grp>();
                        w->emit_lea(r, mem{rbp, element_offset});
                        reg_alloc.push<grp>(r);
                        return;
                    }

                    if (is_float_array) {
                        auto simd_reg = reg_alloc.allocate<simd128>();
                        if (local_metadata.type == "float32") w->emit_movss(simd_reg, mem{rbp, element_offset});
                        else                                   w->emit_movsd(simd_reg, mem{rbp, element_offset});
                        reg_alloc.push<simd128>(simd_reg);
                    }
                    else {
                        auto r = reg_alloc.allocate<grp>();
                        load_scalar(r, mem{rbp, element_offset}, local_metadata.type);
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
            switch (elem_size) {
            case 1: break;
            case 2: w->emit_shl(true_index, 1); break;
            case 4: w->emit_shl(true_index, 2); break;
            case 8: w->emit_shl(true_index, 3); break;
            default: w->emit_imul(true_index, true_index, elem_size); break;
            }
            w->emit_add(addr_reg, true_index);

            if (is_reference_next || is_struct_elem) {
                is_reference_next = false;
                w->emit_mov(true_index, addr_reg);
                reg_alloc.release<grp>(addr_reg);
                reg_alloc.push<grp>(true_index);
                return;
            }

            if (is_float_array) {
                auto simd_reg = reg_alloc.allocate<simd128>();
                if (local_metadata.type == "float32") w->emit_movss(simd_reg, mem{addr_reg});
                else                                   w->emit_movsd(simd_reg, mem{addr_reg});
                reg_alloc.release<grp>(addr_reg);
                reg_alloc.release<grp>(true_index);
                reg_alloc.push<simd128>(simd_reg);
            }
            else {
                load_scalar(true_index, mem{addr_reg}, local_metadata.type);
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
                        if (layout_it->second.opaque) {
                            opaque_struct(pending_struct_type);
                        }
                        std::int32_t struct_size = layout_it->second.total_size;
                        struct_size = ((struct_size + 15) / 16) * 16;
                        totalsizes += struct_size;

                        if (!reg_alloc.empty()) {
                            auto src = reg_alloc.pop<grp>();
                            auto tmp = reg_alloc.allocate<grp>();
                            copy_bytes(rbp, -totalsizes, src, 0, layout_it->second.total_size, tmp);
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
                    auto layout_it = struct_layouts.find(pending_struct_type);
                    if (layout_it != struct_layouts.end() && !is_struct_ptr) {
                        // Full struct copy from the by-value temporary whose
                        // address is at reg_alloc's top into the existing slot.
                        auto src = reg_alloc.pop<grp>();
                        auto tmp = reg_alloc.allocate<grp>();
                        copy_bytes(rbp, -it->second, src, 0, layout_it->second.total_size, tmp);
                        reg_alloc.release<grp>(tmp);
                        reg_alloc.release<grp>(src);
                    }
                    else {
                        auto r = reg_alloc.pop<grp>();
                        std::int32_t offset = -it->second;
                        w->emit_mov(mem{rbp, offset}, r);
                        reg_alloc.release<grp>(r);
                    }
                }
            }
        }

        void copy_bytes(grp dst_base, std::int32_t dst_off,
                        grp src_base, std::int32_t src_off,
                        std::int32_t size, grp tmp) {
            std::int32_t at = 0;
            for (; at + 8 <= size; at += 8) {
                w->emit_mov(tmp, mem{src_base, src_off + at});
                w->emit_mov(mem{dst_base, dst_off + at}, tmp);
            }
            for (std::int32_t step = 4; step >= 1; step /= 2) {
                while (at + step <= size) {
                    switch (step) {
                    case 4:
                        w->emit_mov(as_32(tmp), mem{as_32(src_base), src_off + at});
                        w->emit_mov(mem{as_32(dst_base), dst_off + at}, as_32(tmp));
                        break;
                    case 2:
                        w->emit_movzx(tmp, mem{src_base, src_off + at}, false);
                        w->emit_mov(mem{as_16(dst_base), dst_off + at}, as_16(tmp));
                        break;
                    default:
                        w->emit_movzx(tmp, mem{src_base, src_off + at});
                        w->emit_mov(mem{as_8(dst_base), dst_off + at}, as_8(tmp));
                        break;
                    }
                    at += step;
                }
            }
        }

        static bool type_is_signed(const std::string& t) {
            return t == "int8" || t == "int16" || t == "int32" || t == "int64" || t == "char";
        }

        void load_scalar(grp dst, const mem& at, const std::string& type) {
            const bool sign = type_is_signed(type);
            switch (type_size(type)) {
            case 1: sign ? w->emit_movsx(dst, at) : w->emit_movzx(dst, at); break;
            case 2: sign ? w->emit_movsx(dst, at, false) : w->emit_movzx(dst, at, false); break;
            case 4:
                if (sign) {
                    w->emit_movsxd(dst, at);
                }
                else {
                    // a 32-bit destination zeroes the top half
                    w->emit_mov(as_32(dst), mem{as_32(at.reg), at.disp});
                }
                break;
            default: w->emit_mov(dst, at); break;
            }
        }

        void store_scalar(const mem& at, grp val, const std::string& type) {
            switch (type_size(type)) {
            case 1: w->emit_mov(mem{as_8(at.reg), at.disp}, as_8(val)); break;
            case 2: w->emit_mov(mem{as_16(at.reg), at.disp}, as_16(val)); break;
            case 4: w->emit_mov(mem{as_32(at.reg), at.disp}, as_32(val)); break;
            default: w->emit_mov(at, val); break;
            }
        }

        const struct_member_info* find_member(const std::string& owner,
                                              const std::string& member_name) const {
            if (!owner.empty()) {
                if (auto it = struct_layouts.find(owner); it != struct_layouts.end()) {
                    for (const auto& m : it->second.members) {
                        if (m.name == member_name) {
                            return &m;
                        }
                    }
                }
            }
            for (const auto& [sname, slayout] : struct_layouts) {
                for (const auto& m : slayout.members) {
                    if (m.name == member_name) {
                        return &m;
                    }
                }
            }
            return nullptr;
        }

        void handle_member_access(register_allocator& reg_alloc, ir_instr& instr,
                                  bool address_only = false) {
            auto member_name = std::get<std::string>(instr.operand);
            auto base = reg_alloc.pop<grp>();

            const struct_member_info* found = find_member(instr.owner, member_name);
            if (found == nullptr) {
                throw std::runtime_error("Member \"" + member_name + "\" not found in any struct");
            }
            const std::int32_t member_offset = found->offset;
            const std::string member_type_name = found->type;

            bool member_is_nested_struct = struct_layouts.contains(member_type_name);

            // a nested struct is only ever an address, so the two cases are one
            if (member_is_nested_struct || address_only) {
                w->emit_lea(base, mem{base, member_offset});
                reg_alloc.push<grp>(base);
            }
            else if (member_type_name == "float32" || member_type_name == "float64") {
                auto simd_reg = reg_alloc.allocate<simd128>();
                // an f32 member is four bytes of float, not a double to narrow
                if (member_type_name == "float32") w->emit_movss(simd_reg, mem{base, member_offset});
                else                               w->emit_movsd(simd_reg, mem{base, member_offset});
                reg_alloc.push<simd128>(simd_reg);
                reg_alloc.release<grp>(base);
            }
            else {
                load_scalar(base, mem{base, member_offset}, member_type_name);
                reg_alloc.push<grp>(base);
            }
        }

        void handle_member_store(register_allocator& reg_alloc, ir_instr& instr) {
            auto member_name = std::get<std::string>(instr.operand);
            auto member_type = instr.type;

            const struct_member_info* found = find_member(instr.owner, member_name);
            if (found == nullptr) {
                throw std::runtime_error("Member \"" + member_name + "\" not found in any struct for store");
            }
            const std::int32_t member_offset = found->offset;

            if (member_type.empty()) {
                member_type = found->type;
            }

            if (member_type == "float32" || member_type == "float64") {
                auto fval = reg_alloc.pop<simd128>();
                auto base = reg_alloc.pop<grp>();

                // four bytes for an f32, and no widening to double on the way
                if (member_type == "float32") w->emit_movss(mem{base, member_offset}, fval);
                else                          w->emit_movsd(mem{base, member_offset}, fval);

                reg_alloc.release<simd128>(fval);
                reg_alloc.release<grp>(base);
            }
            else {
                auto val = reg_alloc.pop<grp>();
                auto base = reg_alloc.pop<grp>();

                store_scalar(mem{base, member_offset}, val, member_type);

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

        // asm(x86_64) { ... }: either the whole body of a naked function, or inline in a function
        void handle_asm(register_allocator& reg_alloc, ir_function& func, ir_instr& instr, std::unordered_map<std::string, std::int64_t>& local_variable_map,
                        std::unordered_map<std::string, std::string>& local_variable_map_types, std::unordered_map<std::string, array_metadata>& local_array_metadata,
                        std::unordered_map<std::string, std::string>& local_struct_var_types, std::int32_t& totalsizes, std::int32_t& save_slots) {
            if (instr.type != "x86_64") {
                throw std::runtime_error("asm(" + instr.type + ") is not supported: this compiler targets x86_64");
            }

            const bool naked = func.uses_assembly;

            assembler::variable_lookup vars;
            if (!naked) {
                vars = [&](const std::string& name) -> std::optional<asm_variable> {
                    if (global_offsets.contains(name) && !local_variable_map.contains(name)) {
                        throw std::runtime_error("{" + name + "} is a global; use &" + name + " to get its address");
                    }
                    auto it = local_variable_map.find(name);
                    if (it == local_variable_map.end()) {
                        return std::nullopt;
                    }
                    if (local_array_metadata.contains(name) || local_struct_var_types.contains(name)) {
                        throw std::runtime_error("{" + name + "} is an array or struct; take its address in Occult and use that");
                    }
                    const auto type_it = local_variable_map_types.find(name);
                    const std::string type = type_it == local_variable_map_types.end() ? "int64" : type_it->second;
                    int bits = 64;
                    if (type == "int8" || type == "uint8" || type == "bool") bits = 8;
                    else if (type == "int16" || type == "uint16") bits = 16;
                    else if (type == "int32" || type == "uint32" || type == "float32") bits = 32;
                    return asm_variable{static_cast<std::int32_t>(-it->second), bits};
                };
            }

            assembler a(std::get<std::string>(instr.operand), vars,
                        [this](const std::string& name) { return function_arg_types.contains(name); },
                        [this](const std::string& name) { return global_offsets.contains(name); });

            asm_output res;
            try {
                res = a.assemble();
            }
            catch (const std::runtime_error& e) {
                throw std::runtime_error("in function '" + func.name + "': " + e.what());
            }

            static const grp saved[] = {rbx, r12, r13, r14, r15};
            if (!naked) {
                reg_alloc.spill_all();
                if (save_slots == 0) {
                    totalsizes += 40;
                    save_slots = totalsizes;
                }
                for (int i = 0; i < 5; ++i) {
                    w->emit_mov(mem{rbp, -save_slots + 8 * i}, saved[i]);
                }
            }

            const std::size_t base = w->get_code().size();
            w->push_bytes(res.bytes);
            for (const auto& use : res.calls) {
                (use.is_jump ? asm_jmp_patch_list : call_patch_list).emplace_back(use.name, base + use.at);
            }
            for (const auto& use : res.addresses) {
                func_addr_patch_list.emplace_back(use.name, base + use.at, use.addend, base + use.end);
            }
            for (const auto& use : res.globals) {
                const std::int32_t off = global_offsets.at(use.name);
                const std::int64_t abs = reinterpret_cast<std::int64_t>(globals_storage.get() + off);
                auto& code = w->get_code();
                for (int i = 0; i < 8; ++i) {
                    code[base + use.at + i] = static_cast<std::uint8_t>((abs >> (i * 8)) & 0xFF);
                }
                global_relocs.emplace_back(base + use.at, off);
            }

            if (!naked) {
                for (int i = 0; i < 5; ++i) {
                    w->emit_mov(saved[i], mem{rbp, -save_slots + 8 * i});
                }
            }
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
            std::int32_t asm_save_slots = 0; // frame offset of the callee-saved register save area (0 = not allocated)

            reg_alloc.configure(w.get(), &totalsizes, abi);

            std::unordered_map<std::string, std::size_t> label_map;
            std::vector<std::pair<ir_instr, std::size_t>> jump_instructions;

            auto source_label = [](const std::string& name) {
                return name.starts_with("user_") ? name.substr(5) : name;
            };

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
            std::string pointee;
            std::size_t pointee_at = static_cast<std::size_t>(-1);
            auto pointee_bytes = [](const std::string& t) -> int {
                if (t == "int8" || t == "uint8" || t == "bool" || t == "char"
                    || t == "i8" || t == "u8") return 1;
                if (t == "int16" || t == "uint16" || t == "i16" || t == "u16") return 2;
                if (t == "int32" || t == "uint32" || t == "i32" || t == "u32"
                    || t == "float32" || t == "f32") return 4;
                return 8;
            };
            auto pointee_signed = [](const std::string& t) {
                return t == "int8" || t == "int16" || t == "int32" || t == "char"
                    || t == "i8" || t == "i16" || t == "i32";
            };
            auto pointee_float = [](const std::string& t) {
                return t == "float32" || t == "float64" || t == "f32" || t == "f64";
            };
            auto pointee_double = [](const std::string& t) {
                return t == "float64" || t == "f64";
            };
            bool is_dereference_next = false;
            bool is_dereference_assign_next = false;

            std::size_t deref_count_normal = 0;
            std::size_t deref_count_assign = 0;

            push_mode push_mode = normal;

            handle_prologue_args(func, local_variable_map, local_variable_map_types, local_array_metadata, local_struct_var_types, totalsizes);

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
                            handle_asm(reg_alloc, func, instr, local_variable_map, local_variable_map_types, local_array_metadata, local_struct_var_types, totalsizes, asm_save_slots);
                        }

                        break;
                    }
                    case ir_opcode::op_declare_local:
                    {
                        const auto& decl_name = std::get<std::string>(instr.operand);
                        if (!local_variable_map.contains(decl_name) && !global_offsets.contains(decl_name)) {
                            totalsizes += 8;
                            local_variable_map.insert({decl_name, totalsizes});
                            local_variable_map_types.insert({decl_name, instr.type});

                            grp zero = reg_alloc.allocate<grp>();
                            w->emit_mov(zero, std::int64_t(0));
                            w->emit_mov(mem{rbp, -totalsizes}, zero);
                            reg_alloc.release<grp>(zero);
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
                    case ir_opcode::op_sizeof: {
                        const auto& tname = std::get<std::string>(instr.operand);
                        std::int64_t size = 8; // pointers
                        if (!tname.ends_with('*')) {
                            if (primitive_size(tname) != 0 || struct_layouts.count(tname)) {
                                size = type_size(tname);
                            }
                            else {
                                if (auto m_it = local_array_metadata.find(tname);
                                    m_it != local_array_metadata.end()) {
                                    std::int64_t n = 1;
                                    for (auto d : m_it->second.dimensions) {
                                        n *= static_cast<std::int64_t>(d);
                                    }
                                    grp ar = reg_alloc.allocate<grp>();
                                    w->emit_mov(ar, n * type_size(m_it->second.type));
                                    reg_alloc.push<grp>(ar);
                                    break;
                                }

                                std::string of;
                                if (auto l_it = local_variable_map_types.find(tname);
                                    l_it != local_variable_map_types.end()) {
                                    of = l_it->second;
                                }
                                else if (auto a_it = local_struct_var_types.find(tname);
                                         a_it != local_struct_var_types.end()) {
                                    of = a_it->second;
                                }
                                else if (auto g_it = global_types.find(tname);
                                         g_it != global_types.end()) {
                                    of = g_it->second;
                                }
                                if (of.empty()) {
                                    throw std::runtime_error(
                                        "sizeof: '" + tname + "' is neither a type nor a variable in scope");
                                }
                                size = type_size(of);
                            }
                        }
                        grp r = reg_alloc.allocate<grp>();
                        w->emit_mov(r, size);
                        reg_alloc.push<grp>(r);
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
                                    local_array_metadata, is_reference_next, is_dereference_next, deref_count_normal,
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
                    case ir_opcode::op_pointee: {
                        pointee = std::get<std::string>(instr.operand);
                        pointee_at = i;
                        break;
                    }
                    case ir_opcode::op_dereference: {
                        auto operand = std::get<std::int64_t>(instr.operand);
                        auto addr = reg_alloc.pop<grp>();
                        // every step but the last walks a pointer, a machine word
                        const std::string t = (pointee_at + 1 == i) ? pointee : std::string();
                        if (pointee_float(t)) {
                            // walk the pointer, then read the last step as a float
                            for (std::int64_t count = 0; count + 1 < operand; count++) {
                                w->emit_mov(addr, mem{addr});
                            }
                            auto dst = reg_alloc.allocate<simd128>();
                            if (pointee_double(t)) w->emit_movsd(dst, mem{addr});
                            else                   w->emit_movss(dst, mem{addr});
                            reg_alloc.release<grp>(addr);
                            reg_alloc.push<simd128>(dst);
                            break;
                        }
                        const int width = pointee_bytes(t);
                        const bool sign = pointee_signed(t);
                        for (std::int64_t count = 0; count < operand; count++) {
                            const bool last = count + 1 == operand;
                            if (!last || width == 8) {
                                w->emit_mov(addr, mem{addr});
                            }
                            else if (width == 1) {
                                if (sign) w->emit_movsx(addr, mem{addr});
                                else      w->emit_movzx(addr, mem{addr});
                            }
                            else if (width == 2) {
                                if (sign) w->emit_movsx(addr, mem{addr}, false);
                                else      w->emit_movzx(addr, mem{addr}, false);
                            }
                            else {
                                if (sign) w->emit_movsxd(addr, mem{addr});
                                // a 32-bit destination zeroes the top half
                                else      w->emit_mov(as_32(addr), mem{as_32(addr)});
                            }
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
                        {
                            const std::string t = (pointee_at + 1 == i) ? pointee : std::string();
                            if (pointee_float(t)) {
                                auto fval = reg_alloc.pop<simd128>();
                                auto faddr = reg_alloc.pop<grp>();
                                if (pointee_double(t)) w->emit_movsd(mem{faddr}, fval);
                                else                   w->emit_movss(mem{faddr}, fval);
                                reg_alloc.release<simd128>(fval);
                                reg_alloc.release<grp>(faddr);
                                is_dereference_next = false;
                                break;
                            }
                        }
                        auto val = reg_alloc.pop<grp>();
                        auto addr = reg_alloc.pop<grp>();
                        // as narrow as the pointer said
                        const std::string t = (pointee_at + 1 == i) ? pointee : std::string();
                        switch (pointee_bytes(t)) {
                        case 1: w->emit_mov(mem{as_8(addr)}, as_8(val)); break;
                        case 2: w->emit_mov(mem{as_16(addr)}, as_16(val)); break;
                        case 4: w->emit_mov(mem{as_32(addr)}, as_32(val)); break;
                        default: w->emit_mov(mem{addr}, val); break;
                        }
                        reg_alloc.release<grp>(val);
                        reg_alloc.release<grp>(addr);
                        is_dereference_next = false;
                        break;
                    }
                    case ir_opcode::op_call:
                    case ir_opcode::op_call_indirect: {
                        handle_call(reg_alloc, instr, &totalsizes);

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
                        handle_array_access_element(reg_alloc, func, i, instr, local_variable_map, local_array_metadata, is_reference_next);
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
                    case ir_opcode::op_member_addr: {
                        handle_member_access(reg_alloc, instr, true);
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

                        // two labels with one name would silently make the second win
                        if (label_map.contains(label_name)) {
                            throw std::runtime_error("Label \"" + source_label(label_name) + "\" appears twice in one function");
                        }

                        label_map[label_name] = w->get_code().size();

                        break;
                    }
                    case ir_opcode::op_error_jump: {
                        const auto label = std::get<std::string>(instr.operand);
                        const auto carried = reg_alloc.state();
                        w->push_bytes({0x74, 0x05}); // jz past the jmp: nothing in flight
                        reg_alloc.restore({});
                        sync_to_label(label);
                        w->push_bytes({opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP, opcode::NOP});
                        jump_instructions.emplace_back(ir_instr(ir_opcode::op_jmp, label), w->get_code().size() - 5);
                        reg_alloc.restore(carried);

                        break;
                    }
                    // an error is leaving the function: return whatever is in rax,
                    // except that a caller copying a big struct out reads the
                    // address it passed, so that goes back as usual
                    case ir_opcode::op_propagate: {
                        reg_alloc.restore({});
                        if (returns_in_memory(func.type) && sret_offset != 0) {
                            w->emit_mov(rax, mem{rbp, sret_offset});
                        }
                        handle_return(func, reg_alloc, push_mode);
                        push_mode = normal;
                        reachable = false;

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
                    throw std::runtime_error("Label \"" + source_label(label_name) + "\" not found");
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

            while (w->get_code().size() % 16 != 0) {
                w->get_code().push_back(0xCC); // int3: never reached, loud if it is
            }

            auto loc = w->get_code().size();
            function_locs.insert({func.name, loc});
            sret_offset = 0; // per function, set by the prologue if needed

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
                layout.opaque = s.opaque;
                for (const auto& m : s.members) {
                    layout.members.push_back({m.name, m.datatype, 0, m.union_group, m.element_count});
                }
                struct_layouts[s.datatype] = layout;
            }

            constexpr int kMaxStructLayoutPasses = 16;
            bool changed = true;
            for (int pass = 0; pass < kMaxStructLayoutPasses && changed; ++pass) {
                changed = false;
                for (auto& [name, layout] : struct_layouts) {
                    if (layout.opaque) {
                        continue;
                    }
                    auto member_bytes = [this](const struct_member_info& m) -> std::int32_t {
                        const std::int32_t elem = type_size(m.type);
                        return m.element_count > 0 ? elem * m.element_count : elem;
                    };
                    std::int32_t offset = 0;
                    std::int32_t worst = 1;
                    for (std::size_t i = 0; i < layout.members.size();) {
                        const auto group = layout.members[i].union_group;
                        if (group == 0) {
                            // one plain member
                            auto& m = layout.members[i];
                            const std::int32_t a = type_align(m.type);
                            offset = align_up(offset, a);
                            m.offset = offset;
                            offset += member_bytes(m);
                            worst = std::max(worst, a);
                            ++i;
                            continue;
                        }
                        std::int32_t group_align = 1;
                        std::int32_t group_size = 0;
                        std::size_t j = i;
                        while (j < layout.members.size() && layout.members[j].union_group == group) {
                            group_align = std::max(group_align, type_align(layout.members[j].type));
                            group_size = std::max(group_size, member_bytes(layout.members[j]));
                            ++j;
                        }
                        offset = align_up(offset, group_align);
                        for (std::size_t k = i; k < j; ++k) {
                            layout.members[k].offset = offset;
                        }
                        offset += group_size;
                        worst = std::max(worst, group_align);
                        i = j;
                    }
                    const std::int32_t size = std::max(align_up(offset, worst), 1);
                    if (size != layout.total_size || worst != layout.alignment) {
                        layout.total_size = size;
                        layout.alignment = worst;
                        changed = true;
                    }
                }
            }

            if (debug) {
                for (const auto& [name, layout] : struct_layouts) {
                    std::cout << BLUE << "[CODEGEN] struct " << RESET << name << " size "
                              << layout.total_size << " align " << layout.alignment << "\n";
                    for (const auto& m : layout.members) {
                        std::cout << "    +" << m.offset << "  " << m.type << " " << m.name << "\n";
                    }
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
                if (func.is_external) {
                    extern_functions.insert(func.name);
                    if (!func.library.empty()) {
                        extern_library[func.name] = func.library;
                    }
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

        std::size_t function_arity(const std::string& name) const {
            auto it = function_arg_types.find(name);
            return it == function_arg_types.end() ? 0 : it->second.size();
        }

        void compile(const bool use_jit = true, const bool for_object = false) {
            dynamic_externs = use_jit && !for_object;

            for (auto& func : stack_ir) {
                compile_function(func);
            }

            if (for_object) {
                relocate_for_object();
            }
            else {
                backpatch_calls();
            }

            if (debug) {
                std::cout << GREEN << "[CODEGEN] Blob: " << RESET << "\n";
                w->print_bytes();
            }
        }
    };
}