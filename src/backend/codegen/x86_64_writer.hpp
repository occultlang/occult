#pragma once
#include <bitset>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include "writer.hpp"
#include "x86_64_defs.hpp"

// rewritten entirety of the x64writer to be more readable and understandable
// will document as i go, more
// based on https://ref.x86asm.net/coder64.html

// disabled address_size_override 0x67, only calculate using 64-bit addressing
// for now. (has bug in codegen)

#define REG_RANGE(to_cmp, start, end) (to_cmp >= start && to_cmp <= end)

#define IS_STACK_PTR(dest) (dest == rsp || dest == esp || dest == sp || dest == spl || dest == r12 || dest == r12d || dest == r12w || dest == r12b)
#define NOT_STACK_PTR(dest) (!IS_STACK_PTR(dest))

#define IS_STACK_BASE_PTR(dest) (dest == rbp || dest == ebp || dest == bp || dest == bpl || dest == r13 || dest == r13d || dest == r13w || dest == r13b)
#define NOT_STACK_BASE_PTR(dest) (!IS_STACK_BASE_PTR(dest))

namespace occult::x86_64 {
    enum class mem_mode : std::uint8_t { none, disp, reg, scaled_index, abs };

    struct mem {
        grp reg;
        std::size_t scale;
        grp index;
        std::int32_t disp;
        mem_mode mode = mem_mode::none;
        mem_mode second_mode = mem_mode::none;

        mem() = delete;

        explicit mem(const grp& reg) : reg(reg), scale(0), index(), disp(0) { mode = mem_mode::reg; }

        explicit mem(const grp& reg, const std::int32_t& disp) : reg(reg), scale(0), index(), disp(disp) { mode = mem_mode::disp; }

        explicit mem(const grp& reg, const grp& index) : reg(reg), scale(0), index(index), disp(0) { mode = mem_mode::scaled_index; }

        explicit mem(const grp& reg, const grp& index, const std::size_t& scale) : reg(reg), scale(scale), index(index), disp(0) { mode = mem_mode::scaled_index; }

        explicit mem(const grp& reg, const grp& index, const std::size_t& scale, const std::int32_t& disp) : reg(reg), scale(scale), index(index), disp(disp) {
            mode = mem_mode::scaled_index;
            second_mode = mem_mode::disp;
        }
    };

    template <typename T>
    concept IsGrp = std::is_enum_v<T> || std::is_base_of_v<grp, T>;

    template <typename T>
    concept IsMem = std::is_base_of_v<mem, T>;

    template <typename T>
    concept IsSignedImm = std::is_integral_v<T> && std::is_signed_v<T>;

    template <typename T>
    concept IsUnsignedImm = std::is_integral_v<T> && std::is_unsigned_v<T>;

    template <typename T>
    concept IsSignedImm8 = IsSignedImm<T> && sizeof(T) == 1;

    template <typename T>
    concept IsUnsignedImm8 = IsUnsignedImm<T> && sizeof(T) == 1;

    template <typename T>
    concept IsSimd128 = std::is_enum_v<T> || std::is_base_of_v<simd128, T>;

#define REG_TO_REG_ARG const IsGrp auto &dest, const IsGrp auto &base
#define REG_TO_MEM_ARG const IsMem auto &dest, const IsGrp auto &base
#define MEM_TO_REG_ARG const IsGrp auto &dest, const IsMem auto &base
#define SIGNED_IMM_TO_REG_ARG const IsGrp auto &dest, const IsSignedImm auto &imm
#define UNSIGNED_IMM_TO_REG_ARG const IsGrp auto &dest, const IsUnsignedImm auto &imm
#define SIGNED_IMM_TO_MEM_ARG const IsMem auto &dest, const IsSignedImm auto &imm
#define UNSIGNED_IMM_TO_MEM_ARG const IsMem auto &dest, const IsUnsignedImm auto &imm
#define REG_ARG const IsGrp auto& _reg
#define SIGNED_IMM_ARG const IsSignedImm auto& imm
#define UNSIGNED_IMM_ARG const IsUnsignedImm auto& imm
#define REL8_ARG const std::uint8_t& target_address
#define MEM_ARG const IsMem auto& m
#define REL32_ARG const std::uint32_t& target_address
#define REG_TO_SIMD128_ARG const IsSimd128 auto &dest, const IsGrp auto &base
#define SIMD128_TO_REG_ARG const IsGrp auto &dest, const IsSimd128 auto &base
#define SIMD128_TO_SIMD128_ARG const IsSimd128 auto &dest, const IsGrp auto &base
#define SIMD128_TO_MEM_ARG const IsMem auto &dest, const IsSimd128 auto &base
#define MEM_TO_SIMD128_ARG const IsSimd128 auto &dest, const IsMem auto &base

    class x86_64_writer : public writer {
        enum class imm_mode : std::uint8_t {
            do_8,
            do_16,
            no_64,
            normal,
        };

        template <typename IntType = std::int8_t>
        void emit_imm(IntType imm, const int size) {
            for (int i = 0; i < size; ++i) {
                push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
            }
        }

        template <typename IntType = std::int8_t> // template if we want to use
                                                  // unsigned values
        void emit_imm8(IntType imm) {
            emit_imm<IntType>(imm, 1);
        }

        template <typename IntType = std::int16_t>
        void emit_imm16(IntType imm) {
            emit_imm<IntType>(imm, 2);
        }

        template <typename IntType = std::int32_t>
        void emit_imm32(IntType imm) {
            emit_imm<IntType>(imm, 4);
        }

        template <typename IntType = std::int64_t>
        void emit_imm64(IntType imm) {
            emit_imm<IntType>(imm, 8);
        }

#define CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH                                                                                                                                                                                                     \
    if (is_2bOPC) {                                                                                                                                                                                                                            \
        push_byte(k2ByteOpcodePrefix);                                                                                                                                                                                                         \
    }

