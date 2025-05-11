#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>


/*
    definitions for x86_64
    based on https://ref.x86asm.net/coder64.html
*/

#define DECLARE_OPCODE(name, value) \
    name = value,

namespace occult {
    namespace x86_64 {
        enum rex_bits : std::uint8_t {
            none = 0,
            b = 1 << 0,
            x = 1 << 1,
            r = 1 << 2,
            w = 1 << 3
        };
    
        /*
            to clarify:

            w is the 64-bit operand size (extended register size for 64 bit) rAX to rSP
            r is extending the reg field in the ModR/M byte (+8 to register number i.e r8-15) e.g add r9, rax (rax is base)
            x is extending the index field in the SIB byte (+8 to index register)
            b is extending the rm or base field in ModR/M or SIB byte (+8 to the base register)
        */
        enum rex : std::uint8_t {
            rex = 0x40, // access to extended registers in 64-bit mode
            rex_b = rex | b, // access to extended base register
            rex_x = rex | x, // access to extended SIB index
            rex_xb = rex | x | b, // access to extended SIB index and extended base register
            rex_r = rex | r, // access to extended modrm reg 
            rex_rb = rex | r | b, // access to extended modrm reg and extended base register
            rex_rx = rex | r | x, // access to extended modrm reg and extended SIB index
            rex_rxb = rex | r | x | b, // access to extended modrm reg, extended SIB index and extended base register
            rex_w = rex | w, // 64-bit operand size
            rex_wb = rex | w | b, // 64-bit operand size and extended base register
            rex_wx = rex | w | x, // 64-bit operand size and extended SIB index
            rex_wxb = rex | w | x | b, // 64-bit operand size, extended SIB index and extended base register
            rex_wr = rex | w | r, // 64-bit operand size and extended modrm reg
            rex_wrb = rex | w | r | b, // 64-bit operand size, extended modrm reg and extended base register
            rex_wrx = rex | w | r | x, // 64-bit operand size, extended modrm reg and extended SIB index
            rex_wrxb = rex | w | r | x | b // 64-bit operand size, extended modrm reg, extended SIB index and extended base register
        };

        enum other_prefix : std::uint8_t {
            fs_segment_override = 0x64,
            gs_segment_override = 0x65,
            operand_size_override = 0x66, // 16-bit operand size (also includes precision override)
            address_size_override = 0x67,
        };

        constexpr std::uint8_t k2ByteOpcodePrefix = 0x0F; 
        constexpr std::uint8_t kSpecialSIBIndex = 0b100;

        // addressing method 
        enum class rm_field : std::uint8_t { // in order already
            immediate = 0b000, // 0b000 operand is a constant value (e.g., mov rax, 42)
            reg = 0b001, // 0b001 operand is a register (e.g., mov rax, rbx)
            direct = 0b010, // 0b010 operand is a memory address (e.g., mov rax, [0x1234])
            indirect = 0b011, // 0b011 operand is a memory address in a register (e.g., mov rax, [rbx])
            indexed = 0b100, // 0b100 operand uses base + index (e.g., mov rax, [rbx + rsi])
            rip_relative = 0b101, // 0b110 operand is relative to instruction pointer (e.g., mov rax, [rip + offset]) 
            scaled_index = 0b110, // 0b101 operand uses base + index * scale (e.g., mov rax, [rbx + rsi*4]) NOT SURE IF RIGHT
            segment_offset = 0b111 // 0b111 operand uses segment register offset (e.g., mov rax, fs:[0x10]) NOT SURE IF RIGHT
        };

        // addressing modes
        enum class mod_field : std::uint8_t {
            indirect = 0b00, // [reg] — no displacement (except special case RIP-relative)
            disp8 = 0b01, // [reg + disp8] — 8-bit displacement
            disp32 = 0b10, // [reg + disp32] — 32-bit displacement
            register_direct = 0b11 // reg — register to register (no memory)
        };
        
        // general purpose registers
        enum grp : std::uint8_t {
            rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi,
            eax, ecx, edx, ebx, esp, ebp, esi, edi,
            ax, cx, dx, bx, sp, bp, si, di,
            al, cl, dl, bl, spl, bpl, sil, dil,
            r8, r9, r10, r11, r12, r13, r14, r15,
            r8d, r9d, r10d, r11d, r12d, r13d, r14d, r15d,
            r8w, r9w, r10w, r11w, r12w, r13w, r14w, r15w,
            r8b, r9b, r10b, r11b, r12b, r13b, r14b, r15b,
            rip // ip
        };

