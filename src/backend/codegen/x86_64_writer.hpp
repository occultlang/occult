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

#define REG_TO_REG_ARG const IsGrp auto& dest, const IsGrp auto& base
#define REG_TO_MEM_ARG const IsMem auto& dest, const IsGrp auto& base
#define MEM_TO_REG_ARG const IsGrp auto& dest, const IsMem auto& base
#define SIGNED_IMM_TO_REG_ARG const IsGrp auto& dest, const IsSignedImm auto& imm
#define UNSIGNED_IMM_TO_REG_ARG const IsGrp auto& dest, const IsUnsignedImm auto& imm
#define SIGNED_IMM_TO_MEM_ARG const IsMem auto& dest, const IsSignedImm auto& imm
#define UNSIGNED_IMM_TO_MEM_ARG const IsMem auto& dest, const IsUnsignedImm auto& imm

        class x86_64_writer : public writer {  
            template<typename IntType = std::int8_t> // template if we want to use unsigned values
            void emit_imm8(IntType imm) {
                for (int i = 0; i < 1; ++i) {
                    push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
                }
            }
            
            template<typename IntType = std::int16_t>
            void emit_imm16(IntType imm) {
                for (int i = 0; i < 2; ++i) {
                    push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
                }
            }
            
            template<typename IntType = std::int32_t>
            void emit_imm32(IntType imm) {
                for (int i = 0; i < 4; ++i) {
                    push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
                }
            }
            
            template<typename IntType = std::int64_t>
            void emit_imm64(IntType imm) {
                for (int i = 0; i < 8; ++i) {
                    push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
                }
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
                    push_byte(modrm(mod_field::register_direct, rm, rebase_register(dest)));

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
                    push_byte(modrm(mod_field::register_direct, rm, rebase_register(dest)));
                    
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

            template<typename T, typename T2>
            void validate_imm_size(const T2&) {
                if (sizeof(T2) == sizeof(T)) {
                    throw std::invalid_argument("Immediate value can not be of size (" + std::to_string(sizeof(T)) + " bytes)");
                }
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

            /* continue to add opcodes in order according to coder64 */

            /* immedaite for memory should be finished */

            void emit_add(SIGNED_IMM_TO_REG_ARG) {
                validate_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0));
            }

            void emit_add(UNSIGNED_IMM_TO_REG_ARG) {
                validate_imm_size<std::uint64_t>(imm);
                emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false);
            }

            void emit_add(SIGNED_IMM_TO_MEM_ARG) {
                validate_imm_size<std::int64_t>(imm);
                emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0));
            }

            void emit_add(UNSIGNED_IMM_TO_MEM_ARG) {
                validate_imm_size<std::uint64_t>(imm);
                emit_mem_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0), false);
            }
        };
    }
} // namespace occult