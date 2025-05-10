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
        inline constexpr std::nullopt_t null_val { std::nullopt_t::_Construct::_Token }; // same as nullopt

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
        concept IsImm = std::is_integral_v<T>;

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
                if (dest.reg == bpl) {
                    push_byte(op8);
                }
                else if (dest.reg == bp) {
                    push_byte(other_prefix::operand_size_override);
                    push_byte(op);
                }
                else if (dest.reg == ebp) {
                    push_byte(op);
                }
                else if (dest.reg == rbp) {
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
            void emit_reg_imm(const opcode& op8, const opcode& op, const grp& dest, const IsImm auto& imm, const rm_field& rm) {
                if (REG_RANGE(dest, r8b, r15b) || REG_RANGE(dest, al, spl)) {
                    if (REG_RANGE(dest, r8b, r15b)) {
                        push_byte(rex_b);
                    }

                    push_byte(op8);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));
                    emit_imm8(imm);
                }
                else if (REG_RANGE(dest, r8w, r15w) || REG_RANGE(dest, ax, sp)) {
                    push_byte(other_prefix::operand_size_override); // 16-bit operand size
                    if (REG_RANGE(dest, r8w, r15w)) {
                        push_byte(rex_b);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rebase_register(dest), rm));
                    emit_imm16(imm);
                }
                else if (REG_RANGE(dest, r8d, r15d) || REG_RANGE(dest, eax, esp)) {
                    if (REG_RANGE(dest, r8d, r15d)) {
                        push_byte(rex_b);
                    }

                    push_byte(op);
                    push_byte(modrm(mod_field::register_direct, rm, rebase_register(dest)));
                    emit_imm32(imm);
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
                    emit_imm32(imm);
                }
            }

            template<typename T, typename T2>
            void validate_imm_size(const T2& imm) {
                if (typeid(T2) == typeid(T)) {
                    throw std::invalid_argument("Immediate value can not be of 64-bit size");
                }
            }
        public:
            x86_64_writer() : writer() {}
            
            void emit_add(const IsGrp auto& dest, const IsGrp auto& base) { 
                emit_reg_to_reg(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, dest, base);
            }

            void emit_add(const IsMem auto& dest, const IsGrp auto& base) {     
                emit_reg_to_mem(opcode::ADD_rm8_r8, opcode::ADD_rm16_to_64_r16_to_64, dest, base);
            }

            void emit_add(const IsGrp auto& dest, const IsMem auto& base) {
                emit_reg_to_mem(opcode::ADD_r8_rm8, opcode::ADD_r16_to_64_rm16_to_64, base, dest); // we can just use the same function apparently...?
            }
            
            void emit_add(const IsGrp auto& dest, const IsImm auto& imm) {
                validate_imm_size<std::int64_t>(imm);
                emit_reg_imm(opcode::ADD_rm8_imm8, opcode::ADD_rm8_to_64_imm16_or_32, dest, imm, static_cast<rm_field>(0));
            }
        };
    }
} // namespace occult