        // rebases register to the correct size
        static grp rebase_register(const grp& reg) {
            if (reg >= eax && reg <= esp) {
                return static_cast<grp>(reg - eax + rax);
            }
            else if (reg >= ax && reg <= sp) {
                return static_cast<grp>(reg - ax + rax);
            }
            else if (reg >= al && reg <= spl) {
                return static_cast<grp>(reg - al + rax);
            }
            else if (reg >= r8 && reg <= r15) {
                return static_cast<grp>(reg - r8 + rax);
            }
            else if (reg >= r8d && reg <= r15d) {
                return static_cast<grp>(reg - r8d + rax);
            }
            else if (reg >= r8w && reg <= r15w) {
                return static_cast<grp>(reg - r8w + rax);
            }
            else if (reg >= r8b && reg <= r15b) {
                return static_cast<grp>(reg - r8b + rax);
            }

            return reg;
        }

        // Mod R/M byte
        struct modrm {
            mod_field mod; // addressing mode 
            grp reg; // register
            rm_field rm;  // addressing method (register/memory operand)
            
            modrm(const mod_field& mod, const rm_field& rm, const grp& reg) : mod(mod), reg(reg), rm(rm) {}
            modrm(const mod_field& mod, const grp& rm, const grp& reg) : mod(mod), reg(reg), rm(static_cast<rm_field>(rm)) {}
            modrm(const mod_field& mod, const grp& rm, const rm_field& reg) : mod(mod), reg(static_cast<grp>(reg)), rm(static_cast<rm_field>(rm)) {}
            modrm(const mod_field& mod, const rm_field& rm, const rm_field& reg) : mod(mod), reg(static_cast<grp>(reg)), rm(static_cast<rm_field>(rm)) {}

            operator std::uint8_t() const {
              return (static_cast<std::uint8_t>(mod) << 6) | (reg << 3) | static_cast<std::uint8_t>(rm); 
            }
        };

        // SIB byte
        // Scale, Index, Base
        struct sib {
            std::uint8_t scale; // scaling factor (1, 2, 4 etc.)
            grp index; // index register
            grp base; // base register
            
            sib(const std::uint8_t& scale, const grp& index, const grp& base) : scale(scale), index(index), base(base) {}
            sib(const std::uint8_t& scale, const std::uint8_t& index, const grp& base) : scale(scale), index(static_cast<grp>(index)), base(base) {}

            operator std::uint8_t() const {
              return (scale << 6) | (index << 3) | base;  
            }
        };
        
        /*
            naming scheme for opcodes:
            <operation>_<destination>_<source>
            where <operation> is the operation being performed
            <destination> is the destination of the operation
            <source> is the source of the operation

            for example:
            ADD_rm8_r8 = 0x00
            
            add is the operation
            rm8 is the destination (register/memory 8-bit)
            r8 is the source (register 8-bit)

            another example:
            ADD_rm16_to_64_r16_to_64 = 0x01

            add is the operation
            rm16_to_64 is the destination (register/memory 16-bit to 64-bit)
            r16_to_64 is the source (register 16-bit to 64-bit)

            more examples:
            ADD_al_imm8 = 0x04

            add is the operation
            al is the destination (accumulator register 8-bit)
            imm8 is the source (immediate 8-bit)

            ADD_rAX_imm16_to_32 = 0x05

            add is the operation
            rAX is the destination (accumulator register 16-bit to 32-bit)
            imm16_to_32 is the source (immediate 16-bit to 32-bit)
        */

        enum opcode : std::uint8_t {
            DECLARE_OPCODE(ADD_rm8_r8, 0x00) // ADD r/m8, r8
            DECLARE_OPCODE(ADD_rm16_to_64_r16_to_64, 0x01) // ADD r/m16_to_64, r16_to_64
            DECLARE_OPCODE(ADD_r8_rm8, 0x02) // ADD r8, r/m8
            DECLARE_OPCODE(ADD_r16_to_64_rm16_to_64, 0x03) // ADD r16_to_64, r/m16_to_64
            DECLARE_OPCODE(ADD_al_imm8, 0x04) // ADD AL, imm8
            DECLARE_OPCODE(ADD_rAX_imm16_to_32, 0x05) // ADD rAX, imm16_to_32

            DECLARE_OPCODE(OR_rm8_r8, 0x08) // OR r/m8, r8
            DECLARE_OPCODE(OR_rm16_to_64_r16_to_64, 0x09) // OR r/m16_to_64, r16_to_64
            DECLARE_OPCODE(OR_r8_rm8, 0x0A) // OR r8, r/m8
            DECLARE_OPCODE(OR_r16_to_64_rm16_to_64, 0x0B) // OR r16_to_64, r/m16_to_64
            DECLARE_OPCODE(OR_al_imm8, 0x0C) // OR AL, imm8
            DECLARE_OPCODE(OR_rAX_imm16_to_32, 0x0D) // OR rAX, imm16_to_32

