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

        enum other_prefix {
            fs_segment_override = 0x64,
            gs_segment_override = 0x65,
            operand_size_override = 0x66, // 16-bit operand size (also includes precision override)
            address_size_override = 0x67,
        };

        // addressing method 
        enum rm_field : std::uint8_t { // in order already
            immediate = 0b000, // 0b000 operand is a constant value (e.g., mov rax, 42)
            reg = 0b001, // 0b001 operand is a register (e.g., mov rax, rbx)
            direct = 0b010, // 0b010 operand is a memory address (e.g., mov rax, [0x1234])
            indirect = 0b011, // 0b011 operand is a memory address in a register (e.g., mov rax, [rbx])
            indexed = 0b100, // 0b100 operand uses base + index (e.g., mov rax, [rbx + rsi])
            scaled_indexed = 0b101, // 0b101 operand uses base + index * scale (e.g., mov rax, [rbx + rsi*4])
            rip_relative = 0b110, // 0b110 operand is relative to instruction pointer (e.g., mov rax, [rip + offset])
            segment_offset = 0b111 // 0b111 operand uses segment register offset (e.g., mov rax, fs:[0x10])
        };

        // addressing modes
        enum mod_field : std::uint8_t {
            indirect = 0b00, // [reg] — no displacement (except special case RIP-relative)
            disp8 = 0b01, // [reg + disp8] — 8-bit displacement
            disp32 = 0b10, // [reg + disp32] — 32-bit displacement
            register_direct = 0b11 // reg — register to register (no memory)
        };
        
        enum reg : std::uint8_t {
            
        };

        struct modrm_byte {

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
            DECLARE_OPCODE(ADD_rm8_r8, 0x00)
            DECLARE_OPCODE(ADD_rm16_to_64_r16_to_64, 0x01)
            DECLARE_OPCODE(ADD_r8_rm8, 0x02)
            DECLARE_OPCODE(ADD_r16_to_64_rm16_to_64, 0x03)
            DECLARE_OPCODE(ADD_al_imm8, 0x04)
            DECLARE_OPCODE(ADD_rAX_imm16_to_32, 0x05)

            DECLARE_OPCODE(OR_rm8_r8, 0x08)
            DECLARE_OPCODE(OR_rm16_to_64_r16_to_64, 0x09)
            DECLARE_OPCODE(OR_r8_rm8, 0x0A)
            DECLARE_OPCODE(OR_r16_to_64_rm16_to_64, 0x0B)
            DECLARE_OPCODE(OR_al_imm8, 0x0C)
            DECLARE_OPCODE(OR_rAX_imm16_to_32, 0x0D)

            DECLARE_OPCODE(ADC_rm8_r8, 0x10)
            DECLARE_OPCODE(ADC_rm16_to_64_r16_to_64, 0x11)
            DECLARE_OPCODE(ADC_r8_rm8, 0x12)
            DECLARE_OPCODE(ADC_r16_to_64_rm16_to_64, 0x13)
            DECLARE_OPCODE(ADC_al_imm8, 0x14)
            DECLARE_OPCODE(ADC_rAX_imm16_to_32, 0x15)

            DECLARE_OPCODE(SBB_rm8_r8, 0x18)
            DECLARE_OPCODE(SBB_rm16_to_64_r16_to_64, 0x19)
            DECLARE_OPCODE(SBB_r8_rm8, 0x1A)
            DECLARE_OPCODE(SBB_r16_to_64_rm16_to_64, 0x1B)
            DECLARE_OPCODE(SBB_al_imm8, 0x1C)
            DECLARE_OPCODE(SBB_rAX_imm16_to_32, 0x1D)

            DECLARE_OPCODE(AND_rm8_r8, 0x20)
            DECLARE_OPCODE(AND_rm16_to_64_r16_to_64, 0x21)
            DECLARE_OPCODE(AND_r8_rm8, 0x22)
            DECLARE_OPCODE(AND_r16_to_64_rm16_to_64, 0x23)
            DECLARE_OPCODE(AND_al_imm8, 0x24)
            DECLARE_OPCODE(AND_rAX_imm16_to_32, 0x25)

            DECLARE_OPCODE(SUB_rm8_r8, 0x28)
            DECLARE_OPCODE(SUB_rm16_to_64_r16_to_64, 0x29)
            DECLARE_OPCODE(SUB_r8_rm8, 0x2A)
            DECLARE_OPCODE(SUB_r16_to_64_rm16_to_64, 0x2B)
            DECLARE_OPCODE(SUB_al_imm8, 0x2C)
            DECLARE_OPCODE(SUB_rAX_imm16_to_32, 0x2D)

            DECLARE_OPCODE(XOR_rm8_r8, 0x30)
            DECLARE_OPCODE(XOR_rm16_to_64_r16_to_64, 0x31)
            DECLARE_OPCODE(XOR_r8_rm8, 0x32)
            DECLARE_OPCODE(XOR_r16_to_64_rm16_to_64, 0x33)
            DECLARE_OPCODE(XOR_al_imm8, 0x34)
            DECLARE_OPCODE(XOR_rAX_imm16_to_32, 0x35)

            DECLARE_OPCODE(CMP_rm8_r8, 0x38)
            DECLARE_OPCODE(CMP_rm16_to_64_r16_to_64, 0x39)
            DECLARE_OPCODE(CMP_r8_rm8, 0x3A)
            DECLARE_OPCODE(CMP_r16_to_64_rm16_to_64, 0x3B)
            DECLARE_OPCODE(CMP_al_imm8, 0x3C)
            DECLARE_OPCODE(CMP_rAX_imm16_to_32, 0x3D)

            DECLARE_OPCODE(PUSH_r64_or_16, 0x50)
            DECLARE_OPCODE(POP_r64_or_16, 0x58) 

            DECLARE_OPCODE(MOVSXD_r64_or_32_rm32, 0x63)
        };
    }
} // namespace occult