        void emit_reg_to_reg(const opcode& op8, const opcode& op, const grp& dest, const grp& base, const bool is_2bOPC = false) {
            if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, dil) || REG_RANGE(base, r8b, r15b) || REG_RANGE(base, al, dil)) {
                if (REG_RANGE(dest, r8b, r15b) && REG_RANGE(base, r8b, r15b)) {
                    push_byte(rex_rb);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(dest, r8b, r15b)) {
                    push_byte(rex_b);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(base, r8b, r15b)) {
                    push_byte(rex_r);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else {
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }

                push_byte(op8);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
            }
            else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, di) || REG_RANGE(base, r8w, r15w) || REG_RANGE(base, ax, di)) {
                push_byte(other_prefix::operand_size_override); // 16-bit operand size

                if (REG_RANGE(dest, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                    push_byte(rex_rb);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(dest, r8w, r15w)) {
                    push_byte(rex_b);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(base, r8w, r15w)) {
                    push_byte(rex_r);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else {
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
            }
            else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, edi) || REG_RANGE(base, r8d, r15d) || REG_RANGE(base, eax, edi)) {
                if (REG_RANGE(dest, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                    push_byte(rex_rb);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(dest, r8d, r15d)) {
                    push_byte(rex_b);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(base, r8d, r15d)) {
                    push_byte(rex_r);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else {
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
            }
            else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rdi) || REG_RANGE(base, r8, r15) || REG_RANGE(base, rax, rdi)) {
                if (REG_RANGE(dest, r8, r15) && REG_RANGE(base, r8, r15)) {
                    push_byte(rex_wrb);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(dest, r8, r15)) {
                    push_byte(rex_wb);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else if (REG_RANGE(base, r8, r15)) {
                    push_byte(rex_wr);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }
                else {
                    push_byte(rex_w);
                    CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
            }
        }

        void emit_reg_to_simd128(const float_opcode_2b& op, const grp& dest, const simd128& base, std::vector<std::uint8_t> prefix_special_2b, const bool is_2bOPC = true) {

            push_bytes(prefix_special_2b);

            if (REG_RANGE(dest, r8, r15)) {
                push_byte(rex_wb);
            }
            else if (REG_RANGE(dest, rax, rdi)) {
                push_byte(rex_w);
            }
            else if (REG_RANGE(dest, r8d, r15d)) {
                push_byte(rex_b);
            }

            CHECK_2BYTE_OPCODE_PREFIX_AND_PUSH

            push_byte(op);
            push_byte(modrm(mod_field::register_direct, base, rebase_register(dest)));
        }

        /*void check_simd128_r2m(const float_opcode_2b& op, const mem& dest, const simd128& src) {
            return;
        }*/

        void print_binary(uint8_t value) { std::cout << std::bitset<8>(value) << '\n'; }

        void print_modrm_debug(uint8_t modrm_byte, uint8_t reg_field, uint8_t rm_field, uint8_t mod) {
            std::cout << "ModR/M byte: ";
            print_binary(modrm_byte);

            std::cout << "breakdown: ";
            std::cout << std::bitset<2>(static_cast<uint8_t>(mod)) << " ";
            std::cout << std::bitset<3>(reg_field) << " ";
            std::cout << std::bitset<3>(rm_field) << " = 0x" << std::hex << std::uppercase << +modrm_byte << std::dec << "\n";
        }

        void emit_simd128_to_mem(const opcode& op, const simd128& base, const mem& dest, const std::vector<std::uint8_t>& prefix) {
            bool is_2bOPC = true;

            if (debug)
                std::cout << "dest reg: " << reg_to_string(dest.reg) << std::endl;

            if (dest.reg == rip) { // rip relative
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::indirect, rm_field::rip_relative, base));
                emit_imm32(dest.disp);
            }
            else if (NOT_STACK_PTR(dest.reg) && NOT_STACK_BASE_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [reg] (this was broken, i had to change to disp8???)
                // std::cout << "Emitting register based addressing for memory\n\tDest: "
                // << reg_to_string(base) << "\n\tSource: [" << reg_to_string(dest.reg) <<
                // "]\n\tMode = mem_mode::reg;\n";
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                // std::cout << "Rebased (Source): " <<
                // reg_to_string(rebase_register(base)) << std::endl; std::cout <<
                // "Rebased (Dest): " << reg_to_string(rebase_register(dest.reg)) <<
                // std::endl;
                push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), base));
                push_byte(0);
            }
            else if (IS_STACK_BASE_PTR(dest.reg) && NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rbp]
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), base));
                emit_imm8(0);
            }
            else if (NOT_STACK_BASE_PTR(dest.reg) && IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rsp]
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::indirect, rm_field::indexed, base));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
            }
            else if (NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) {
                // [reg + disp32] (we are always going to do disp32)
                if (debug)
                    printf("disp32 debug simd128\n");

                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                uint8_t reg_field = static_cast<uint8_t>(base) & 7;

                uint8_t rm_field = rebase_register(dest.reg);
                uint8_t modrm_byte = (0b10 << 6) | (reg_field << 3) | rm_field;

                if (debug)
                    print_modrm_debug(modrm_byte, reg_field, rm_field, 0b10);

                push_byte(modrm_byte);
                emit_imm32(dest.disp);
            }
            else if (IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [rsp + disp32]
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, base));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::none) { // [reg + SIB]
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::indirect, rm_field::indexed, base));
                push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::disp) {
                // [reg + SIB + disp]
                check_reg_r2m_simd(op, op, dest, static_cast<grp>(base), is_2bOPC, prefix);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, base));
                push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }
        }

        void check_reg_r2m_simd(const opcode& op8, const opcode& op, const mem& dest, const grp& base, const bool is_2bOPC = false, const std::vector<std::uint8_t>& prefix = {}) {
            if (debug) {
                std::cout << "is_2bOPC: " << is_2bOPC << std::endl;
            }

            auto emit_2b = [&]() {
                push_bytes(prefix);
                push_byte(k2ByteOpcodePrefix);
            };

            const bool dest_r8b = REG_RANGE(dest.reg, r8b, r15b);
            const bool dest_8 = REG_RANGE(dest.reg, al, dil);
            const bool dest_r8w = REG_RANGE(dest.reg, r8w, r15w);
            const bool dest_16 = REG_RANGE(dest.reg, ax, di);
            const bool dest_r8d = REG_RANGE(dest.reg, r8d, r15d);
            const bool dest_32 = REG_RANGE(dest.reg, eax, edi);
            const bool dest_r8q = REG_RANGE(dest.reg, r8, r15);
            const bool dest_64 = REG_RANGE(dest.reg, rax, rdi);

            const bool base_r8b = REG_RANGE(base, r8b, r15b);
            const bool base_8 = REG_RANGE(base, al, dil);
            const bool base_r8w = REG_RANGE(base, r8w, r15w);
            const bool base_16 = REG_RANGE(base, ax, di);
            const bool base_r8d = REG_RANGE(base, r8d, r15d);
            const bool base_32 = REG_RANGE(base, eax, edi);
            const bool base_r8q = REG_RANGE(base, r8, r15);
            const bool base_64 = REG_RANGE(base, rax, rdi);

            const bool index_r8q = dest.mode == mem_mode::scaled_index && REG_RANGE(dest.index, r8, r15);
            const bool index_64 = dest.mode == mem_mode::scaled_index && REG_RANGE(dest.index, rax, rdi);

            std::uint8_t rex = rex::rex;

            if (dest_r8b || dest_8 || base_r8b || base_8) {
                bool needs_rex = false;
                if (dest_r8b)
                    rex |= rex_bits::b;
                if (base_r8b)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (dest.reg == spl || dest.reg == bpl || dest.reg == sil || dest.reg == dil || base == spl || base == bpl || base == sil || base == dil) {
                    needs_rex = true;
                }
                if (needs_rex || rex != rex::rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op8);

                return;
            }

            if (dest_r8w || dest_16 || base_r8w || base_16) {
                push_byte(other_prefix::operand_size_override);
                if (dest_r8w)
                    rex |= rex_bits::b;
                if (base_r8w)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (rex != rex::rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op);

                return;
            }

            if (dest_r8d || dest_32 || base_r8d || base_32) {
                if (dest_r8d)
                    rex |= rex_bits::b;
                if (base_r8d)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (rex != rex::rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op);

                return;
            }

            if (dest_r8q || dest_64 || base_r8q || base_64 || index_r8q || index_64) {
                rex |= rex_bits::w;
                if (dest_r8q)
                    rex |= rex_bits::b;
                if (base_r8q)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;

                push_byte(rex);
                emit_2b();
                push_byte(op);

                return;
            }

            if (dest.reg == sp || dest.reg == esp || dest.reg == rsp || dest.reg == spl || base == sp || base == esp || base == rsp || base == spl) {
                if (dest.reg == sp || base == sp) {
                    push_byte(other_prefix::operand_size_override);
                }
                else if (dest.reg == rsp || dest.reg == spl || base == rsp || base == spl) {
                    rex |= rex_bits::w;
                    push_byte(rex);
                }

                emit_2b();
                push_byte((dest.reg == spl || base == spl) ? op8 : op);

                return;
            }

            push_byte(rex_bits::w);
            emit_2b();
            push_byte(op);
        }

        void check_reg_r2m(const opcode& op8, const opcode& op, const mem& dest, const grp& base, const bool is_2bOPC = false) {
            auto emit_2b = [&]() {
                if (is_2bOPC)
                    push_byte(k2ByteOpcodePrefix);
            };

            const bool dest_r8b = REG_RANGE(dest.reg, r8b, r15b);
            const bool dest_8 = REG_RANGE(dest.reg, al, dil);
            const bool dest_r8w = REG_RANGE(dest.reg, r8w, r15w);
            const bool dest_16 = REG_RANGE(dest.reg, ax, di);
            const bool dest_r8d = REG_RANGE(dest.reg, r8d, r15d);
            const bool dest_32 = REG_RANGE(dest.reg, eax, edi);
            const bool dest_r8q = REG_RANGE(dest.reg, r8, r15);
            const bool dest_64 = REG_RANGE(dest.reg, rax, rdi);

            const bool base_r8b = REG_RANGE(base, r8b, r15b);
            const bool base_8 = REG_RANGE(base, al, dil);
            const bool base_r8w = REG_RANGE(base, r8w, r15w);
            const bool base_16 = REG_RANGE(base, ax, di);
            const bool base_r8d = REG_RANGE(base, r8d, r15d);
            const bool base_32 = REG_RANGE(base, eax, edi);
            const bool base_r8q = REG_RANGE(base, r8, r15);
            const bool base_64 = REG_RANGE(base, rax, rdi);

            const bool index_r8q = dest.mode == mem_mode::scaled_index && REG_RANGE(dest.index, r8, r15);
            const bool index_64 = dest.mode == mem_mode::scaled_index && REG_RANGE(dest.index, rax, rdi);

            std::uint8_t rex = rex::rex;

            if (dest_r8b || dest_8 || base_r8b || base_8) {
                bool needs_rex = false;
                if (dest_r8b)
                    rex |= rex_bits::b;
                if (base_r8b)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (dest.reg == spl || dest.reg == bpl || dest.reg == sil || dest.reg == dil || base == spl || base == bpl || base == sil || base == dil) {
                    needs_rex = true;
                }
                if (needs_rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op8);

                return;
            }

            if (dest_r8w || dest_16 || base_r8w || base_16) {
                push_byte(other_prefix::operand_size_override);
                if (dest_r8w)
                    rex |= rex_bits::b;
                if (base_r8w)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (rex != rex::rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op);

                return;
            }

            if (dest_r8d || dest_32 || base_r8d || base_32) {
                if (dest_r8d)
                    rex |= rex_bits::b;
                if (base_r8d)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;
                if (rex != rex::rex)
                    push_byte(rex);

                emit_2b();
                push_byte(op);

                return;
            }

            if (dest_r8q || dest_64 || base_r8q || base_64 || index_r8q || index_64) {
                rex |= rex_bits::w;
                if (dest_r8q)
                    rex |= rex_bits::b;
                if (base_r8q)
                    rex |= rex_bits::r;
                if (index_r8q)
                    rex |= rex_bits::x;

                push_byte(rex);
                emit_2b();
                push_byte(op);

                return;
            }

            if (dest.reg == sp || dest.reg == esp || dest.reg == rsp || dest.reg == spl || base == sp || base == esp || base == rsp || base == spl) {
                if (dest.reg == sp || base == sp) {
                    push_byte(other_prefix::operand_size_override);
                }
                else if (dest.reg == rsp || dest.reg == spl || base == rsp || base == spl) {
                    rex |= rex_bits::w;
                    push_byte(rex);
                }

                emit_2b();
                push_byte((dest.reg == spl || base == spl) ? op8 : op);

                return;
            }

            push_byte(rex_bits::w);
            emit_2b();
            push_byte(op);
        }

        void emit_reg_to_mem(const opcode& op8, const opcode& op, const mem& dest, const grp& base, const bool is_2bOPC = false) {
            if (dest.reg == rip) { // rip relative
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::indirect, rm_field::rip_relative, rebase_register(base)));
                emit_imm32(dest.disp);
            }
            else if (NOT_STACK_PTR(dest.reg) && NOT_STACK_BASE_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [reg] (this was broken, i had to change to disp8???)
                if (debug)
                    std::cout << "Emitting register based addressing for memory\n\tDest: " << reg_to_string(base) << "\n\tSource: [" << reg_to_string(dest.reg) << "]\n\tMode = mem_mode::reg;\n";

                check_reg_r2m(op8, op, dest, base, is_2bOPC);

                if (debug)
                    std::cout << "Rebased (Source): " << reg_to_string(rebase_register(base)) << std::endl;

                if (debug)
                    std::cout << "Rebased (Dest): " << reg_to_string(rebase_register(dest.reg)) << std::endl;

                push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), rebase_register(base)));
                push_byte(0);
            }
            else if (IS_STACK_BASE_PTR(dest.reg) && NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rbp]
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), rebase_register(base)));
                emit_imm8(0);
            }
            else if (NOT_STACK_BASE_PTR(dest.reg) && IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rsp]
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::indirect, rm_field::indexed, rebase_register(dest.reg)));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
            }
            else if (NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) {
                // [reg + disp32] (we are always going to do disp32)
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::disp32, rebase_register(dest.reg), rebase_register(base)));
                emit_imm32(dest.disp);
            }
            else if (IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [rsp + disp32]
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, rebase_register(dest.reg)));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::none) { // [reg + SIB]
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                if (IS_STACK_BASE_PTR(dest.reg)) {
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rebase_register(base)));
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }
                else {
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rebase_register(base)));
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                }
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::disp) {
                // [reg + SIB + disp]
                check_reg_r2m(op8, op, dest, base, is_2bOPC);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, rebase_register(base)));
                push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }
        }

        // rm field indicates which operation is used for things such as (ADD, OR,
        // etc. ) (opcodes are 0x80 for 8bit and 0x81 for higher than 8bit) this can
        // also be used for 0xC7 and 0xC6 this is for the opcodes which require
        // "different modes?"
        template <std::integral T>
        void emit_reg_imm(const opcode& op8, const opcode& op, const grp& dest, const T& imm, const rm_field& rm, const bool is_signed = true, const imm_mode im_m = imm_mode::normal) {
            if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, dil)) {
                if (REG_RANGE(dest, r8b, r15b)) {
                    push_byte(rex_b);
                }

                push_byte(op8);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                if (is_signed) {
                    emit_imm8(imm);
                }
                else {
                    emit_imm8<std::uint8_t>(imm);
                }
            }
            else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, di)) {
                push_byte(other_prefix::operand_size_override); // 16-bit operand size
                if (REG_RANGE(dest, r8w, r15w)) {
                    push_byte(rex_b);
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                if (im_m == imm_mode::do_8) {
                    if (is_signed) {
                        emit_imm8(imm);
                    }
                    else {
                        emit_imm8<std::uint8_t>(imm);
                    }
                }
                else {
                    if (is_signed) {
                        emit_imm16(imm);
                    }
                    else {
                        emit_imm16<std::uint16_t>(imm);
                    }
                }
            }
            else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, edi)) {
                if (REG_RANGE(dest, r8d, r15d)) {
                    push_byte(rex_b);
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                if (im_m == imm_mode::do_8) {
                    if (is_signed) {
                        emit_imm8(imm);
                    }
                    else {
                        emit_imm8<std::uint8_t>(imm);
                    }
                }
                else {
                    if (is_signed) {
                        emit_imm32(imm);
                    }
                    else {
                        emit_imm32<std::uint32_t>(imm);
                    }
                }
            }
            else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rdi)) {
                if (REG_RANGE(dest, r8, r15)) {
                    push_byte(rex_wb);
                }
                else {
                    push_byte(rex_w);
                }

                push_byte(op);
                push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                if (im_m == imm_mode::no_64) {
                    if (is_signed) {
                        emit_imm32(imm);
                    }
                    else {
                        emit_imm32<std::uint32_t>(imm);
                    }
                }
                else if (im_m == imm_mode::do_8) {
                    if (is_signed) {
                        emit_imm8(imm);
                    }
                    else {
                        emit_imm8<std::uint8_t>(imm);
                    }
                }
                else if (im_m == imm_mode::normal) {
                    if (is_signed) {
                        emit_imm64(imm);
                    }
                    else {
                        emit_imm64<std::uint64_t>(imm);
                    }
                }
            }
        }

        // uses OP + REG & prefixes for correct registers & requires no MOD/RM
        template <std::integral T>
        void emit_reg_imm_basic(const opcode& op8, const opcode& op, const grp& dest, const T& imm, bool is_signed = true) {
            if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, dil)) {
                if (REG_RANGE(dest, r8b, r15b)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op8) + static_cast<std::uint8_t>(rebase_register(dest)));

                if (is_signed) {
                    emit_imm8(imm);
                }
                else {
                    emit_imm8<std::uint8_t>(imm);
                }
            }
            else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, di)) {
                push_byte(other_prefix::operand_size_override); // 16-bit operand size
                if (REG_RANGE(dest, r8w, r15w)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(dest)));

                if (is_signed) {
                    emit_imm16(imm);
                }
                else {
                    emit_imm16<std::uint16_t>(imm);
                }
            }
            else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, edi)) {
                if (REG_RANGE(dest, r8d, r15d)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(dest)));

                if (is_signed) {
                    emit_imm32(imm);
                }
                else {
                    emit_imm32<std::uint32_t>(imm);
                }
            }
            else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rdi)) {
                if (REG_RANGE(dest, r8, r15)) {
                    push_byte(rex_wb);
                }
                else {
                    push_byte(rex_w);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(dest)));

                if (is_signed) {
                    emit_imm64(imm);
                }
                else {
                    emit_imm64<std::uint64_t>(imm);
                }
            }
        }

        // believe this is finished, too tired to document, will get to it in the
        // future i guess lmao
        template <std::integral T>
        void emit_mem_imm(const opcode& op8, const opcode& op, const mem& dest, const T& imm, const rm_field& rm, const bool is_signed = true, const imm_mode im_m = imm_mode::normal) {
            if (dest.reg == rip) { // rip relative
                throw std::invalid_argument("Can't move immediate to rip-relative address.");
            }
            if (NOT_STACK_PTR(dest.reg) && NOT_STACK_BASE_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [reg]
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::indirect, rebase_register(dest.reg), rm));
            }
            else if (IS_STACK_BASE_PTR(dest.reg) && NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rbp]
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), rm));
                emit_imm8(0);
            }
            else if (NOT_STACK_BASE_PTR(dest.reg) && IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) {
                // [rsp]
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::indirect, rm_field::indexed, rm));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
            }
            else if (NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) {
                // [reg + disp32] (we are always going to do disp32)
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::disp32, rebase_register(dest.reg), rm));
                emit_imm32(dest.disp);
            }
            else if (IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [rsp + disp32]
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, rm));
                push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::none) { // [reg + SIB]
                check_reg_r2m(op8, op, dest, dest.reg);
                if (IS_STACK_BASE_PTR(dest.reg)) {
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rm));
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }
                else {
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rm));
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                }
            }
            else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::disp) {
                // [reg + SIB + disp]
                check_reg_r2m(op8, op, dest, dest.reg);
                push_byte(modrm(mod_field::disp32, rm_field::indexed, rm));
                push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                emit_imm32(dest.disp);
            }

            switch (im_m) {
            case imm_mode::do_8:
                {
                    if (is_signed) {
                        emit_imm8(imm);
                    }
                    else {
                        emit_imm8<std::uint8_t>(imm);
                    }

                    break;
                }
            case imm_mode::do_16:
                {
                    if (is_signed) {
                        emit_imm16(imm);
                    }
                    else {
                        emit_imm16<std::uint16_t>(imm);
                    }

                    break;
                }
            case imm_mode::no_64:
                {
                    if (is_signed) {
                        emit_imm32(imm);
                    }
                    else {
                        emit_imm32<std::uint32_t>(imm);
                    }

                    break;
                }
            case imm_mode::normal:
                {
                    if (is_signed) {
                        emit_imm64(imm);
                    }
                    else {
                        emit_imm64<std::uint64_t>(imm);
                    }

                    break;
                }
            }
        }

        // we don't need 64 bit extensions here for normal rax-rdi
        void emit_reg(const opcode& op8, const opcode& op, const grp& r) {
            if (REG_RANGE(r, r8b, r15b) || REG_RANGE(r, al, dil)) {
                if (REG_RANGE(r, r8b, r15b)) {
                    push_byte(rex_rb);
                }
                else if (REG_RANGE(r, r8b, r15b)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op8) + static_cast<std::uint8_t>(rebase_register(r)));
            }
            else if (REG_RANGE(r, r8w, r15w) || REG_RANGE(r, ax, di)) {
                push_byte(other_prefix::operand_size_override); // 16-bit operand size

                if (REG_RANGE(r, r8w, r15w)) {
                    push_byte(rex_rb);
                }
                else if (REG_RANGE(r, r8w, r15w)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
            }
            else if (REG_RANGE(r, r8d, r15d) || REG_RANGE(r, eax, edi)) {
                if (REG_RANGE(r, r8d, r15d)) {
                    push_byte(rex_rb);
                }
                else if (REG_RANGE(r, r8d, r15d)) {
                    push_byte(rex_b);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
            }
            else if (REG_RANGE(r, r8, r15) || REG_RANGE(r, rax, rdi)) {
                if (REG_RANGE(r, r8, r15)) {
                    push_byte(rex_wrb);
                }
                else if (REG_RANGE(r, r8, r15)) {
                    push_byte(rex_wb);
                }

                push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
            }
        }

        void emit_short_jump(const std::uint8_t opcode, const std::uint8_t target_address) {
            const std::uint8_t current_address = get_code().size();
            const std::uint8_t rel8 = target_address - (current_address + 2);

            push_byte(opcode);
            push_byte(rel8);
        }

        void emit_near_jump(const std::uint8_t opcode, const std::uint32_t target_address) {
            const std::uint32_t current_address = get_code().size();
            const std::int32_t rel32 = target_address - (current_address + 6);

            push_bytes({k2ByteOpcodePrefix, opcode});
            emit_imm32<std::uint32_t>(rel32);
        }

        void emit_long_jump(const std::uint8_t opcode, const std::uint32_t target_address) {
            const std::uint32_t current_address = get_code().size();
            const std::int32_t rel32 = target_address - (current_address + 5);

            push_bytes({opcode});
            emit_imm32<std::uint32_t>(rel32);
        }

        void emit_setX_reg8(const opcode_2b& op, REG_ARG) { // remove the t ;)
            emit_reg_to_reg(static_cast<opcode>(op), static_cast<opcode>(op), _reg, static_cast<grp>(0), true);
        }

        void emit_setX_mem8(const opcode_2b& op, MEM_ARG) { emit_reg_to_mem(static_cast<opcode>(op), static_cast<opcode>(op), m, static_cast<grp>(0), true); }

        template <typename T, typename T2>
        static void assert_imm_size(const T2&) {
            if (sizeof(T2) == sizeof(T)) {
                throw std::invalid_argument("Immediate value can not be of size (" + std::to_string(sizeof(T)) + " bytes)");
            }
        }

        template <typename T>
        bool imm_fits(std::int64_t imm) {
            return imm >= (std::numeric_limits<T>::min)() && imm <= (std::numeric_limits<T>::max)();
        }

    public:
        bool debug; // this has to be like this :(

        x86_64_writer(bool debug = false) : writer(), debug(debug) {}

        // tbf i should use macros for this, but i can do that another time
        void emit_add(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, base, dest); }

        void emit_add(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::ADD_rm8_r8, opcode::ADD_rm16_to_64_r16_to_64, dest, base); }

        void emit_add(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, base, dest);
            // we can just use the same function apparently...?
        }

        void emit_or(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::OR_r8_rm8, opcode::OR_r16_to_64_rm16_to_64, base, dest); }

        void emit_or(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::OR_rm8_r8, opcode::OR_rm16_to_64_r16_to_64, dest, base); }

        void emit_or(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::OR_r8_rm8, opcode::OR_r16_to_64_rm16_to_64, base, dest); }

        void emit_adc(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::ADC_r8_rm8, opcode::ADC_r16_to_64_rm16_to_64, base, dest); }

        void emit_adc(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::ADC_rm8_r8, opcode::ADC_rm16_to_64_r16_to_64, dest, base); }

        void emit_adc(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::ADC_r8_rm8, opcode::ADC_r16_to_64_rm16_to_64, base, dest); }

        void emit_sbb(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::SBB_r8_rm8, opcode::SBB_r16_to_64_rm16_to_64, base, dest); }

        void emit_sbb(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::SBB_rm8_r8, opcode::SBB_rm16_to_64_r16_to_64, dest, base); }

        void emit_sbb(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::SBB_r8_rm8, opcode::SBB_r16_to_64_rm16_to_64, base, dest); }

        void emit_and(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::AND_r8_rm8, opcode::AND_r16_to_64_rm16_to_64, base, dest); }

        void emit_and(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::AND_rm8_r8, opcode::AND_rm16_to_64_r16_to_64, dest, base); }

        void emit_and(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::AND_r8_rm8, opcode::AND_r16_to_64_rm16_to_64, base, dest);
            // we can just use the same function apparently...?
        }

        void emit_sub(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::SUB_r8_rm8, opcode::SUB_r16_to_64_rm16_to_64, base, dest); }

        void emit_sub(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::SUB_rm8_r8, opcode::SUB_rm16_to_64_r16_to_64, dest, base); }

        void emit_sub(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::SUB_r8_rm8, opcode::SUB_r16_to_64_rm16_to_64, base, dest);
            // we can just use the same function apparently...?
        }

        void emit_xor(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::XOR_r8_rm8, opcode::XOR_r16_to_64_rm16_to_64, base, dest); }

        void emit_xor(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::XOR_rm8_r8, opcode::XOR_rm16_to_64_r16_to_64, dest, base); }

        void emit_xor(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::XOR_r8_rm8, opcode::XOR_r16_to_64_rm16_to_64, base, dest);
            // we can just use the same function apparently...?
        }

        void emit_cmp(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::CMP_r8_rm8, opcode::CMP_r16_to_64_rm16_to_64, base, dest); }

        void emit_cmp(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::CMP_rm8_r8, opcode::CMP_rm16_to_64_r16_to_64, dest, base); }

        void emit_cmp(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::CMP_r8_rm8, opcode::CMP_r16_to_64_rm16_to_64, base, dest);
            // we can just use the same function apparently...?
        }

        void emit_push(REG_ARG) { emit_reg(opcode::PUSH_r64_or_16, opcode::PUSH_r64_or_16, _reg); }

        void emit_pop(REG_ARG) { emit_reg(opcode::POP_r64_or_16, opcode::POP_r64_or_16, _reg); }

        void emit_push(SIGNED_IMM_ARG) {
            assert_imm_size<std::int64_t>(imm);

            if (imm_fits<std::int8_t>(imm)) {
                push_byte(opcode::PUSH_imm8);
                emit_imm8(imm);
            }
            else if (imm_fits<std::int16_t>(imm)) {
                push_byte(other_prefix::operand_size_override);
                push_byte(opcode::PUSH_imm16_or_32);
                emit_imm16(imm);
            }
            else {
                push_byte(opcode::PUSH_imm16_or_32);
                emit_imm32(imm);
            }
        }

        void emit_push(UNSIGNED_IMM_ARG) {
            assert_imm_size<std::uint64_t>(imm);

            if (imm_fits<std::uint8_t>(imm)) {
                push_byte(opcode::PUSH_imm8);
                emit_imm8(imm);
            }
            else if (imm_fits<std::uint16_t>(imm)) {
                push_byte(other_prefix::operand_size_override);
                push_byte(opcode::PUSH_imm16_or_32);
                emit_imm16(imm);
            }
            else {
                push_byte(opcode::PUSH_imm16_or_32);
                emit_imm32(imm);
            }
        }

        void emit_imul(const IsGrp auto& dest, const IsGrp auto& base, const IsSignedImm auto& imm) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), true, imm_mode::no_64);
        }

        void emit_imul(const IsGrp auto& dest, const IsMem auto& base, const IsSignedImm auto& imm) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), true, imm_mode::no_64);
        }

        void emit_imul(const IsGrp auto& dest, const IsGrp auto& base, const IsUnsignedImm auto& imm) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), false, imm_mode::no_64);
        }

        void emit_imul(const IsGrp auto& dest, const IsMem auto& base, const IsUnsignedImm auto& imm) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), false, imm_mode::no_64);
        }

        void emit_jo_short(REL8_ARG) { emit_short_jump(opcode::JO_rel8, target_address); }

        void emit_jno_short(REL8_ARG) { emit_short_jump(opcode::JNO_rel8, target_address); }

        void emit_jb_short(REL8_ARG) { emit_short_jump(opcode::JB_rel8, target_address); }

        void emit_jnb_short(REL8_ARG) { emit_short_jump(opcode::JNB_rel8, target_address); }

        void emit_jz_short(REL8_ARG) { emit_short_jump(opcode::JZ_rel8, target_address); }

        void emit_jnz_short(REL8_ARG) { emit_short_jump(opcode::JNZ_rel8, target_address); }

        void emit_jbe_short(REL8_ARG) { emit_short_jump(opcode::JBE_rel8, target_address); }

        void emit_jnbe_short(REL8_ARG) { emit_short_jump(opcode::JNBE_rel8, target_address); }

        void emit_js_short(REL8_ARG) { emit_short_jump(opcode::JS_rel8, target_address); }

        void emit_jns_short(REL8_ARG) { emit_short_jump(opcode::JNS_rel8, target_address); }

        void emit_jp_short(REL8_ARG) { emit_short_jump(opcode::JP_rel8, target_address); }

        void emit_jnp_short(REL8_ARG) { emit_short_jump(opcode::JNP_rel8, target_address); }

        void emit_jl_short(REL8_ARG) { emit_short_jump(opcode::JL_rel8, target_address); }

        void emit_jnl_short(REL8_ARG) { emit_short_jump(opcode::JNL_rel8, target_address); }

        void emit_jle_short(REL8_ARG) { emit_short_jump(opcode::JLE_rel8, target_address); }

        void emit_jnle_short(REL8_ARG) { emit_short_jump(opcode::JNLE_rel8, target_address); }

        void emit_add(SIGNED_IMM_TO_REG_ARG) {
            // for these, the number in rm_field is the operation, 0 (add), 1 (or) etc.
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), true, imm_mode::no_64);
        }

        void emit_add(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false, imm_mode::no_64);
        }

        void emit_add(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), true, imm_mode::no_64);
        }

        void emit_add(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false, imm_mode::no_64);
        }

        void emit_or(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), true, imm_mode::no_64);
        }

        void emit_or(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false, imm_mode::no_64);
        }

        void emit_or(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), true, imm_mode::no_64);
        }

        void emit_or(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false, imm_mode::no_64);
        }

        void emit_adc(SIGNED_IMM_TO_REG_ARG) {
            // not gonna lie, the opcode numbers are the same, i cba to change them
            // right now, will do them later
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), true, imm_mode::no_64);
        }

        void emit_adc(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), false, imm_mode::no_64);
        }

        void emit_adc(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), true, imm_mode::no_64);
        }

        void emit_adc(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), false, imm_mode::no_64);
        }

        void emit_sbb(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), true, imm_mode::no_64);
        }

        void emit_sbb(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), false, imm_mode::no_64);
        }

        void emit_sbb(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), true, imm_mode::no_64);
        }

        void emit_sbb(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), false, imm_mode::no_64);
        }

        void emit_and(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), true, imm_mode::no_64);
        }

        void emit_and(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), false, imm_mode::no_64);
        }

        void emit_and(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), true, imm_mode::no_64);
        }

        void emit_and(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), false, imm_mode::no_64);
        }

        void emit_sub(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), true, imm_mode::no_64);
        }

        void emit_sub(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), false, imm_mode::no_64);
        }

        void emit_sub(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), true, imm_mode::no_64);
        }

        void emit_sub(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), false, imm_mode::no_64);
        }

        void emit_xor(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), true, imm_mode::no_64);
        }

        void emit_xor(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), false, imm_mode::no_64);
        }

        void emit_xor(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), true, imm_mode::no_64);
        }

        void emit_xor(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), false, imm_mode::no_64);
        }

        void emit_cmp(SIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), true, imm_mode::no_64);
        }

        void emit_cmp(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), false, imm_mode::no_64);
        }

        void emit_cmp(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), true, imm_mode::no_64);
        }

        void emit_cmp(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), false, imm_mode::no_64);
        }

        void emit_test(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_r16_to_64, dest, base); }

        void emit_test(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_r16_to_64, dest, base); }

        void emit_xchg(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::XCHG_r8_rm8, opcode::XCHG_r16_to_64_rm16_to_64, dest, base); }

        void emit_xchg(MEM_TO_REG_ARG) {
            emit_reg_to_mem(opcode::XCHG_r8_rm8, opcode::XCHG_r16_to_64_rm16_to_64, base,
                            dest); // opposite still
        }

        void emit_mov(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::MOV_rm8_r8, opcode::MOV_rm16_to_64_r16_to_64, dest, base); }

        void emit_mov(REG_TO_MEM_ARG) { emit_reg_to_mem(opcode::MOV_rm8_r8, opcode::MOV_rm16_to_64_r16_to_64, dest, base); }

        void emit_mov(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::MOV_r8_rm8, opcode::MOV_r16_to_64_rm16_to_64, base, dest); }

        /*
            quick note, this library does not support direct memory addresses, you
           have to load it into a register first then use `operation operand,
           [register]` for example

            instead of:

            `operation operand, [0xDEADBEEF]` <- won't work

            use:

            `mov register, 0xDEADBEEF`
            `operation operand, [register]`

            from what i know, this is specific with LEA and perhaps CALL, but its fine
           i guess, we can't really call with 64 bit addresses anyways?
        */
        void emit_lea(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::LEA_r16_to_64_mem, opcode::LEA_r16_to_64_mem, base, dest); }

        void emit_pop(MEM_ARG) { emit_reg_to_mem(opcode::POP_rm16_to_64, opcode::POP_rm16_to_64, m, static_cast<grp>(0)); }

        void emit_nop() { push_byte(opcode::NOP); }

        void emit_mov(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm_basic(opcode::MOV_r8_imm8, opcode::MOV_r16_to_64_imm16_to_64, dest, imm); }

        void emit_mov(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm_basic(opcode::MOV_r8_imm8, opcode::MOV_r16_to_64_imm16_to_64, dest, imm, false); }

        void emit_rol(SIGNED_IMM_TO_REG_ARG) { // no check for size here, just goes to
                                               // imm8 static cast
            emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(0), true, imm_mode::do_8);
        }

        void emit_rol(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(0), false, imm_mode::do_8); }

        void emit_ror(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(1), true, imm_mode::do_8); }

        void emit_ror(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(1), false, imm_mode::do_8); }

        void emit_rcl(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(2), true, imm_mode::do_8); }

        void emit_rcl(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(2), false, imm_mode::do_8); }

        void emit_rcr(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(3), true, imm_mode::do_8); }

        void emit_rcr(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(3), false, imm_mode::do_8); }

        /* do note that SAL and SHL are the SAME logical operation */
        void emit_shl(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(4), true, imm_mode::do_8); }

        /* do note that SAL and SHL are the SAME logical operation */
        void emit_shl(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(4), false, imm_mode::do_8); }

        void emit_shr(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(5), true, imm_mode::do_8); }

        void emit_shr(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(5), false, imm_mode::do_8); }

        /* do note that SAL and SHL are the SAME logical operation */
        void emit_sal(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(6), true, imm_mode::do_8); }

        /* do note that SAL and SHL are the SAME logical operation */
        void emit_sal(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(6), false, imm_mode::do_8); }

        void emit_sar(SIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::int8_t>(imm), static_cast<rm_field>(7), true, imm_mode::do_8); }

        void emit_sar(UNSIGNED_IMM_TO_REG_ARG) { emit_reg_imm(opcode::ROL_rm8_imm8, opcode::ROL_rm16_to_64_imm8, dest, static_cast<std::uint8_t>(imm), static_cast<rm_field>(7), false, imm_mode::do_8); }

        /* return near */
        void emit_retn() { push_byte(opcode::RETN); }

        inline void emit_ret() { emit_retn(); }

        /* return far */
        void emit_retf() { push_byte(opcode::RETF); }

        void emit_mov(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);

            if (imm_fits<std::int8_t>(imm)) {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), true, imm_mode::do_8);
            }
            else if (imm_fits<std::int16_t>(imm)) {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), true, imm_mode::do_16);
            }
            else {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), true, imm_mode::no_64);
            }
        }

        void emit_mov(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);

            if (imm_fits<std::uint8_t>(imm)) {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false, imm_mode::do_8);
            }
            else if (imm_fits<std::int16_t>(imm)) {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false, imm_mode::do_16);
            }
            else {
                emit_mem_imm(opcode::MOV_rm8_imm8, opcode::MOV_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false, imm_mode::no_64);
            }
        }

        void emit_test(SIGNED_IMM_TO_REG_ARG) { // i think the rm_field could also be 0 for TEST
            assert_imm_size<std::int64_t>(imm);
            emit_reg_imm(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), true, imm_mode::no_64);
        }

        void emit_test(UNSIGNED_IMM_TO_REG_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_reg_imm(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false, imm_mode::no_64);
        }

        void emit_test(SIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::int64_t>(imm);
            emit_mem_imm(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), true, imm_mode::no_64);
        }

        void emit_test(UNSIGNED_IMM_TO_MEM_ARG) {
            assert_imm_size<std::uint64_t>(imm);
            emit_mem_imm(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false, imm_mode::no_64);
        }

        void emit_not(REG_ARG) {
            emit_reg_to_reg(opcode::NOT_rm8, opcode::NOT_rm16_to_64, _reg, static_cast<grp>(2));
            // 2 is which operation
        }

        void emit_not(MEM_ARG) { emit_reg_to_mem(opcode::NOT_rm8, opcode::NOT_rm16_to_64, m, static_cast<grp>(2)); }

        void emit_neg(REG_ARG) {
            emit_reg_to_reg(opcode::NOT_rm8, opcode::NOT_rm16_to_64, _reg, static_cast<grp>(3));
            // 2 is which operation
        }

        void emit_neg(MEM_ARG) { emit_reg_to_mem(opcode::NOT_rm8, opcode::NOT_rm16_to_64, m, static_cast<grp>(3)); }

        void emit_mul(REG_ARG) { emit_reg_to_reg(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, _reg, static_cast<grp>(4)); }

        void emit_mul(MEM_ARG) { emit_reg_to_mem(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, m, static_cast<grp>(4)); }

        void emit_imul(REG_ARG) { emit_reg_to_reg(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, _reg, static_cast<grp>(5)); }

        void emit_imul(MEM_ARG) { emit_reg_to_mem(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, m, static_cast<grp>(5)); }

        void emit_div(REG_ARG) { emit_reg_to_reg(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, _reg, static_cast<grp>(6)); }

        void emit_div(MEM_ARG) { emit_reg_to_mem(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, m, static_cast<grp>(6)); }

        void emit_idiv(REG_ARG) { emit_reg_to_reg(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, _reg, static_cast<grp>(7)); }

        void emit_idiv(MEM_ARG) { emit_reg_to_mem(opcode::MUL_AX_AL_rm8, opcode::MUL_rDX_rAX_rm16_to_64, m, static_cast<grp>(7)); }

        void emit_inc(REG_ARG) { emit_reg_to_reg(opcode::INC_rm8, opcode::INC_rm16_to_64, _reg, static_cast<grp>(0)); }

        void emit_inc(MEM_ARG) { emit_reg_to_mem(opcode::INC_rm8, opcode::INC_rm16_to_64, m, static_cast<grp>(0)); }

        void emit_dec(REG_ARG) { emit_reg_to_reg(opcode::DEC_rm8, opcode::INC_rm16_to_64, _reg, static_cast<grp>(1)); }

        void emit_dec(MEM_ARG) { emit_reg_to_mem(opcode::DEC_rm8, opcode::INC_rm16_to_64, m, static_cast<grp>(1)); }

        void emit_call(REG_ARG) { // we will not support 64
            emit_reg_to_reg(opcode::CALL_rm16_or_32, opcode::CALL_rm16_or_32, _reg, static_cast<grp>(2));
        }

        void emit_call(MEM_ARG) { emit_reg_to_mem(opcode::CALL_rm16_or_32, opcode::CALL_rm16_or_32, m, static_cast<grp>(2)); }

        void emit_jmp(REG_ARG) { emit_reg_to_reg(opcode::JMP_rm16_or_32, opcode::JMP_rm16_or_32, _reg, static_cast<grp>(4)); }

        void emit_jmp(MEM_ARG) { emit_reg_to_mem(opcode::JMP_rm16_or_32, opcode::JMP_rm16_or_32, m, static_cast<grp>(4)); }

        void emit_push(MEM_ARG) { emit_reg_to_mem(opcode::PUSH_rm16_or_32, opcode::PUSH_rm16_or_32, m, static_cast<grp>(6)); }

        void emit_syscall() {
            push_byte(k2ByteOpcodePrefix);
            push_byte(opcode_2b::SYSCALL);
        }

        void emit_jo_near(REL32_ARG) { emit_near_jump(opcode_2b::JO_rel32, target_address); }

        void emit_jno_near(REL32_ARG) { emit_near_jump(opcode_2b::JNO_rel32, target_address); }

        void emit_jb_near(REL32_ARG) { emit_near_jump(opcode_2b::JB_rel32, target_address); }

        void emit_jnb_near(REL32_ARG) { emit_near_jump(opcode_2b::JNB_rel32, target_address); }

        void emit_jz_near(REL32_ARG) { emit_near_jump(opcode_2b::JZ_rel32, target_address); }

        void emit_jnz_near(REL32_ARG) { emit_near_jump(opcode_2b::JNZ_rel32, target_address); }

        void emit_jbe_near(REL32_ARG) { emit_near_jump(opcode_2b::JBE_rel32, target_address); }

        void emit_jnbe_near(REL32_ARG) { emit_near_jump(opcode_2b::JNBE_rel32, target_address); }

        void emit_js_near(REL32_ARG) { emit_near_jump(opcode_2b::JS_rel32, target_address); }

        void emit_jns_near(REL32_ARG) { emit_near_jump(opcode_2b::JNS_rel32, target_address); }

        void emit_jp_near(REL32_ARG) { emit_near_jump(opcode_2b::JP_rel32, target_address); }

        void emit_jnp_near(REL32_ARG) { emit_near_jump(opcode_2b::JNP_rel32, target_address); }

        void emit_jl_near(REL32_ARG) { emit_near_jump(opcode_2b::JL_rel32, target_address); }

        void emit_jnl_near(REL32_ARG) { emit_near_jump(opcode_2b::JNL_rel32, target_address); }

        void emit_jle_near(REL32_ARG) { emit_near_jump(opcode_2b::JLE_rel32, target_address); }

        void emit_jnle_near(REL32_ARG) { emit_near_jump(opcode_2b::JNLE_rel32, target_address); }

        void emit_call(REL32_ARG) { emit_long_jump(opcode::CALL_rel32, target_address); }

        void emit_jmp_near(REL32_ARG) { emit_long_jump(opcode::JMP_rel32, target_address); }

        void emit_jmp_short(REL8_ARG) { emit_short_jump(opcode::JMP_rel8, target_address); }

        void emit_function_prologue() {
            emit_push(rbp);
            emit_mov(rbp, rsp);
        }

        void emit_function_epilogue() {
            emit_mov(rsp, rbp);
            emit_pop(rbp);
        }

        void emit_seto(REG_ARG) { emit_setX_reg8(opcode_2b::SETO_rm8, _reg); }

        void emit_seto(MEM_ARG) { emit_setX_mem8(opcode_2b::SETO_rm8, m); }

        void emit_setno(REG_ARG) { emit_setX_reg8(opcode_2b::SETNO_rm8, _reg); }

        void emit_setno(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNO_rm8, m); }

        void emit_setb(REG_ARG) { emit_setX_reg8(opcode_2b::SETB_rm8, _reg); }

        void emit_setb(MEM_ARG) { emit_setX_mem8(opcode_2b::SETB_rm8, m); }

        void emit_setnb(REG_ARG) { emit_setX_reg8(opcode_2b::SETNB_rm8, _reg); }

        void emit_setnb(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNB_rm8, m); }

        void emit_setz(REG_ARG) { emit_setX_reg8(opcode_2b::SETZ_rm8, _reg); }

        void emit_setz(MEM_ARG) { emit_setX_mem8(opcode_2b::SETZ_rm8, m); }

        void emit_setnz(REG_ARG) { emit_setX_reg8(opcode_2b::SETNZ_rm8, _reg); }

        void emit_setnz(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNZ_rm8, m); }

        void emit_setbe(REG_ARG) { emit_setX_reg8(opcode_2b::SETBE_rm8, _reg); }

        void emit_setbe(MEM_ARG) { emit_setX_mem8(opcode_2b::SETBE_rm8, m); }

        void emit_setnbe(REG_ARG) { emit_setX_reg8(opcode_2b::SETNBE_rm8, _reg); }

        void emit_setnbe(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNBE_rm8, m); }

        void emit_sets(REG_ARG) { emit_setX_reg8(opcode_2b::SETS_rm8, _reg); }

        void emit_sets(MEM_ARG) { emit_setX_mem8(opcode_2b::SETS_rm8, m); }

        void emit_setns(REG_ARG) { emit_setX_reg8(opcode_2b::SETNS_rm8, _reg); }

        void emit_setns(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNS_rm8, m); }

        void emit_setp(REG_ARG) { emit_setX_reg8(opcode_2b::SETP_rm8, _reg); }

        void emit_setp(MEM_ARG) { emit_setX_mem8(opcode_2b::SETP_rm8, m); }

        void emit_setnp(REG_ARG) { emit_setX_reg8(opcode_2b::SETNP_rm8, _reg); }

        void emit_setnp(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNP_rm8, m); }

        void emit_setl(REG_ARG) { emit_setX_reg8(opcode_2b::SETL_rm8, _reg); }

        void emit_setl(MEM_ARG) { emit_setX_mem8(opcode_2b::SETL_rm8, m); }

        void emit_setnl(REG_ARG) { emit_setX_reg8(opcode_2b::SETNL_rm8, _reg); }

        void emit_setnl(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNL_rm8, m); }

        void emit_setle(REG_ARG) { emit_setX_reg8(opcode_2b::SETLE_rm8, _reg); }

        void emit_setle(MEM_ARG) { emit_setX_mem8(opcode_2b::SETLE_rm8, m); }

        void emit_setnle(REG_ARG) { emit_setX_reg8(opcode_2b::SETNLE_rm8, _reg); }

        void emit_setnle(MEM_ARG) { emit_setX_mem8(opcode_2b::SETNLE_rm8, m); }

        /* for these ones, the other thing to shift by is in cl */
        void emit_shl(REG_ARG) { emit_reg_to_reg(opcode::SHR_rm8_CL, opcode::SHR_rm16_to_64_CL, _reg, static_cast<grp>(4), false); }

        void emit_shr(REG_ARG) { emit_reg_to_reg(opcode::SHR_rm8_CL, opcode::SHR_rm16_to_64_CL, _reg, static_cast<grp>(5), false); }

        void emit_sar(REG_ARG) { emit_reg_to_reg(opcode::SAR_rm8_CL, opcode::SAR_rm16_to_64_CL, _reg, static_cast<grp>(7), false); }

        /* only 8/16 bits ZERO EXTEND */
        void emit_movzx(REG_TO_REG_ARG, bool is_8 = true) {
            if (is_8) {
                emit_reg_to_reg(static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm8), static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm8), base, dest, true);
            }
            else {
                emit_reg_to_reg(static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm16), static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm16), base, dest, true);
            }
        }

        /* only 8/16 bits ZERO EXTEND */
        void emit_movzx(MEM_TO_REG_ARG, bool is_8 = true) {
            if (is_8) {
                emit_reg_to_mem(static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm8), static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm8), base, dest, true);
            }
            else {
                emit_reg_to_mem(static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm16), static_cast<opcode>(opcode_2b::MOVZX_r16_to_64_rm16), base, dest, true);
            }
        }

        /* only 8/16 bits */
        void emit_movsx(REG_TO_REG_ARG, bool is_8 = true) {
            if (is_8) {
                emit_reg_to_reg(static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm8), static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm8), base, dest, true);
            }
            else {
                emit_reg_to_reg(static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm16), static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm16), base, dest, true);
            }
        }

        /* only 8/16 bits */
        void emit_movsx(MEM_TO_REG_ARG, bool is_8 = true) {
            if (is_8) {
                emit_reg_to_mem(static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm8), static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm8), base, dest, true);
            }
            else {
                emit_reg_to_mem(static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm16), static_cast<opcode>(opcode_2b::MOVSX_r16_to_64_rm16), base, dest, true);
            }
        }

        /* 32 to 64 */
        void emit_movsxd(REG_TO_REG_ARG) { emit_reg_to_reg(opcode::MOVSXD_r64_or_32_rm32, opcode::MOVSXD_r64_or_32_rm32, base, dest); }

        /* 32 to 64 */
        void emit_movsxd(MEM_TO_REG_ARG) { emit_reg_to_mem(opcode::MOVSXD_r64_or_32_rm32, opcode::MOVSXD_r64_or_32_rm32, base, dest); }

        // move value into sse registers from GRP (single or double precision)
        void emit_movq(REG_TO_SIMD128_ARG) { emit_reg_to_simd128(float_opcode_2b::MOVQ_xmm_to_rm32_to_64, base, dest, {operand_size_override}); }

        // move value into sse registers from GRP (single or double precision)
        void emit_movd(REG_TO_SIMD128_ARG) { emit_reg_to_simd128(float_opcode_2b::MOVD_xmm_to_rm32_to_64, base, dest, {operand_size_override}); }

        // add scalar packed double precision
        void emit_addsd(SIMD128_TO_SIMD128_ARG) { push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(ADDSD_xmm_to_xmm_or_m64), modrm(mod_field::register_direct, dest, base)}); }

        // add scalar packed single precision
        void emit_addss(SIMD128_TO_SIMD128_ARG) { push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(ADDSS_xmm_to_xmm_or_m32), modrm(mod_field::register_direct, dest, base)}); }

        void emit_subsd(SIMD128_TO_SIMD128_ARG) { push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(SUBSD_xmm_to_xmm_or_m64), modrm(mod_field::register_direct, dest, base)}); }

        void emit_subss(SIMD128_TO_SIMD128_ARG) { push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(SUBSS_xmm_to_xmm_or_m32), modrm(mod_field::register_direct, dest, base)}); }

        void emit_mulsd(SIMD128_TO_SIMD128_ARG) { push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(MULSD_xmm_to_xmm_or_m64), modrm(mod_field::register_direct, dest, base)}); }

        void emit_mulss(SIMD128_TO_SIMD128_ARG) { push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(MULSS_xmm_to_xmm_or_m32), modrm(mod_field::register_direct, dest, base)}); }

        void emit_divsd(SIMD128_TO_SIMD128_ARG) { push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(DIVSD_xmm_to_xmm_or_m64), modrm(mod_field::register_direct, dest, base)}); }

        void emit_divss(SIMD128_TO_SIMD128_ARG) { push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(DIVSS_xmm_to_xmm_or_m32), modrm(mod_field::register_direct, dest, base)}); }

        void emit_cvttsd2si(const grp& dst, const simd128& src) {
            const auto src_val = static_cast<std::uint8_t>(src) & 7;
            const auto dst_val = static_cast<std::uint8_t>(rebase_register(dst));
            std::uint8_t rex = static_cast<std::uint8_t>(rex_w);
            if (REG_RANGE(dst, r8, r15))
                rex |= 0x04; // REX.R extends reg field
            if (static_cast<std::uint8_t>(src) >= 8)
                rex |= 0x01; // REX.B extends xmm8-15
            push_bytes({kREPNE_Prefix, rex, k2ByteOpcodePrefix, static_cast<std::uint8_t>(CVTTSD2SI_xmm_or_m64_to_r32_to_64), static_cast<std::uint8_t>((0b11 << 6) | (dst_val << 3) | src_val)});
        }

        void emit_cvtsi2sd(const simd128& dst, const grp& src) {
            const auto dst_val = static_cast<std::uint8_t>(dst) & 7;
            const auto src_val = static_cast<std::uint8_t>(rebase_register(src));
            std::uint8_t rex = static_cast<std::uint8_t>(rex_w);
            if (static_cast<std::uint8_t>(dst) >= 8)
                rex |= 0x04; // REX.R extends xmm8-15 in reg field
            if (REG_RANGE(src, r8, r15))
                rex |= 0x01; // REX.B extends r8-r15 in r/m field
            push_bytes({kREPNE_Prefix, rex, k2ByteOpcodePrefix, static_cast<std::uint8_t>(CVTSI2SD_rm32_to_64_to_xmm), static_cast<std::uint8_t>((0b11 << 6) | (dst_val << 3) | src_val)});
        }

        void emit_cvttss2si(const grp& dst, const simd128& src) {
            const auto src_val = static_cast<std::uint8_t>(src) & 7;
            const auto dst_val = static_cast<std::uint8_t>(rebase_register(dst));
            std::uint8_t rex = static_cast<std::uint8_t>(rex_w);
            if (REG_RANGE(dst, r8, r15))
                rex |= 0x04; // REX.R extends reg field
            if (static_cast<std::uint8_t>(src) >= 8)
                rex |= 0x01; // REX.B extends xmm8-15
            push_bytes({kREP_Prefix, rex, k2ByteOpcodePrefix, static_cast<std::uint8_t>(CVTTSD2SI_xmm_or_m64_to_r32_to_64), static_cast<std::uint8_t>((0b11 << 6) | (dst_val << 3) | src_val)});
        }

        void emit_cvtsi2ss(const simd128& dst, const grp& src) {
            const auto dst_val = static_cast<std::uint8_t>(dst) & 7;
            const auto src_val = static_cast<std::uint8_t>(rebase_register(src));
            std::uint8_t rex = static_cast<std::uint8_t>(rex_w);
            if (static_cast<std::uint8_t>(dst) >= 8)
                rex |= 0x04; // REX.R extends xmm8-15 in reg field
            if (REG_RANGE(src, r8, r15))
                rex |= 0x01; // REX.B extends r8-r15 in r/m field
            push_bytes({kREP_Prefix, rex, k2ByteOpcodePrefix, static_cast<std::uint8_t>(CVTSI2SD_rm32_to_64_to_xmm), static_cast<std::uint8_t>((0b11 << 6) | (dst_val << 3) | src_val)});
        }

        void emit_cvtss2sd(SIMD128_TO_SIMD128_ARG) { push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(CVTSS2SD_xmm_or_m32_to_xmm), modrm(mod_field::register_direct, dest, base)}); }

        void emit_cvtsd2ss(SIMD128_TO_SIMD128_ARG) { push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(CVTSD2SS_xmm_or_m64_to_xmm), modrm(mod_field::register_direct, dest, base)}); }

        void emit_movss(SIMD128_TO_SIMD128_ARG) {
            simd128 true_base, true_dest;

            true_base = static_cast<simd128>(static_cast<uint8_t>(base & 7));
            true_dest = static_cast<simd128>(static_cast<uint8_t>(dest & 7));

            push_bytes({kREP_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(MOVSD_xmm_or_m64_to_xmm), modrm(mod_field::register_direct, true_dest, true_base)});
        }

        void emit_movsd(SIMD128_TO_SIMD128_ARG) {
            simd128 true_base, true_dest;

            true_base = static_cast<simd128>(static_cast<uint8_t>(base & 7));
            true_dest = static_cast<simd128>(static_cast<uint8_t>(dest & 7));

            push_bytes({kREPNE_Prefix, k2ByteOpcodePrefix, static_cast<uint8_t>(MOVSD_xmm_or_m64_to_xmm), modrm(mod_field::register_direct, true_dest, true_base)});
        }

        void emit_movss(SIMD128_TO_MEM_ARG) { emit_simd128_to_mem(static_cast<opcode>(MOVSS_xmm_to_xmm_or_m32), base, dest, {kREP_Prefix}); }

        void emit_movss(MEM_TO_SIMD128_ARG) { emit_simd128_to_mem(static_cast<opcode>(MOVSS_xmm_or_m32_to_xmm), dest, base, {kREP_Prefix}); }

        void emit_movsd(SIMD128_TO_MEM_ARG) { emit_simd128_to_mem(static_cast<opcode>(MOVSD_xmm_to_xmm_or_m64), base, dest, {kREPNE_Prefix}); }

        void emit_movsd(MEM_TO_SIMD128_ARG) { emit_simd128_to_mem(static_cast<opcode>(MOVSD_xmm_or_m64_to_xmm), dest, base, {kREPNE_Prefix}); }

        void emit_fmod_double(simd128 result, simd128 x, simd128 y, simd128 temp1, simd128 temp2, grp temp_gpr) {
            emit_movsd(temp1, x);
            emit_divsd(temp1, y);

            emit_cvttsd2si(temp_gpr, temp1);
            emit_cvtsi2sd(temp2, temp_gpr);

            emit_mulsd(temp2, y);

            emit_movsd(result, x);
            emit_subsd(result, temp2);
        }

        void emit_fmod_float(simd128 result, simd128 x, simd128 y, simd128 temp1, simd128 temp2, grp temp_gpr) {
            emit_movss(temp1, x);
            emit_divss(temp1, y);

            emit_cvttss2si(temp_gpr, temp1);
            emit_cvtsi2ss(temp2, temp_gpr);

            emit_mulss(temp2, y);

            emit_movss(result, x);
            emit_subss(result, temp2);
        }

        void emit_comiss(SIMD128_TO_SIMD128_ARG) { push_bytes({k2ByteOpcodePrefix, static_cast<uint8_t>(0x2F), modrm(mod_field::register_direct, dest, base)}); }

        void emit_comisd(SIMD128_TO_SIMD128_ARG) { push_bytes({operand_size_override, k2ByteOpcodePrefix, static_cast<uint8_t>(0x2F), modrm(mod_field::register_direct, dest, base)}); }
    };
} // namespace occult::x86_64