            DECLARE_OPCODE(ADC_rm8_r8, 0x10) // ADC r/m8, r8
            DECLARE_OPCODE(ADC_rm16_to_64_r16_to_64, 0x11) // ADC r/m16_to_64, r16_to_64
            DECLARE_OPCODE(ADC_r8_rm8, 0x12)  // ADC r8, r/m8
            DECLARE_OPCODE(ADC_r16_to_64_rm16_to_64, 0x13) // ADC r16_to_64, r/m16_to_64
            DECLARE_OPCODE(ADC_al_imm8, 0x14) // ADC AL, imm8
            DECLARE_OPCODE(ADC_rAX_imm16_to_32, 0x15) // ADC rAX, imm16_to_32

            DECLARE_OPCODE(SBB_rm8_r8, 0x18) // SBB r/m8, r8
            DECLARE_OPCODE(SBB_rm16_to_64_r16_to_64, 0x19) // SBB r/m16_to_64, r16_to_64
            DECLARE_OPCODE(SBB_r8_rm8, 0x1A) // SBB r8, r/m8
            DECLARE_OPCODE(SBB_r16_to_64_rm16_to_64, 0x1B) // SBB r16_to_64, r/m16_to_64
            DECLARE_OPCODE(SBB_al_imm8, 0x1C) // SBB AL, imm8
            DECLARE_OPCODE(SBB_rAX_imm16_to_32, 0x1D) // SBB rAX, imm16_to_32

            DECLARE_OPCODE(AND_rm8_r8, 0x20) // AND r/m8, r8
            DECLARE_OPCODE(AND_rm16_to_64_r16_to_64, 0x21) // AND r/m16_to_64, r16_to_64
            DECLARE_OPCODE(AND_r8_rm8, 0x22)  // AND r8, r/m8
            DECLARE_OPCODE(AND_r16_to_64_rm16_to_64, 0x23) // AND r16_to_64, r/m16_to_64
            DECLARE_OPCODE(AND_al_imm8, 0x24) // AND AL, imm8
            DECLARE_OPCODE(AND_rAX_imm16_to_32, 0x25) // AND rAX, imm16_to_32

            DECLARE_OPCODE(SUB_rm8_r8, 0x28) // SUB r/m8, r8
            DECLARE_OPCODE(SUB_rm16_to_64_r16_to_64, 0x29) // SUB r/m16_to_64, r16_to_64
            DECLARE_OPCODE(SUB_r8_rm8, 0x2A) // SUB r8, r/m8
            DECLARE_OPCODE(SUB_r16_to_64_rm16_to_64, 0x2B) // SUB r16_to_64, r/m16_to_64
            DECLARE_OPCODE(SUB_al_imm8, 0x2C) // SUB AL, imm8
            DECLARE_OPCODE(SUB_rAX_imm16_to_32, 0x2D) // SUB rAX, imm16_to_32

            DECLARE_OPCODE(XOR_rm8_r8, 0x30) // XOR r/m8, r8
            DECLARE_OPCODE(XOR_rm16_to_64_r16_to_64, 0x31) // XOR r/m16_to_64, r16_to_64
            DECLARE_OPCODE(XOR_r8_rm8, 0x32) // XOR r8, r/m8
            DECLARE_OPCODE(XOR_r16_to_64_rm16_to_64, 0x33) // XOR r16_to_64, r/m16_to_64
            DECLARE_OPCODE(XOR_al_imm8, 0x34) // XOR AL, imm8
            DECLARE_OPCODE(XOR_rAX_imm16_to_32, 0x35) // XOR rAX, imm16_to_32

            DECLARE_OPCODE(CMP_rm8_r8, 0x38) // CMP r/m8, r8
            DECLARE_OPCODE(CMP_rm16_to_64_r16_to_64, 0x39) // CMP r/m16_to_64, r16_to_64
            DECLARE_OPCODE(CMP_r8_rm8, 0x3A) // CMP r8, r/m8
            DECLARE_OPCODE(CMP_r16_to_64_rm16_to_64, 0x3B) // CMP r16_to_64, r/m16_to_64
            DECLARE_OPCODE(CMP_al_imm8, 0x3C) // CMP AL, imm8
            DECLARE_OPCODE(CMP_rAX_imm16_to_32, 0x3D) // CMP rAX, imm16_to_32

