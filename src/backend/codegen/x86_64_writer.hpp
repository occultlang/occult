#include "writer.hpp"
#include "x86_64_defs.hpp"
#include <type_traits>

// rewritten entirety of the x64writer to be more readable and understandable
// will document as i go, more
// based on https://ref.x86asm.net/coder64.html

#define REG_RANGE(to_cmp, start, end) \
    (to_cmp >= start && to_cmp <= end)

#define NOT_STACK_BASE_PTR(dest) \
    (dest != rbp && dest != ebp && dest != bp && dest != bpl)

#define NOT_STACK_PTR(dest) \
    (dest != rsp && dest != esp && dest != sp && dest != spl)

#define IS_STACK_BASE_PTR(dest) \
    (dest == rbp || dest == ebp || dest == bp || dest == bpl)

#define IS_STACK_PTR(dest) \
    (dest == rsp || dest == esp || dest == sp || dest == spl)

namespace occult {
    namespace x86_64 {
        enum class mem_mode : std::uint8_t {
            none,
            disp,
            reg,
            scaled_index,
            abs
        };

        struct mem {
            grp reg;
            std::size_t scale;
            grp index;
            std::int32_t disp;
            mem_mode mode = mem_mode::none;
            mem_mode second_mode = mem_mode::none;

            mem() = delete;

            mem(const grp& reg) 
                : reg(reg) { 
                mode = mem_mode::reg; 
            }

            mem(const grp& reg, const std::int32_t& disp) 
                : reg(reg), disp(disp) { 
                mode = mem_mode::disp; 
            }

            mem(const grp& reg, const grp& index) 
                : reg(reg), scale(0), index(index), disp(0) { 
                mode = mem_mode::scaled_index; 
            }

            mem(const grp& reg, const grp& index, const std::size_t& scale) 
                : reg(reg), scale(scale), index(index), disp(0) { 
                mode = mem_mode::scaled_index; 
            }

            mem(const grp& reg, const grp& index, const std::size_t& scale, const std::int32_t& disp) 
                : reg(reg), scale(scale), index(index), disp(disp) { 
                mode = mem_mode::scaled_index; second_mode = mem_mode::disp; 
            }
        };

        template<typename T>
        concept IsGrp = std::is_enum_v<T> || std::is_base_of_v<grp, T>;

        template<typename T>
        concept IsMem = std::is_base_of_v<mem, T>;

        template<typename T>
        concept IsSignedImm = std::is_integral_v<T> && std::is_signed_v<T>;

        template<typename T>
        concept IsUnsignedImm = std::is_integral_v<T> && std::is_unsigned_v<T>;

        template<typename T>
        concept IsRel8 = std::is_integral_v<T> &&std::is_unsigned_v<T> && sizeof(T) == 1;

#define REG_TO_REG_ARG const IsGrp auto& dest, const IsGrp auto& base
#define REG_TO_MEM_ARG const IsMem auto& dest, const IsGrp auto& base
#define MEM_TO_REG_ARG const IsGrp auto& dest, const IsMem auto& base
#define SIGNED_IMM_TO_REG_ARG const IsGrp auto& dest, const IsSignedImm auto& imm
#define UNSIGNED_IMM_TO_REG_ARG const IsGrp auto& dest, const IsUnsignedImm auto& imm
#define SIGNED_IMM_TO_MEM_ARG const IsMem auto& dest, const IsSignedImm auto& imm
#define UNSIGNED_IMM_TO_MEM_ARG const IsMem auto& dest, const IsUnsignedImm auto& imm
#define REG_ARG const IsGrp auto& reg
#define SIGNED_IMM_ARG const IsSignedImm auto& imm
#define UNSIGNED_IMM_ARG const IsUnsignedImm auto& imm
#define REL8_ARG const IsRel8 auto& target_address

        class x86_64_writer : public writer {  
            template<typename IntType = std::int8_t>
            void emit_imm(IntType imm, int size) {
                for (int i = 0; i < size; ++i) {
                    push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
                }
            }
            
            template<typename IntType = std::int8_t> // template if we want to use unsigned values
            void emit_imm8(IntType imm) {
                emit_imm<IntType>(imm, 1);
            }
            
