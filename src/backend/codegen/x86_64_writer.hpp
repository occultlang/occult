#include "writer.hpp"
#include "x86_64_defs.hpp"

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
        inline constexpr std::nullopt_t null_val { std::nullopt_t::_Construct::_Token }; // same as nullopt

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

            void emit_reg_to_reg(const i_opcode& op8, const i_opcode& op, const grp& dest, const grp& base) { 
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

            void emit_reg_to_mem(const i_opcode& op8, const i_opcode& op, const grp& dest,  
                                std::optional<grp> index, std::optional<std::size_t> scale, 
                                std::optional<std::int32_t> disp, const grp& base) { 
                if ((NOT_STACK_PTR(dest) && NOT_STACK_BASE_PTR(dest)) && (!index.has_value() && !disp.has_value() && !scale.has_value())) { // [reg]
                    if (REG_RANGE(dest, r8b, r15b) && REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_rb);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(dest, r8b, r15b)) {
                        push_byte(rex_b);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_r);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(dest, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_rb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_b);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_r);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_rb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex_b);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_r);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8, r15) && REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wrb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8, r15)) {
                        push_byte(rex_wb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wr);
                        push_byte(op);
                    }
                    else {
                        push_byte(rex_w);
                        push_byte(op);
                    }
                    
                    push_byte(modrm(mod_field::indirect, rebase_register(dest), rebase_register(base)));
                } 
                else if ((IS_STACK_PTR(dest) && NOT_STACK_BASE_PTR(dest)) && (!index.has_value() && !disp.has_value() && !scale.has_value())) { // only [rsp]
                    push_byte(rex_w);
                    push_byte(op);
                    push_byte(modrm(mod_field::indirect, rm_field::indexed, rebase_register(base))); 
                    push_byte(sib(0, kSpecialSIBIndex, rebase_register(dest)));
                }
                else if (NOT_STACK_PTR(dest) && IS_STACK_BASE_PTR(dest) && (!index.has_value() && disp.has_value() && !scale.has_value())) { // [reg + disp32] (we are always going to do disp32)
                    if (REG_RANGE(dest, r8b, r15b) && REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_rb);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(dest, r8b, r15b)) {
                        push_byte(rex_b);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex_r);
                        push_byte(op8);
                    }
                    else if (REG_RANGE(dest, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_rb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_b);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8w, r15w)) {
                        push_byte(other_prefix::operand_size_override); // 16-bit operand size
                        push_byte(rex_r);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_rb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex_b);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex_r);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8, r15) && REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wrb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(dest, r8, r15)) {
                        push_byte(rex_wb);
                        push_byte(op);
                    }
                    else if (REG_RANGE(base, r8, r15)) {
                        push_byte(rex_wr);
                        push_byte(op);
                    }
                    else {
                        push_byte(rex_w);
                        push_byte(op);
                    }

                    push_byte(modrm(mod_field::disp32, rebase_register(dest), rebase_register(base)));
                    emit_imm32(disp.value());
                }
            }
        public:
            x86_64_writer() : writer() {}

            void emit_add(const grp& dest, const grp& base) { 
                emit_reg_to_reg(i_opcode::ADD_r8_rm8, i_opcode::ADD_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_add(const grp& dest, std::optional<grp> index, std::optional<std::size_t> scale, 
                          std::optional<std::int32_t> disp, const grp& base) { 
                emit_reg_to_mem(i_opcode::ADD_rm8_r8, i_opcode::ADD_rm16_to_64_r16_to_64, dest, index, scale, disp, base);
            }
        };
    }
} // namespace occult