            DECLARE_OPCODE(PUSH_r64_or_16, 0x50) // PUSH r64_or_16
            DECLARE_OPCODE(POP_r64_or_16, 0x58)  // POP r64_or_16

            DECLARE_OPCODE(MOVSXD_r64_or_32_rm32, 0x63) // MOVSXD r64_or_32, rm32
            DECLARE_OPCODE(PUSH_imm16_or_32, 0x68)  // PUSH imm16_or_32
            DECLARE_OPCODE(IMUL_r16_to_64_rm16_to_64_imm16_to_32, 0x69) // IMUL r16_to_64, r16_to_64, imm16_to_32
            DECLARE_OPCODE(PUSH_imm8, 0x6A) // PUSH imm8
            DECLARE_OPCODE(IMUL_r16_to_64_rm16_to_64_imm8, 0x6B) // IMUL r16_to_64, r16_to_64, imm8
            
            DECLARE_OPCODE(INS_m8_DX, 0x6C) // INS m8, DX
            DECLARE_OPCODE(INS_m16_DX, 0x6D) // INS m16, DX
            DECLARE_OPCODE(INS_m16_or_32_DX, 0x6D) // INS m16_or_32, DX
            DECLARE_OPCODE(OUTS_DX_m8, 0x6E) // OUTS DX, m8
            DECLARE_OPCODE(OUTS_DX_m16, 0x6F) // OUTS DX, m16
            DECLARE_OPCODE(OUTS_DX_m16_or_32, 0x6F) // OUTS DX, m16_or_32

            DECLARE_OPCODE(JO_rel8, 0x70) // JO rel8
            DECLARE_OPCODE(JNO_rel8, 0x71) // JNO rel8
            DECLARE_OPCODE(JB_rel8, 0x72) // JB rel8
            DECLARE_OPCODE(JNB_rel8, 0x73) // JNB rel8
            DECLARE_OPCODE(JZ_rel8, 0x74) // JZ rel8
            DECLARE_OPCODE(JNZ_rel8, 0x75) // JNZ rel8
            DECLARE_OPCODE(JBE_rel8, 0x76) // JBE rel8
            DECLARE_OPCODE(JNBE_rel8, 0x77) // JNBE rel8
            DECLARE_OPCODE(JS_rel8, 0x78) // JS rel8
            DECLARE_OPCODE(JNS_rel8, 0x79) // JNS rel8
            DECLARE_OPCODE(JP_rel8, 0x7A) // JP rel8
            DECLARE_OPCODE(JNP_rel8, 0x7B) // JNP rel8
            DECLARE_OPCODE(JL_rel8, 0x7C) // JL rel8
            DECLARE_OPCODE(JNL_rel8, 0x7D) // JNL rel8
            DECLARE_OPCODE(JLE_rel8, 0x7E) // JLE rel8
            DECLARE_OPCODE(JNLE_rel8, 0x7F) // JNLE rel8

            DECLARE_OPCODE(ADD_rm8_imm8, 0x80) // ADD r/m8, imm8
            DECLARE_OPCODE(OR_rm8_imm8, 0x80) // OR r/m8, imm8
            DECLARE_OPCODE(ADC_rm8_imm8, 0x80) // ADC r/m8, imm8
            DECLARE_OPCODE(SBB_rm8_imm8, 0x80) // SBB r/m8, imm8
            DECLARE_OPCODE(AND_rm8_imm8, 0x80) // AND r/m8, imm8
            DECLARE_OPCODE(SUB_rm8_imm8, 0x80) // SUB r/m8, imm8
            DECLARE_OPCODE(XOR_rm8_imm8, 0x80) // XOR r/m8, imm8
            DECLARE_OPCODE(CMP_rm8_imm8, 0x80) // CMP r/m8, imm8

            DECLARE_OPCODE(ADD_rm8_to_64_imm16_or_32, 0x81) // ADD r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(OR_rm8_to_64_imm16_or_32, 0x81) // OR r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(ADC_rm8_to_64_imm16_or_32, 0x81) // ADC r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(SBB_rm8_to_64_imm16_or_32, 0x81) // SBB r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(AND_rm8_to_64_imm16_or_32, 0x81) // AND r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(SUB_rm8_to_64_imm16_or_32, 0x81) // SUB r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(XOR_rm8_to_64_imm16_or_32, 0x81) // XOR r/m16/32/64, imm16_or_32
            DECLARE_OPCODE(CMP_rm8_to_64_imm16_or_32, 0x81) // CMP r/m16/32/64, imm16_or_32