            template<typename IntType = std::int16_t>
            void emit_imm16(IntType imm) {
                emit_imm<IntType>(imm, 2);
            }
            
            template<typename IntType = std::int32_t>
            void emit_imm32(IntType imm) {
                emit_imm<IntType>(imm, 4);
            }
            
            template<typename IntType = std::int64_t>
            void emit_imm64(IntType imm) {
                emit_imm<IntType>(imm, 8);
            }

            void emit_reg_to_reg(const opcode& op8, const opcode& op, const grp& dest, const grp& base) { 
                if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, spl) || 
                    REG_RANGE(base, r8b, r15b) || REG_RANGE(base, al, spl)) {
                    if (REG_RANGE(dest, r8b, r15b) && REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(dest, r8b, r15b)) {
                        push_byte(rex_b);
                    }
                    else if (REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_r);
                    }

                    push_byte(op8);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, sp) || 
                         REG_RANGE(base, r8w, r15w) || REG_RANGE(base, ax, sp)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size

                    if (REG_RANGE(dest, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(rex_b);
                    }
                    else if (REG_RANGE(base, r8w, r15w)) {
                        push_byte(rex_r);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, esp) || 
                         REG_RANGE(base, r8d, r15d) || REG_RANGE(base, eax, esp)) {
                    if (REG_RANGE(dest, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex_b);
                    }
                    else if (REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_r);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rsp) || 
                         REG_RANGE(base, r8, r15) || REG_RANGE(base, rax, rsp)) {
                    if (REG_RANGE(dest, r8, r15) && REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wrb);
                    }
                    else if (REG_RANGE(dest, r8, r15)) {
                        push_byte(rex_wb);
                    }
                    else if (REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wr);
                    }
                    else {
                        push_byte(rex_w);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
            }

            void check_reg_r2m(const opcode& op8, const opcode& op, const mem& dest, const grp& base, bool is_rip = false) {
                if (REG_RANGE(dest.reg, r8b, r15b) && REG_RANGE(base, r8b, r15b) && !is_rip) {
                    push_byte(rex_rb);
                    push_byte(op8);
                }
                else if (REG_RANGE(dest.reg, r8b, r15b)) {
                    push_byte(rex_b);
                    push_byte(op8);
                }
                else if (REG_RANGE(base, r8b, r15b) && !is_rip) {
                    push_byte(rex_r);
                    push_byte(op8);
                }
                else if (REG_RANGE(dest.reg, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size
                    push_byte(rex_rb);
                    push_byte(op);
                }
                else if (REG_RANGE(dest.reg, r8w, r15w)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size
                    push_byte(rex_b);
                    push_byte(op);
                }
                else if (REG_RANGE(base, r8w, r15w)&& !is_rip) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size
                    push_byte(rex_r);
                    push_byte(op);
                }
                else if (REG_RANGE(dest.reg, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                    push_byte(rex_rb);
                    push_byte(op);
                }
                else if (REG_RANGE(dest.reg, r8d, r15d)) {
                    push_byte(rex_b);
                    push_byte(op);
                }
                else if (REG_RANGE(base, r8d, r15d) && !is_rip) {
                    push_byte(rex_r);
                    push_byte(op);
                }
                else if (REG_RANGE(dest.reg, r8, r15) && REG_RANGE(base, r8, r15) && !is_rip) {
                    push_byte(rex_wrb);
                    push_byte(op);
                }
                else if (REG_RANGE(dest.reg, r8, r15)) {
                    push_byte(rex_wb);
                    push_byte(op);
                }
                else if (REG_RANGE(base, r8, r15) && !is_rip) {
                    push_byte(rex_wr);
                    push_byte(op);
                }
                else {
                    push_byte(rex_w);
                    push_byte(op);
                }
            }

            void check_stack_reg_r2m(const opcode& op8, const opcode& op, const mem& dest) {
                if (dest.reg == spl) {
                    push_byte(op8);
                }
                else if (dest.reg == sp) {
                    push_byte(other_prefix::operand_size_override);
                    push_byte(op);
                }
                else if (dest.reg == esp) {
                    push_byte(op);
                }
                else if (dest.reg == rsp) {
                    push_byte(rex_w);
                    push_byte(op);
                }
            }

            void emit_reg_to_mem(const opcode& op8, const opcode& op, const mem& dest, const grp& base) { 
                if (dest.reg == rip) { // rip relative
                    check_reg_r2m(op8, op, dest, base, true);
                    push_byte(modrm(mod_field::indirect, rm_field::rip_relative, rebase_register(base)));
                    emit_imm32(dest.disp);
                }
                else if (NOT_STACK_PTR(dest.reg) && NOT_STACK_BASE_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [reg]
                    check_reg_r2m(op8, op, dest, base);
                    push_byte(modrm(mod_field::indirect, rebase_register(dest.reg), rebase_register(base)));
                } 
                else if (IS_STACK_BASE_PTR(dest.reg) && NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [rbp]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), rebase_register(base)));
                    emit_imm8(0);
                }
                else if (NOT_STACK_BASE_PTR(dest.reg) && IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [rsp]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rebase_register(base))); 
                    push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                }
                else if (NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [reg + disp32] (we are always going to do disp32)
                    check_reg_r2m(op8, op, dest, base);
                    push_byte(modrm(mod_field::disp32, rebase_register(dest.reg), rebase_register(base)));
                    emit_imm32(dest.disp);
                }
                else if (IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [rsp + disp32]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rebase_register(base))); 
                    push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }
                else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::none) { // [reg + SIB]
                    check_reg_r2m(op8, op, dest, base);
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rebase_register(base))); 
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                }
                else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::disp) { // [reg + SIB + disp]
                    check_reg_r2m(op8, op, dest, base);
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rebase_register(base))); 
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }
            }

            // rm field indicates which operation is used for things such as (ADD, OR, etc. ) (opcodes are 0x80 for 8bit and 0x81 for higher than 8bit)
            // this can also be used for 0xC7 and 0xC6
            // this is for the opcodes which require "different modes?" 
            template<std::integral T> 
            void emit_reg_imm(const opcode& op8, const opcode& op, const grp& dest, const T& imm, const rm_field& rm, bool is_signed = true, bool do_imm64 = false) {
                if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, spl)) {
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
                else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, sp)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size
                    if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(rex_b);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                    if (is_signed) {
                        emit_imm16(imm);
                    }
                    else {
                        emit_imm16<std::uint16_t>(imm);
                    }
                }
                else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, esp)) {
                    if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex_b);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));

                    if (is_signed) {
                        emit_imm32(imm);
                    }
                    else {
                        emit_imm32<std::uint32_t>(imm);
                    }
                }
                else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rsp) ) {
                    if (REG_RANGE(dest, r8, r15)) {
                        push_byte(rex_wb);
                    }
                    else {
                        push_byte(rex_w);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));
                    
                    if (!do_imm64)
                        if (is_signed) {
                            emit_imm32(imm);
                        }
                        else {
                            emit_imm32<std::uint32_t>(imm);
                        }
                    else {
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
            template<std::integral T> 
            void emit_reg_imm_basic(const opcode& op8, const opcode& op, const grp& dest, const T& imm, bool is_signed = true) { 
                if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, spl)) {
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
                else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, sp)) {
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
                else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, esp)) {
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
                else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rsp) ) {
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

            // believe this is finished, too tired to document, will get to it in the future i guess lmao
            template<std::integral T> 
            void emit_mem_imm(const opcode& op8, const opcode& op, const mem& dest, const T& imm, const rm_field& rm, bool is_signed = true, bool do_imm64 = false) {
                if (dest.reg == rip) { // rip relative
                    throw std::invalid_argument("Can't move immediate to rip-relative address.");
                }
                else if (NOT_STACK_PTR(dest.reg) && NOT_STACK_BASE_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [reg]
                    check_reg_r2m(op8, op, dest, dest.reg);
                    push_byte(modrm(mod_field::indirect, rebase_register(dest.reg), rm));
                } 
                else if (IS_STACK_BASE_PTR(dest.reg) && NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [rbp]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::disp8, rebase_register(dest.reg), rm));
                    emit_imm8(0);
                }
                else if (NOT_STACK_BASE_PTR(dest.reg) && IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::reg) { // [rsp]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rm)); 
                    push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                }
                else if (NOT_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [reg + disp32] (we are always going to do disp32)
                    check_reg_r2m(op8, op, dest, dest.reg);
                    push_byte(modrm(mod_field::disp32, rebase_register(dest.reg), rm));
                    emit_imm32(dest.disp);
                }
                else if (IS_STACK_PTR(dest.reg) && dest.mode == mem_mode::disp) { // [rsp + disp32]
                    check_stack_reg_r2m(op8, op, dest);
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rm)); 
                    push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }
                else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::none) { // [reg + SIB]
                    check_reg_r2m(op8, op, dest, dest.reg);
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rm)); 
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                }
                else if (dest.mode == mem_mode::scaled_index && dest.second_mode == mem_mode::disp) { // [reg + SIB + disp]
                    check_reg_r2m(op8, op, dest, dest.reg);
                    push_byte(modrm(mod_field::disp32, rm_field::indexed, rm)); 
                    push_byte(sib(dest.scale, dest.index, rebase_register(dest.reg)));
                    emit_imm32(dest.disp);
                }

                if (is_signed) {
                    if (do_imm64) {
                        emit_imm64(imm);
                    } else {
                        emit_imm32(imm);
                    }
                } 
                else {
                    if (do_imm64) {
                        emit_imm64<std::uint64_t>(imm);
                    } else {
                        emit_imm32<std::uint32_t>(imm);
                    }
                }
            }
            
            void emit_reg(const opcode& op8, const opcode& op, const grp& r) {
                if (REG_RANGE(r, r8b, r15b) || REG_RANGE(r, al, spl)) {
                    if (REG_RANGE(r10b, r8b, r15b)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(r, r8b, r15b)) {
                        push_byte(rex_b);
                    }

                    push_byte(static_cast<std::uint8_t>(op8) + static_cast<std::uint8_t>(rebase_register(r)));
                }
                else if (REG_RANGE(r, r8w, r15w) || REG_RANGE(r, ax, sp)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size

                    if (REG_RANGE(r, r8w, r15w)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(r, r8w, r15w)) {
                        push_byte(rex_b);
                    }

                    push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
                }
                else if (REG_RANGE(r, r8d, r15d) || REG_RANGE(r, eax, esp)) {
                    if (REG_RANGE(r, r8d, r15d)) {
                        push_byte(rex_rb);
                    }
                    else if (REG_RANGE(r, r8d, r15d)) {
                        push_byte(rex_b);
                    }

                    push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
                }
                else if (REG_RANGE(r, r8, r15) || REG_RANGE(r, rax, rsp)) {
                    if (REG_RANGE(r, r8, r15)) {
                        push_byte(rex_wrb);
                    }
                    else if (REG_RANGE(r, r8, r15)) {
                        push_byte(rex_wb);
                    }
                    else {
                        push_byte(rex_w);
                    }

                    push_byte(static_cast<std::uint8_t>(op) + static_cast<std::uint8_t>(rebase_register(r)));
                }
            }

            void emit_short_jump(std::uint8_t opcode, std::uint8_t target_address) {
                std::uint8_t current_address = get_code().size();
                std::uint8_t rel8 = target_address - (current_address + 2);
                
                push_byte(opcode);
                push_byte(rel8);
            }

            template<typename T, typename T2>
            void assert_imm_size(const T2&) {
                if (sizeof(T2) == sizeof(T)) {
                    throw std::invalid_argument("Immediate value can not be of size (" + std::to_string(sizeof(T)) + " bytes)");
                }
            }

            template<typename T>
            bool imm_fits(std::int64_t imm) {
                return imm >= std::numeric_limits<T>::min() && imm <= std::numeric_limits<T>::max();
            }
        public:
            x86_64_writer() : writer() {}

            // tbf i should use macros for this, but i can do that another time
            void emit_add(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_add(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::ADD_rm8_r8, opcode::ADD_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_add(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }
            
            void emit_or(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::OR_r8_rm8, opcode::OR_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_or(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::OR_rm8_r8, opcode::OR_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_or(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::OR_r8_rm8, opcode::OR_r16_to_64_rm16_to_64, base, dest);
            }

            void emit_adc(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::ADC_r8_rm8, opcode::ADC_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_adc(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::ADC_rm8_r8, opcode::ADC_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_adc(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::ADC_r8_rm8, opcode::ADC_r16_to_64_rm16_to_64, base, dest);
            }

            void emit_sbb(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::SBB_r8_rm8, opcode::SBB_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_sbb(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::SBB_rm8_r8, opcode::SBB_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_sbb(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::SBB_r8_rm8, opcode::SBB_r16_to_64_rm16_to_64, base, dest);
            }

            void emit_and(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::AND_r8_rm8, opcode::AND_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_and(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::AND_rm8_r8, opcode::AND_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_and(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::AND_r8_rm8, opcode::AND_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }

            void emit_sub(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::SUB_r8_rm8, opcode::SUB_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_sub(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::SUB_rm8_r8, opcode::SUB_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_sub(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::SUB_r8_rm8, opcode::SUB_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }
            
            void emit_xor(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::XOR_r8_rm8, opcode::XOR_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_xor(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::XOR_rm8_r8, opcode::XOR_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_xor(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::XOR_r8_rm8, opcode::XOR_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }

            void emit_cmp(REG_TO_REG_ARG) { 
                emit_reg_to_reg(opcode::CMP_r8_rm8, opcode::CMP_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_cmp(REG_TO_MEM_ARG) {     
                emit_reg_to_mem(opcode::CMP_rm8_r8, opcode::CMP_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_cmp(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::CMP_r8_rm8, opcode::CMP_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }

            void emit_push(REG_ARG) {
                emit_reg(opcode::PUSH_r64_or_16, opcode::PUSH_r64_or_16, reg);
            }

            void emit_pop(REG_ARG) {
                emit_reg(opcode::POP_r64_or_16, opcode::POP_r64_or_16, reg);
            }

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
                emit_reg_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest));
            }
            
            void emit_imul(const IsGrp auto& dest, const IsMem auto& base, const IsSignedImm auto& imm) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest));
            }

            void emit_imul(const IsGrp auto& dest, const IsGrp auto& base, const IsUnsignedImm auto& imm) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), false);
            }
            
            void emit_imul(const IsGrp auto& dest, const IsMem auto& base, const IsUnsignedImm auto& imm) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::IMUL_r16_to_64_rm16_to_64_imm8, opcode::IMUL_r16_to_64_rm16_to_64_imm16_to_32, base, imm, static_cast<rm_field>(dest), false);
            }
            
            void emit_jo(REL8_ARG) {
                emit_short_jump(opcode::JO_rel8, target_address);
            }
            
            void emit_jno(REL8_ARG) {
                emit_short_jump(opcode::JNO_rel8, target_address);
            }
            
            void emit_jb(REL8_ARG) {
                emit_short_jump(opcode::JB_rel8, target_address);
            }
            
            void emit_jnb(REL8_ARG) {
                emit_short_jump(opcode::JNB_rel8, target_address);
            }
            
            void emit_jz(REL8_ARG) {
                emit_short_jump(opcode::JZ_rel8, target_address);
            }
            
            void emit_jnz(REL8_ARG) {
                emit_short_jump(opcode::JNZ_rel8, target_address);
            }
            
            void emit_jbe(REL8_ARG) {
                emit_short_jump(opcode::JBE_rel8, target_address);
            }
            
            void emit_jnbe(REL8_ARG) {
                emit_short_jump(opcode::JNBE_rel8, target_address);
            }
            
            void emit_js(REL8_ARG) {
                emit_short_jump(opcode::JS_rel8, target_address);
            }
            
            void emit_jns(REL8_ARG) {
                emit_short_jump(opcode::JNS_rel8, target_address);
            }
            
            void emit_jp(REL8_ARG) {
                emit_short_jump(opcode::JP_rel8, target_address);
            }
            
            void emit_jnp(REL8_ARG) {
                emit_short_jump(opcode::JNP_rel8, target_address);
            }
            
            void emit_jl(REL8_ARG) {
                emit_short_jump(opcode::JL_rel8, target_address);
            }
            
            void emit_jnl(REL8_ARG) {
                emit_short_jump(opcode::JNL_rel8, target_address);
            }
            
            void emit_jle(REL8_ARG) {
                emit_short_jump(opcode::JLE_rel8, target_address);
            }
            
            void emit_jnle(REL8_ARG) {
                emit_short_jump(opcode::JNLE_rel8, target_address);
            }

            void emit_add(SIGNED_IMM_TO_REG_ARG) { // for these, the number in rm_field is the operation, 0 (add), 1 (or) etc.
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0));
            }

            void emit_add(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false);
            }

            void emit_add(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0));
            }

            void emit_add(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false);
            }

            void emit_or(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1));
            }

            void emit_or(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false);
            }

            void emit_or(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1));
            }

            void emit_or(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(1), false);
            }
            
            void emit_adc(SIGNED_IMM_TO_REG_ARG) { // not gonna lie, the opcode numbers are the same, i cba to change them right now, will do them later
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2));
            }

            void emit_adc(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), false);
            }

            void emit_adc(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2));
            }

            void emit_adc(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(2), false);
            }

            void emit_sbb(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3));
            }

            void emit_sbb(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), false);
            }

            void emit_sbb(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3));
            }

            void emit_sbb(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(3), false);
            }

            void emit_and(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4));
            }

            void emit_and(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), false);
            }

            void emit_and(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4));
            }

            void emit_and(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(4), false);
            }

            void emit_sub(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5));
            }

            void emit_sub(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), false);
            }

            void emit_sub(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5));
            }

            void emit_sub(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(5), false);
            }

            void emit_xor(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6));
            }

            void emit_xor(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), false);
            }

            void emit_xor(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6));
            }

            void emit_xor(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(6), false);
            }

            void emit_cmp(SIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7));
            }

            void emit_cmp(UNSIGNED_IMM_TO_REG_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), false);
            }

            void emit_cmp(SIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7));
            }

            void emit_cmp(UNSIGNED_IMM_TO_MEM_ARG) {
                assert_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::OR_rm8_imm8, opcode::OR_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(7), false);
            }

            void emit_test(REG_TO_REG_ARG) {
                emit_reg_to_reg(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_test(REG_TO_MEM_ARG) {
                emit_reg_to_mem(opcode::TEST_rm8_imm8, opcode::TEST_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_xchg(REG_TO_REG_ARG) {
                emit_reg_to_reg(opcode::XCHG_r8_rm8, opcode::XCHG_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_xchg(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::XCHG_r8_rm8, opcode::XCHG_r16_to_64_rm16_to_64, base, dest); // opposite still
            }
            
            void emit_mov(REG_TO_REG_ARG) {
                emit_reg_to_reg(opcode::MOV_rm8_r8, opcode::MOV_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_mov(REG_TO_MEM_ARG) {
                emit_reg_to_mem(opcode::MOV_rm8_r8, opcode::MOV_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_mov(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::MOV_rm8_r8, opcode::MOV_rm16_to_64_r16_to_64, base, dest);
            }

            /*
                quick note, this library does not support direct memory addresses, you have to load it into a register first 
                then use `operation operand, [register]` for example

                instead of:

                `operation operand, [0xDEADBEEF]` <- won't work

                use:

                `mov register, 0xDEADBEEF`
                `operation operand, [register]` 

                from what i know, this is specific with LEA and perhaps CALL, but its fine i guess, we can't really call with 64 bit addresses anyways? 
            */      
            void emit_lea(MEM_TO_REG_ARG) {
                emit_reg_to_mem(opcode::LEA_r16_to_64_mem, opcode::LEA_r16_to_64_mem, base, dest);
            }

            

            void emit_mov(SIGNED_IMM_TO_REG_ARG) {
                emit_reg_imm_basic(opcode::MOV_rm16_to_64_imm16_to_64, opcode::MOV_rm16_to_64_imm16_to_64, dest, imm);
            }
        };
    }
} // namespace occult