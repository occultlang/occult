#include "writer.hpp"
#include "x86_64_defs.hpp"

// rewritten entirety of the x64writer to be more readable and understandable
// will document as i go, more
// based on https://ref.x86asm.net/coder64.html

#define REG_RANGE(to_cmp, start, end) \
    (to_cmp >= start && to_cmp <= end)


namespace occult {
    namespace x86_64 {
        class x86_64_writer : public writer {  
            void emit_reg_to_reg(const i_opcode& op8, const i_opcode& op, const grp& dest, const grp& base) { 
                if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, spl) || 
                    REG_RANGE(base, r8b, r15b) || REG_RANGE(base, al, spl)) {
                    if (REG_RANGE(dest, r8b, r15b) && REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex::rex_rb);
                    }
                    else if (REG_RANGE(dest, r8b, r15b)) {
                        push_byte(rex::rex_b);
                    }
                    else if (REG_RANGE(base, r8b, r15b)) {
                        push_byte(rex::rex_r);
                    }

                    push_byte(op8);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, sp) || 
                         REG_RANGE(base, r8w, r15w) || REG_RANGE(base, ax, sp)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size

                    if (REG_RANGE(dest, r8w, r15w) && REG_RANGE(base, r8w, r15w)) {
                        push_byte(rex::rex_rb);
                    }
                    else if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(rex::rex_b);
                    }
                    else if (REG_RANGE(base, r8w, r15w)) {
                        push_byte(rex::rex_r);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, esp) || 
                         REG_RANGE(base, r8d, r15d) || REG_RANGE(base, eax, esp)) {
                    if (REG_RANGE(dest, r8d, r15d) && REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex::rex_rb);
                    }
                    else if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex::rex_b);
                    }
                    else if (REG_RANGE(base, r8d, r15d)) {
                        push_byte(rex::rex_r);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
                else if (REG_RANGE(dest, r8, r15) || REG_RANGE(dest, rax, rsp) || 
                         REG_RANGE(base, r8, r15) || REG_RANGE(base, rax, rsp)) {
                    if (REG_RANGE(dest, r8, r15) && REG_RANGE(base, r8, r15)) {
                        push_byte(rex::rex_wrb);
                    }
                    else if (REG_RANGE(dest, r8, r15)) {
                        push_byte(rex::rex_wb);
                    }
                    else if (REG_RANGE(base, r8, r15)) {
                        push_byte(rex::rex_wr);
                    }
                    else {
                        push_byte(rex::rex_w);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rebase_register(base)));
                }
            }
        public:
            x86_64_writer() : writer() {}

            void emit_add(const grp& dest, const grp& base) { // will add other things like immediates and memory later :)
                emit_reg_to_reg(i_opcode::ADD_r8_rm8, i_opcode::ADD_r16_to_64_rm16_to_64, dest, base);
            }
        };
    }
} // namespace occult