            DECLARE_OPCODE(ADD_rm8_to_64_imm8, 0x83) // ADD r/m16/32/64, imm8
            DECLARE_OPCODE(OR_rm8_to_64_imm8, 0x83) // OR r/m16/32/64, imm8
            DECLARE_OPCODE(ADC_rm8_to_64_imm8, 0x83) // ADC r/m16/32/64, imm8
            DECLARE_OPCODE(SBB_rm8_to_64_imm8, 0x83) // SBB r/m16/32/64, imm8
            DECLARE_OPCODE(AND_rm8_to_64_imm8, 0x83) // AND r/m16/32/64, imm8
            DECLARE_OPCODE(SUB_rm8_to_64_imm8, 0x83) // SUB r/m16/32/64, imm8
            DECLARE_OPCODE(XOR_rm8_to_64_imm8, 0x83) // XOR r/m16/32/64, imm8
            DECLARE_OPCODE(CMP_rm8_to_64_imm8, 0x83) // CMP r/m16/32/64, imm8

            DECLARE_OPCODE(TEST_rm8_r8, 0x84) // TEST r/m8, r8
            DECLARE_OPCODE(TEST_rm16_to_64_r16_to_64, 0x85) // TEST r/m16_to_64, r16_to_64
            
            DECLARE_OPCODE(XCHG_r8_rm8, 0x86) // XCHG r/8, rm8
            DECLARE_OPCODE(XCHG_r16_to_64_rm16_to_64, 0x87) // XCHG r16_to_64, r/m16_to_64

            DECLARE_OPCODE(MOV_rm8_r8, 0x88) // MOV r/m8, r8
            DECLARE_OPCODE(MOV_rm16_to_64_r16_to_64, 0x89) // MOV r/m16_to_64, r16_to_64
            DECLARE_OPCODE(MOV_r8_rm8, 0x8A) // MOV r8, r/m8
            DECLARE_OPCODE(MOV_r16_to_64_rm16_to_64, 0x8B) // MOV r16_to_64, r/m16_to_64
            
            // skipping MOV, rm, sreg (its 0x8C)

            DECLARE_OPCODE(LEA_r16_to_64_mem, 0x8D) // LEA r16_to_64, mem

            // skipping MOV sreg, r/m16_to_64 (its 0x8E)

            DECLARE_OPCODE(POP_rm16_to_64, 0x8F) // POP r/m16_to_64

            DECLARE_OPCODE(XCHG_r16_to_64_rAX, 0x90) // XCHG r16_to_64, rAX : 90 + grp

            DECLARE_OPCODE(NOP, 0x90) // NOP

            DECLARE_OPCODE(CBW, 0x98) // CBW
            DECLARE_OPCODE(CWD, 0x99) // CWD

            DECLARE_OPCODE(FWAIT, 0x9B) // FWAIT
            DECLARE_OPCODE(WAIT, 0x9B) // CLD

            DECLARE_OPCODE(PUSHF, 0x9C) // PUSHF
            DECLARE_OPCODE(POPF, 0x9D) // POPF

            DECLARE_OPCODE(SAHF_AH, 0x9E) // SAHF AH
            DECLARE_OPCODE(LAHF_AH, 0x9F) // LAHF AH

            DECLARE_OPCODE(MOV_AL_moffs8, 0xA0) // MOV AL, moffs8
            DECLARE_OPCODE(MOV_rAX_moffs16_to_32, 0xA1) // MOV rAX, moffs16_to_32
            DECLARE_OPCODE(MOV_moffs8_AL, 0xA2) // MOV moffs8, AL
            DECLARE_OPCODE(MOV_moffs16_to_32_rAX, 0xA3) // MOV moffs16_to_32, rAX
            DECLARE_OPCODE(MOVS_m8_m8, 0xA4) // MOVS m8, m8
            DECLARE_OPCODE(MOVS_m16_to_32_m16_to_32, 0xA5) // MOVS m16_to_32, m16_to_32
            DECLARE_OPCODE(CMPS_m8_m8, 0xA6) // CMPS m8, m8
            DECLARE_OPCODE(CMPS_m16_to_32_m16_to_32, 0xA7) // CMPS m16_to_32, m16_to_32
            
            DECLARE_OPCODE(TEST_AL_imm8, 0xA8) // TEST AL, imm8
            DECLARE_OPCODE(TEST_rAX_imm16_to_32, 0xA9) // TEST rAX, imm16_to_32
            DECLARE_OPCODE(STOS_m8_AL, 0xAA) // STOS m8 AL
            DECLARE_OPCODE(STOS_m16_to_32_rAX, 0xAB) // STOS m16_to_32 rAX
            DECLARE_OPCODE(LODS_AL_m8, 0xAC) // LODS AL, m8
            DECLARE_OPCODE(LODS_rAX_m16_to_32, 0xAD) // LODS rAX, m16_to_32
            DECLARE_OPCODE(SCAS_m8_AL, 0xAE) // SCAS m8, AL
            DECLARE_OPCODE(SCAS_rAX_m16_to_32, 0xAF) // SCAS rAX, m16_to_32

            DECLARE_OPCODE(MOV_r8_imm8, 0xB0) // MOV r8, imm8
            DECLARE_OPCODE(MOV_rm16_to_64_imm16_to_64, 0xB8) // MOV r/m16_to_64, imm16_to_64

            DECLARE_OPCODE(ROL_rm8_imm8, 0xC0) // ROL r/m8, imm8
            DECLARE_OPCODE(ROR_rm8_imm8, 0xC0) // ROR r/m8, imm8
            DECLARE_OPCODE(RCL_rm8_imm8, 0xC0) // RCL r/m8, imm8
            DECLARE_OPCODE(RCR_rm8_imm8, 0xC0) // RCR r/m8, imm8
            DECLARE_OPCODE(SHL_rm8_imm8, 0xC0) // SHL r/m8, imm8
            DECLARE_OPCODE(SAL_rm8_imm8, 0xC0) // SAL r/m8, imm8
            DECLARE_OPCODE(SHR_rm8_imm8, 0xC0) // SHR r/m8, imm8
            DECLARE_OPCODE(SAR_rm8_imm8, 0xC0) // SAR r/m8, imm8

            DECLARE_OPCODE(ROL_rm16_to_64_imm8, 0xC1) // ROL r/m16_to_64, imm8
            DECLARE_OPCODE(ROR_rm16_to_64_imm8, 0xC1) // ROR r/m16_to_64, imm8
            DECLARE_OPCODE(RCL_rm16_to_64_imm8, 0xC1) // RCL r/m16_to_64, imm8
            DECLARE_OPCODE(RCR_rm16_to_64_imm8, 0xC1) // RCR r/m16_to_64, imm8
            DECLARE_OPCODE(SHL_rm16_to_64_imm8, 0xC1) // SHL r/m16_to_64, imm8
            DECLARE_OPCODE(SAL_rm16_to_64_imm8, 0xC1) // SAL r/m16_to_64, imm8
            DECLARE_OPCODE(SHR_rm16_to_64_imm8, 0xC1) // SHR r/m16_to_64, imm8
            DECLARE_OPCODE(SAR_rm16_to_64_imm8, 0xC1) // SAR r/m16_to_64, imm8

            DECLARE_OPCODE(RETN_imm16, 0xC2) // RETN imm16
            DECLARE_OPCODE(RETN, 0xC3) // RETN

            DECLARE_OPCODE(MOV_rm8_imm8, 0xC6) // MOV r/m8, imm8
            DECLARE_OPCODE(MOV_rm16_to_64_imm16_or_32, 0xC7) // MOV r/m16_to_64, imm16_or_32

            DECLARE_OPCODE(ENTER_rBP_imm16_imm8, 0xC8) // ENTER rBP, imm16, imm8
            DECLARE_OPCODE(LEAVE_rBP, 0xC9) // LEAVE rBP

            DECLARE_OPCODE(RETNF_imm16, 0xCA) // RETNF imm16   
            DECLARE_OPCODE(RETNF, 0xCB) // RETNF
            
            DECLARE_OPCODE(INT3_eFlags, 0xCC) // INT3 eFlags
            DECLARE_OPCODE(INT_imm8, 0xCD) // INT imm8
            DECLARE_OPCODE(INTO_eFlags, 0xCE) // INTO eFlags
            DECLARE_OPCODE(IRET_eFlags, 0xCF) // IRET eFlags
            
            DECLARE_OPCODE(ROL_rm8_1, 0xD0) // ROL r/m8, 1
            DECLARE_OPCODE(ROR_rm8_1, 0xD0) // ROR r/m8, 1
            DECLARE_OPCODE(RCL_rm8_1, 0xD0) // RCL r/m8, 1
            DECLARE_OPCODE(RCR_rm8_1, 0xD0) // RCR r/m8, 1
            DECLARE_OPCODE(SHL_rm8_1, 0xD0) // SHL r/m8, 1
            DECLARE_OPCODE(SAL_rm8_1, 0xD0) // SAL r/m8, 1
            DECLARE_OPCODE(SHR_rm8_1, 0xD0) // SHR r/m8, 1
            DECLARE_OPCODE(SAR_rm8_1, 0xD0) // SAR r/m8, 1
            DECLARE_OPCODE(ROL_rm16_to_64_1, 0xD1) // ROL r/m16_to_64, 1
            DECLARE_OPCODE(ROR_rm16_to_64_1, 0xD1) // ROR r/m16_to_64, 1
            DECLARE_OPCODE(RCL_rm16_to_64_1, 0xD1) // RCL r/m16_to_64, 1
            DECLARE_OPCODE(RCR_rm16_to_64_1, 0xD1) // RCR r/m16_to_64, 1
            DECLARE_OPCODE(SHL_rm16_to_64_1, 0xD1) // SHL r/m16_to_64, 1
            DECLARE_OPCODE(SAL_rm16_to_64_1, 0xD1) // SAL r/m16_to_64, 1
            DECLARE_OPCODE(SHR_rm16_to_64_1, 0xD1) // SHR r/m16_to_64, 1
            DECLARE_OPCODE(SAR_rm16_to_64_1, 0xD1) // SAR r/m16_to_64, 1
            DECLARE_OPCODE(ROL_rm8_CL, 0xD2) // ROL r/m8, CL
            DECLARE_OPCODE(ROR_rm8_CL, 0xD2) // ROR r/m8, CL
            DECLARE_OPCODE(RCL_rm8_CL, 0xD2) // RCL r/m8, CL
            DECLARE_OPCODE(RCR_rm8_CL, 0xD2) // RCR r/m8, CL
            DECLARE_OPCODE(SHL_rm8_CL, 0xD2) // SHL r/m8, CL
            DECLARE_OPCODE(SAL_rm8_CL, 0xD2) // SAL r/m8, CL
            DECLARE_OPCODE(SHR_rm8_CL, 0xD2) // SHR r/m8, CL
            DECLARE_OPCODE(SAR_rm8_CL, 0xD2) // SAR r/m8, CL
            DECLARE_OPCODE(ROL_rm16_to_64_CL, 0xD3) // ROL r/m16_to_64, CL
            DECLARE_OPCODE(ROR_rm16_to_64_CL, 0xD3) // ROR r/m16_to_64, CL
            DECLARE_OPCODE(RCL_rm16_to_64_CL, 0xD3) // RCL r/m16_to_64, CL
            DECLARE_OPCODE(RCR_rm16_to_64_CL, 0xD3) // RCR r/m16_to_64, CL
            DECLARE_OPCODE(SHL_rm16_to_64_CL, 0xD3) // SHL r/m16_to_64, CL
            DECLARE_OPCODE(SAL_rm16_to_64_CL, 0xD3) // SAL r/m16_to_64, CL
            DECLARE_OPCODE(SHR_rm16_to_64_CL, 0xD3) // SHR r/m16_to_64, CL
            DECLARE_OPCODE(SAR_rm16_to_64_CL, 0xD3) // SAR r/m16_to_64, CL

            DECLARE_OPCODE(XLAT_AL_m8, 0xD7) // XLAT AL, m8

            DECLARE_OPCODE(IN_AL_imm8, 0xE4) // IN AL, imm8
            DECLARE_OPCODE(IN_eAX_imm8, 0xE5) // IN rAX, imm8
            DECLARE_OPCODE(OUT_imm8_AL, 0xE6) // OUT imm8, AL
            DECLARE_OPCODE(OUT_imm8_eAX, 0xE7) // OUT imm8, rAX
            
            DECLARE_OPCODE(INT1_eFlags, 0xF1) // INT1 eFlags
            
            DECLARE_OPCODE(HLT, 0xF4) // HLT
            DECLARE_OPCODE(CMC, 0xF5) // CMC

            DECLARE_OPCODE(TEST_rm8_imm8, 0xF6) // TEST r/m8, imm8
            DECLARE_OPCODE(NOT_rm8, 0xF6) // NOT r/m8
            DECLARE_OPCODE(NEG_rm8, 0xF6) // NEG r/m8
            DECLARE_OPCODE(MUL_AX_AL_rm8, 0xF6) // MUL rAX, r/m8
            DECLARE_OPCODE(IMUL_AX_AL_rm8, 0xF6) // IMUL rAX, r/m8
            DECLARE_OPCODE(DIV_AL_AH_AX_rm8, 0xF6) // DIV rAX, r/m8
            DECLARE_OPCODE(IDIV_AL_AH_AX_rm8, 0xF6) // IDIV rAX, r/m8
            DECLARE_OPCODE(TEST_rm16_to_64_imm16_or_32, 0xF7) // TEST r/m16_to_64, imm16_or_32
            DECLARE_OPCODE(NOT_rm16_to_64, 0xF7) // NOT r/m16_to_64
            DECLARE_OPCODE(NEG_rm16_to_64, 0xF7) // NEG r/m16_to_64
            DECLARE_OPCODE(MUL_rDX_rAX_rm16_to_64, 0xF7) // MUL rAX, r/m16_to_64
            DECLARE_OPCODE(IMUL_rDX_rAX_rm16_to_64, 0xF7) // IMUL rAX, r/m16_to_64
            DECLARE_OPCODE(DIV_rDX_rAX_rm16_to_64, 0xF7) // DIV rAX, r/m16_to_64
            DECLARE_OPCODE(IDIV_rDX_rAX_rm16_to_64, 0xF7) // IDIV rAX, r/m16_to_64

            DECLARE_OPCODE(CLC, 0xF8) // CLR CARRY
            DECLARE_OPCODE(STC, 0xF9) // SET CARRY
            DECLARE_OPCODE(CLI, 0xFA) // CLEAR INTERRUPT
            DECLARE_OPCODE(STI, 0xFB) // SET INTERRUPT
            DECLARE_OPCODE(CLD, 0xFC) // CLEAR DIRECTION
            DECLARE_OPCODE(STD, 0xFD) // SET DIRECTION

            DECLARE_OPCODE(INC_rm8, 0xFE) // INC r/m8
            DECLARE_OPCODE(DEC_imm8, 0xFE) // DEC r/m8
            DECLARE_OPCODE(INC_rm16_to_64, 0xFF) // INC r/m16_to_64
            DECLARE_OPCODE(DEC_rm16_to_64, 0xFF) // DEC r/m16_to_64

            DECLARE_OPCODE(CALL_rm16_or_32, 0xFF) // CALL r/m16_or_32
            DECLARE_OPCODE(CALL_rm64, 0xFF) // CALL r/m64
            
            DECLARE_OPCODE(JMP_rm16_or_32, 0xFF) // JMP r/m16_or_32
            DECLARE_OPCODE(JMP_rm64, 0xFF) // JMP r/m64

            DECLARE_OPCODE(PUSH_rm16_or_32, 0xFF) // PUSH r/m16_or_32
            DECLARE_OPCODE(PUSH_rm64, 0xFF) // PUSH r/m64
        };

        // naming scheme is the same, these require the k2ByteOpcodePrefix (0x0F)
        enum opcode_2b : std::uint8_t { // 2 bytes
            DECLARE_OPCODE(SYSCALL, 0x05) // SYSCALL RCX R11 SS
            DECLARE_OPCODE(SYSRET, 0x07) // SYSRET RAX EFlags SS
            DECLARE_OPCODE(SYSENTER, 0x34) // SYSENTER SS RSP IA32_SYSENTER_EIP
            DECLARE_OPCODE(SYSEXIT, 0x35) // SYSEXIT SS RSP IA32_SYSENTER_EIP

            DECLARE_OPCODE(JO_rel16_or_32, 0x80) // JO rel16/32
            DECLARE_OPCODE(JNO_rel16_or_32, 0x81) // JNO rel16/32
            DECLARE_OPCODE(JB_rel16_or_32, 0x82)  // JB rel16/32
            DECLARE_OPCODE(JNB_rel16_or_32, 0x83) // JNB rel16/32
            DECLARE_OPCODE(JZ_rel16_or_32, 0x84) // JZ rel16/32
            DECLARE_OPCODE(JNZ_rel16_or_32, 0x85) // JNZ rel16/32
            DECLARE_OPCODE(JBE_rel16_or_32, 0x86) // JBE rel16/32
            DECLARE_OPCODE(JNBE_rel16_or_32, 0x87) // JNBE rel16/32
            DECLARE_OPCODE(JS_rel16_or_32, 0x88) // JS rel16/32
            DECLARE_OPCODE(JNS_rel16_or_32, 0x89) // JNS rel16/32
            DECLARE_OPCODE(JP_rel16_or_32, 0x8A) // JP rel16/32
            DECLARE_OPCODE(JNP_rel16_or_32, 0x8B) // JNP rel16/32
            DECLARE_OPCODE(JL_rel16_or_32, 0x8C) // JL rel16/32
            DECLARE_OPCODE(JNL_rel16_or_32, 0x8D) // JNL rel16/32
            DECLARE_OPCODE(JLE_rel16_or_32, 0x8E) // JLE rel16/32
            DECLARE_OPCODE(JNLE_rel16_or_32, 0x8F) // JNLE rel16/32
        };
    } // namespace x86_64
} // namespace occult