#pragma once
#include "writer.hpp"
#include <cmath>
#include <cstdint>

// TODO: need to do memory for emit_reg_imm16_32 and below.
// TODO: add bitshifting

// a lot of this is with http://ref.x86asm.net/coder64.html & https://defuse.ca/online-x86-assembler.htm
// quick note right now, memory support may be limited as i have not really dealt with SIB yet, so bear with me please... (no rsp support yet)
// another note, there is no floating point operations as of now, i have yet to learn about it

// function naming goes as follows
// emit_OPERATION_DEST_SRC_*SIZE*
// some have full 64-bit to 8-bit support so it wont have anything like 32_8 at the end
// all if not most will have the operation as follows, look it up on coder64 if you need a reference
// DEST_SRC specifies where it is going and the source
/*
 * example: emit_add_with_carry_accumulator_imm_32_8
 * 
 * operation: adc / add with carry
 * destination: accumulator register (eax, ax, al)
 * source: immediate value
 * size: 32-bit to 8-bit
 *
 * example: emit_add_reg_mem
 * 
 * operation: add
 * destination: general purpose registers 
 * source: memory (rip + displacement, or register)
 * size: 64-bit to 8-bit (there is no specified size such as 32_8, therefore this fully supports it)
*/

namespace occult {  
  enum rex_prefix : std::uint8_t {
    rex = 0x40, // access to new 8-bit registers
    rex_b = 0x41, // extension of r/m field, base field, or opcode reg field
    rex_x = 0x42, // extension of SIB index field
    rex_xb = 0x43, // rex.x and rex.b combination
    rex_r = 0x44, // extension of modr/m reg field
    rex_rb = 0x45, // rex.r and rex.b combination
    rex_rx = 0x46, // rex.r and rex.x combination
    rex_rxb = 0x47, // rex.r, rex.x and rex.b combination
    rex_w = 0x48, // 64-bit operand size
    rex_wb = 0x49, // rex.w and rex.b combination
    rex_wx = 0x4A, // rex.w and rex.x combination
    rex_wxb = 0x4B, // rex.w, rex.x and rex.b combination
    rex_wr = 0x4C, // rex.w and rex.r combination
    rex_wrb = 0x4D, // rex.w, rex.r and rex.b combination
    rex_wrx = 0x4E, // rex.w, rex.r and rex.x combination
    rex_wrxb = 0x4F // rex.w, rex.r, rex.x and rex.b combination
  };
  
  static std::unordered_map<std::size_t, std::uint8_t> prefix64 = {
    {256, rex_wr},  // rex.wr 64-bit extended register for sources (r8 - 15)
    {128, rex_wb},  // rex.wb, 64-bit extended registers for targets (r8 - 15)
    {64, rex_w},    // rex.w
    {32, 0x00},     // default
    {16, 0x66},     // operand size override
    {8, 0x00},      // default
  }; // prefixes for sizes
  
  static std::unordered_map<std::string, std::uint8_t> x64_register = {
    {"rax", 0x00},
    {"rcx", 0x01},
    {"rdx", 0x02},
    {"rbx", 0x03},
    {"rsp", 0x04},
    {"rbp", 0x05},
    {"rsi", 0x06},
    {"rdi", 0x07},
    // extended registers
    {"r8", 0x00},
    {"r9", 0x01},
    {"r10", 0x02},
    {"r11", 0x03},
    {"r12", 0x04},
    {"r13", 0x05},
    {"r14", 0x06},
    {"r15", 0x07},
  };
  
  enum addressing_modes : std::uint8_t {
    direct = 0b00,
    disp8 = 0b01,
    disp32 = 0b10,
    reg_to_reg = 0b11
  };
  
  enum rm_field : std::uint8_t {
    reg = 0b000,        
    mem_disp8 = 0b001,   
    mem_disp32 = 0b010,      
    reg_direct = 0b011,    
    sib = 0b100,                
    alt_mem_disp32 = 0b101,  
    mem_direct = 0b110,      
    alt_reg_direct = 0b111 
  };
  
  constexpr std::uint8_t no_register = 0b000;
  
  struct modrm_byte {
    std::uint8_t addressing_mode; // addressing mode
    std::uint8_t reg; // register
    std::uint8_t rm;  // memory operand or register
    
    modrm_byte(const std::uint8_t& addressing_mode, const std::uint8_t& reg, const std::uint8_t& rm) {
      this->addressing_mode = addressing_mode;
      this->reg = reg;
      this->rm = rm;
    }
    
    operator std::uint8_t() const {
      return (addressing_mode << 6) | (reg << 3) | rm; 
    }
  };
  
  struct sib_byte {
    std::uint8_t scale; // scaling factor (1, 2, 4 etc.)
    std::uint8_t index; // index register
    std::uint8_t base; // base register
    
    sib_byte(const std::uint8_t& scale, const std::uint8_t& index, const std::uint8_t& base) {
      this->scale = scale;
      this->index = index;
      this->base = base;
    }
    
    operator std::uint8_t() const {
      return (scale << 6) | (index << 3) | base;  
    }
  };
  
  constexpr std::size_t k64bit_ext_target = 128; // target registers would be extended 64 bit registers r8-15
  constexpr std::size_t k64bit_ext_src = 256; // src registers would be extended 64 bit registers r8-15
  constexpr std::size_t k64bit = 64;
  constexpr std::size_t k32bit = 32;
  constexpr std::size_t k16bit = 16;
  constexpr std::size_t k8bit = 8;
  // add extended registers later on k
  
  struct x64writer : writer {
    x64writer() : writer() {}
    
    void emit_ret() {
      push_byte(0xC3);
    }
    
    void emit_syscall() {
      push_bytes({0x0F, 0x05});
    }
    
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
    
    void emit_imm_by_size(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size) { 
      switch(size) {
        case k64bit_ext_target:
        case k64bit_ext_src:
        case k64bit: {
          if (std::holds_alternative<std::uint64_t>(imm)) {
            emit_imm64<std::uint64_t>(std::get<std::uint64_t>(imm));
          }
          else if (std::holds_alternative<std::int64_t>(imm)) {
            emit_imm64(std::get<std::int64_t>(imm));
          }
          
          break;
        }
        case k32bit: {
          if (std::holds_alternative<std::uint64_t>(imm)) {
            emit_imm32<std::uint32_t>(std::get<std::uint64_t>(imm));
          }
          else if (std::holds_alternative<std::int64_t>(imm)) {
            emit_imm32(std::get<std::int64_t>(imm));
          }
          
          break;
        }
        case k16bit: {
          if (std::holds_alternative<std::uint64_t>(imm)) {
            emit_imm16<std::uint16_t>(std::get<std::uint64_t>(imm));
          }
          else if (std::holds_alternative<std::int64_t>(imm)) {
            emit_imm16(std::get<std::int64_t>(imm));
          }
          
          break;
        }
        case k8bit: {
          if (std::holds_alternative<std::uint64_t>(imm)) {
            emit_imm8<std::uint8_t>(std::get<std::uint64_t>(imm));
          }
          else if (std::holds_alternative<std::int64_t>(imm)) {
            emit_imm8(std::get<std::int64_t>(imm));
          }
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    inline std::size_t get_bit_size(std::variant<std::uint64_t, std::int64_t> v) {
      if (std::holds_alternative<std::uint64_t>(v)) {
        if (std::get<std::uint64_t>(v) <= UCHAR_MAX) {
          return 8; 
        }
        else if (std::get<std::uint64_t>(v) <= USHRT_MAX) {
          return 16;  
        }
        else if (std::get<std::uint64_t>(v) <= UINT_MAX) {
          return 32; 
        }
        else {
          return 64;
        }
      }
      else {
        if (std::get<std::int64_t>(v) <= CHAR_MAX) {
          return 8; 
        }
        else if (std::get<std::int64_t>(v) <= SHRT_MAX) {
          return 16;  
        }
        else if (std::get<std::int64_t>(v) <= INT_MAX) {
          return 32; 
        }
        else {
          return 64;
        }
      }
    }
    
    void check_prefix_size(const std::size_t& size) {
      if (prefix64.find(size) == prefix64.end()) {
        throw std::invalid_argument("invalid operand size: " + std::to_string(size));
      }
    }
    
    // register to register with modrm encoding 64-bit to 8-bit opcodes as args
    void emit_reg_reg_64_8(std::uint8_t opcode, std::uint8_t opcode8, const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      std::uint8_t modrm = modrm_byte(addressing_modes::reg_to_reg, x64_register[src], x64_register[dest]);
      
      if (size == k8bit) {
        push_byte(opcode8);
      }
      else {
        push_byte(opcode);
      }
      
      push_byte(modrm);
    }
    
    void emit_reg_mem_64_8(std::uint8_t opcode, std::uint8_t opcode8, const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t modrm = 0;
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(opcode8);
      }
      else {
        push_byte(opcode);  
      }
      
      if (disp != 0) { // reg + disp
        if (src == "rip" && size != k8bit) {
          modrm = modrm_byte(addressing_modes::direct, x64_register[dest], rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          emit_imm32(disp);
        }
        else {
          if (size == k8bit) {
            modrm = modrm_byte(addressing_modes::disp8, x64_register[dest], x64_register[src]);
            
            push_byte(modrm);
            emit_imm8(disp);
          }
          else {
            modrm = modrm_byte(addressing_modes::disp32, x64_register[dest], x64_register[src]);
            
            push_byte(modrm);
            emit_imm32(disp);
          }
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, x64_register[dest], x64_register[src]);
        
        push_byte(modrm);
      }
    }
    
    void emit_mem_reg_64_8(std::uint8_t opcode, std::uint8_t opcode8, const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t modrm = 0;
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(opcode8);
      }
      else {
        push_byte(opcode);  
      }
      
      if (disp != 0) { // reg + disp
        if (dest == "rip" && size != k8bit) {
          modrm = modrm_byte(addressing_modes::direct, x64_register[src], rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          
          emit_imm32(disp);
        }
        else {
          if (size == k8bit) {
            modrm = modrm_byte(addressing_modes::disp8, x64_register[src], x64_register[dest]);
            
            push_byte(modrm);
            emit_imm8(disp);
          }
          else {
            modrm = modrm_byte(addressing_modes::disp32, x64_register[src], x64_register[dest]);
            
            push_byte(modrm);
            emit_imm32(disp);
          }
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, x64_register[src], x64_register[dest]);
        
        push_byte(modrm);
      }
    }
    
    // rAX or AL == accumulator (for logical or arithmetic in the beginning of coder64)
    void emit_logical_accumulator_imm_32_8(std::uint8_t opcode, std::uint8_t opcode8, std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit) {
        throw std::invalid_argument("invalid operand size for emit_logical_accumulator_imm_32_8: " + std::to_string(size));
      }
      
      if (size == k8bit) {
        push_byte(opcode8);
        emit_imm_by_size(imm, size);
      }
      else {
        push_byte(opcode);
        emit_imm_by_size(imm, size);
      }
    }
    
    // move register to register
    void emit_mov_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) { // mov reg, reg
      std::uint8_t mov = 0x89; // MOV 	r/m16/32/64 	r16/32/64
      std::uint8_t mov8bit = 0x88;
      
      check_prefix_size(size);
      
      emit_reg_reg_64_8(mov, mov8bit, dest, src, size);
    }
    
    // move memory to register
    void emit_mov_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) { // mov reg, [disp/register + disp]
      std::uint8_t mov = 0x8B; // 	MOV 	r16/32/64 	r/m16/32/64
      std::uint8_t mov8bit = 0x8A;
      
      check_prefix_size(size);
      
      emit_reg_mem_64_8(mov, mov8bit, dest, src, disp, size);
    }
    
    // move register to memory
    void emit_mov_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) { // mov [disp/register + disp], reg
      std::uint8_t mov = 0x89; // MOV 	r/m16/32/64 	r16/32/64
      std::uint8_t mov8bit = 0x88;
      
      check_prefix_size(size);
      
      emit_mem_reg_64_8(mov, mov8bit, dest, disp, src, size);
    }
    
    // move immediate to memory
    void emit_mov_mem_imm(const std::string& dest, std::int64_t disp, std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k64bit) { // mov [disp/register + disp], imm
      std::uint8_t mov = 0xC7;
      std::uint8_t mov8bit = 0xC6;
      std::uint8_t modrm = 0;
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(mov8bit);
      }
      else {
        push_byte(mov);  
      }
      
      if (disp != 0) { // reg + disp
        if (dest == "rip" && size != k8bit) {
          modrm = modrm_byte(addressing_modes::direct, no_register, rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          
          emit_imm32(disp);
        }
        else {
          if (size == k8bit) {
            modrm = modrm_byte(addressing_modes::disp8, no_register, x64_register[dest]);
            
            push_byte(modrm);
            emit_imm8(disp);
          }
          else {
            modrm = modrm_byte(addressing_modes::disp32, no_register, x64_register[dest]);
            
            push_byte(modrm);
            emit_imm32(disp);
          }
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, no_register, x64_register[dest]);
        
        push_byte(modrm);
      }
      
      emit_imm_by_size(imm, size);
    }
    
    // move immediate to register
    void emit_mov_reg_imm(const std::string& reg, std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k64bit) { // mov reg, imm
      std::uint8_t mov32 = 0xB8;
      std::uint8_t mov8bit = 0xB0;
      
      check_prefix_size(size);
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(mov8bit + x64_register[reg]);
      }
      else {
        push_byte(mov32 + x64_register[reg]);
      }

      emit_imm_by_size(imm, size);
    }
    
    void emit_add_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x00;
      std::uint8_t add_no_8bit = 0x01;
 
      check_prefix_size(size);
      
      emit_reg_reg_64_8(add_no_8bit, add_8bit, dest, src, size);
    }
    
    void emit_add_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x00;
      std::uint8_t add_no_8bit = 0x01;
      
      check_prefix_size(size);
      
      emit_mem_reg_64_8(add_no_8bit, add_8bit, dest, disp, src, size);
    }
    
    void emit_add_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x02;
      std::uint8_t add_no_8bit = 0x03;
      
      check_prefix_size(size);
      
      emit_reg_mem_64_8(add_no_8bit, add_8bit, dest, src, disp);
    }
     
    void emit_add_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t add_no_8bit = 0x05;
      std::uint8_t add_8bit = 0x04;
      
      check_prefix_size(size);
      
      emit_logical_accumulator_imm_32_8(add_no_8bit, add_8bit, imm, size);
    }
    
    void emit_inclusive_or_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x08;
      std::uint8_t add_no_8bit = 0x09;
    
      check_prefix_size(size);
      
      emit_reg_reg_64_8(add_no_8bit, add_8bit, dest, src, size);
    }
    
    void emit_inclusive_or_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x08;
      std::uint8_t add_no_8bit = 0x09;
      
      check_prefix_size(size);
      
      emit_mem_reg_64_8(add_no_8bit, add_8bit, dest, disp, src, size);
    }
    
    void emit_inclusive_or_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x0A;
      std::uint8_t add_no_8bit = 0x0B;
      
      check_prefix_size(size);
      
      emit_reg_mem_64_8(add_no_8bit, add_8bit, dest, src, disp);
    }
     
    void emit_inclusive_or_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t add_8bit = 0x0C;
      std::uint8_t add_no_8bit = 0x0D;
      
      check_prefix_size(size);
      
      emit_logical_accumulator_imm_32_8(add_no_8bit, add_8bit, imm, size);
    }
    
    void emit_add_with_carry_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x10;
      std::uint8_t add_no_8bit = 0x11;
      
      check_prefix_size(size);
      
      emit_reg_reg_64_8(add_no_8bit, add_8bit, dest, src, size);
    }
    
    void emit_add_with_carry_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x10;
      std::uint8_t add_no_8bit = 0x11;
      
      check_prefix_size(size);
      
      emit_mem_reg_64_8(add_no_8bit, add_8bit, dest, disp, src, size);
    }
    
    void emit_add_with_carry_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x12;
      std::uint8_t add_no_8bit = 0x13;
      
      check_prefix_size(size);
      
      emit_reg_mem_64_8(add_no_8bit, add_8bit, dest, src, disp);
    }
    
    void emit_add_with_carry_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t add_8bit = 0x14;
      std::uint8_t add_no_8bit = 0x15;
      
      check_prefix_size(size);
      
      emit_logical_accumulator_imm_32_8(add_no_8bit, add_8bit, imm, size);
    }
    
    void emit_sub_with_borrow_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x18;
      std::uint8_t add_no_8bit = 0x19;
    
      check_prefix_size(size);
    
      emit_reg_reg_64_8(add_no_8bit, add_8bit, dest, src, size);
    }
    
    void emit_sub_with_borrow_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x18;
      std::uint8_t add_no_8bit = 0x19;
    
      check_prefix_size(size);
    
      emit_mem_reg_64_8(add_no_8bit, add_8bit, dest, disp, src, size);
    }
    
    void emit_sub_with_borrow_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t add_8bit = 0x1A;
      std::uint8_t add_no_8bit = 0x1B;
    
      check_prefix_size(size);
    
      emit_reg_mem_64_8(add_no_8bit, add_8bit, dest, src, disp);
    }
    
    void emit_sub_with_borrow_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t add_8bit = 0x1C;
      std::uint8_t add_no_8bit = 0x1D;
    
      check_prefix_size(size);
    
      emit_logical_accumulator_imm_32_8(add_no_8bit, add_8bit, imm, size);
    }
    
    void emit_logical_and_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t and_8bit = 0x20;
      std::uint8_t and_no_8bit = 0x21;
    
      check_prefix_size(size);
    
      emit_reg_reg_64_8(and_no_8bit, and_8bit, dest, src, size);
    }
    
    void emit_logical_and_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t and_8bit = 0x20;
      std::uint8_t and_no_8bit = 0x21;
    
      check_prefix_size(size);
    
      emit_mem_reg_64_8(and_no_8bit, and_8bit, dest, disp, src, size);
    }
    
    void emit_logical_and_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t and_8bit = 0x22;
      std::uint8_t and_no_8bit = 0x23;
    
      check_prefix_size(size);
    
      emit_reg_mem_64_8(and_no_8bit, and_8bit, dest, src, disp);
    }
    
    void emit_logical_and_accumulator_imm32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t and_8bit = 0x24;
      std::uint8_t and_no_8bit = 0x25;
    
      check_prefix_size(size);
    
      emit_logical_accumulator_imm_32_8(and_no_8bit, and_8bit, imm, size);
    }
    
    void emit_sub_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t sub_8bit = 0x28;
      std::uint8_t sub_no_8bit = 0x29;
    
      check_prefix_size(size);
    
      emit_reg_reg_64_8(sub_no_8bit, sub_8bit, dest, src, size);
    }
    
    void emit_sub_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t sub_8bit = 0x28;
      std::uint8_t sub_no_8bit = 0x29;
    
      check_prefix_size(size);
    
      emit_mem_reg_64_8(sub_no_8bit, sub_8bit, dest, disp, src, size);
    }
    
    void emit_sub_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t sub_8bit = 0x2A;
      std::uint8_t sub_no_8bit = 0x2B;
    
      check_prefix_size(size);
    
      emit_reg_mem_64_8(sub_no_8bit, sub_8bit, dest, src, disp);
    }
    
    void emit_sub_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t sub_8bit = 0x2C;
      std::uint8_t sub_no_8bit = 0x2D;
    
      check_prefix_size(size);
    
      emit_logical_accumulator_imm_32_8(sub_no_8bit, sub_8bit, imm, size);
    }
    
    void emit_xor_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t xor_8bit = 0x30;
      std::uint8_t xor_no_8bit = 0x31;
    
      check_prefix_size(size);
    
      emit_reg_reg_64_8(xor_no_8bit, xor_8bit, dest, src, size);
    }
    
    void emit_xor_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t xor_8bit = 0x30;
      std::uint8_t xor_no_8bit = 0x31;
    
      check_prefix_size(size);
    
      emit_mem_reg_64_8(xor_no_8bit, xor_8bit, dest, disp, src, size);
    }
    
    void emit_xor_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t xor_8bit = 0x32;
      std::uint8_t xor_no_8bit = 0x33;
    
      check_prefix_size(size);
    
      emit_reg_mem_64_8(xor_no_8bit, xor_8bit, dest, src, disp);
    }
    
    void emit_xor_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t xor_8bit = 0x34;
      std::uint8_t xor_no_8bit = 0x35;
    
      check_prefix_size(size);
    
      emit_logical_accumulator_imm_32_8(xor_no_8bit, xor_8bit, imm, size);
    }
    
    void emit_cmp_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t cmp_8bit = 0x38;
      std::uint8_t cmp_no_8bit = 0x39;
    
      check_prefix_size(size);
    
      emit_reg_reg_64_8(cmp_no_8bit, cmp_8bit, dest, src, size);
    }
    
    void emit_cmp_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t cmp_8bit = 0x38;
      std::uint8_t cmp_no_8bit = 0x39;
    
      check_prefix_size(size);
    
      emit_mem_reg_64_8(cmp_no_8bit, cmp_8bit, dest, disp, src, size);
    }
    
    void emit_cmp_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t cmp_8bit = 0x3A;
      std::uint8_t cmp_no_8bit = 0x3B;
    
      check_prefix_size(size);
    
      emit_reg_mem_64_8(cmp_no_8bit, cmp_8bit, dest, src, disp);
    }
    
    void emit_cmp_accumulator_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      std::uint8_t cmp_8bit = 0x3C;
      std::uint8_t cmp_no_8bit = 0x3D;
    
      check_prefix_size(size);
    
      emit_logical_accumulator_imm_32_8(cmp_no_8bit, cmp_8bit, imm, size);
    }
    
    // mov with sign extension 32-bit to 64-bit
    void emit_mov_sign_ext_reg_mem_32_64(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t movsxd = 0x63;
      
      if (size == k8bit || size == k16bit) {
        throw std::invalid_argument("invalid operand size for emit_mov_sign_ext_reg_mem_32_64: " + std::to_string(size));
      }
      
      emit_reg_mem_64_8(movsxd, 0, dest, src, disp, size); 
    }
    
    void emit_mov_sign_ext_reg_reg_32_64(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t movsxd = 0x63;
      
      if (size == k8bit || size == k16bit) {
        throw std::invalid_argument("invalid operand size for emit_mov_sign_ext_reg_mem_32_64: " + std::to_string(size));
      }
      
      emit_reg_reg_64_8(movsxd, 0, dest, src, size); 
    }
    
    // registers can be 16 to 64
    void emit_signed_mul_reg_reg_imm_8_32(const std::string& dest, const std::string& src, std::variant<std::uint64_t, std::int64_t> imm8_32,
                                          const std::size_t& size_reg = k64bit, const std::size_t& size_imm = k32bit) {
      std::uint8_t imul8 = 0x6B;
      std::uint8_t imul16_32 = 0x69;
      
      if (size_imm == k64bit) {
        throw std::invalid_argument("invalid operand size_imm for emit_signed_mul_reg_reg_imm_8_32: " + std::to_string(size_imm));
      }
      
      emit_reg_reg_64_8(imul16_32, imul8, src, dest, size_reg);
      
      emit_imm_by_size(imm8_32, size_imm);
    }
    
    void emit_signed_mul_reg_mem_imm_8_32(const std::string& dest, const std::string& src, std::int64_t disp, std::variant<std::uint64_t, std::int64_t> imm8_32,
                                          const std::size_t& size_reg = k64bit, const std::size_t& size_imm = k32bit) {
      std::uint8_t imul8 = 0x6B;
      std::uint8_t imul16_32 = 0x69;
      
      if (size_imm == k64bit) {
        throw std::invalid_argument("invalid operand size_imm for emit_signed_mul_reg_reg_imm_8_32: " + std::to_string(size_imm));
      }
      
      emit_reg_mem_64_8(imul16_32, imul8, src, dest, disp, size_reg);
      
      emit_imm_by_size(imm8_32, size_imm);
    }
    
    void emit_push_reg_64(const std::string& reg) {
      push_byte(0x50 + x64_register[reg]);
    }
    
    void emit_push_reg_16(const std::string& reg) {
      push_byte(prefix64[k16bit]);
      push_byte(0x50 + x64_register[reg]);
    }
    
    void emit_push_imm_32_8(std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k32bit) {
      if (size == k64bit) {
        throw std::invalid_argument("invalid operand size for push_imm_32_8: " + std::to_string(size));
      }
      
      if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      } 
      
      if (size == k8bit) {
        push_byte(0x6A);
      }
      else {
        push_byte(0x68);
      }
      
      emit_imm_by_size(imm, size);
    }
    
    void emit_pop_reg_64(const std::string& reg) {      
      push_byte(0x58 + x64_register[reg]);
    }
    
    void emit_pop_reg_16(const std::string& reg) {
      push_byte(prefix64[k16bit]);
      push_byte(0x58 + x64_register[reg]);
    }
    
    void emit_short_jump(std::uint8_t opcode, std::uint8_t target_address) {
      std::uint8_t current_address = get_code().size();
      std::uint8_t rel8 = target_address - (current_address + 2);
      
      push_byte(opcode);
      push_byte(rel8);
    }
    
    void emit_near_jump(std::uint8_t opcode, std::uint32_t target_address) {
      std::uint32_t current_address = get_code().size();
      std::int32_t rel32 = target_address - (current_address + 6);
      
      push_bytes({0x0F, opcode});
      emit_imm_by_size(rel32, k32bit);
    }
    
    void emit_jo_near(std::uint32_t target_address) {
      emit_near_jump(0x80, target_address);
    }
    
    void emit_jno_near(std::uint32_t target_address) {
      emit_near_jump(0x81, target_address);
    }
    
    void emit_jb_near(std::uint32_t target_address) {
      emit_near_jump(0x82, target_address);
    }
    
    void emit_jnb_near(std::uint32_t target_address) {
      emit_near_jump(0x83, target_address);
    }
    
    void emit_jz_near(std::uint32_t target_address) {
      emit_near_jump(0x84, target_address);
    }
    
    void emit_jnz_near(std::uint32_t target_address) {
      emit_near_jump(0x85, target_address);
    }
    
    void emit_jbe_near(std::uint32_t target_address) {
      emit_near_jump(0x86, target_address);
    }
    
    void emit_jnbe_near(std::uint32_t target_address) {
      emit_near_jump(0x87, target_address);
    }
    
    void emit_js_near(std::uint32_t target_address) {
      emit_near_jump(0x88, target_address);
    }
    
    void emit_jns_near(std::uint32_t target_address) {
      emit_near_jump(0x89, target_address);
    }
    
    void emit_jp_near(std::uint32_t target_address) {
      emit_near_jump(0x8A, target_address);
    }
    
    void emit_jnp_near(std::uint32_t target_address) {
      emit_near_jump(0x8B, target_address);
    }
    
    void emit_jl_near(std::uint32_t target_address) {
      emit_near_jump(0x8C, target_address);
    }
    
    void emit_jnl_near(std::uint32_t target_address) {
      emit_near_jump(0x8D, target_address);
    }
    
    void emit_jle_near(std::uint32_t target_address) {
      emit_near_jump(0x8E, target_address);
    }
    
    void emit_jnle_near(std::uint32_t target_address) {
      emit_near_jump(0x8F, target_address);
    }
    
    // start short
    
    void emit_jo_short(std::uint8_t target_address) {
      emit_short_jump(0x70, target_address);
    }
    
    void emit_jno_short(std::uint8_t target_address) {
      emit_short_jump(0x71, target_address);
    }
    
    void emit_jb_short(std::uint8_t target_address) {
      emit_short_jump(0x72, target_address);
    }
    
    void emit_jnb_short(std::uint8_t target_address) {
      emit_short_jump(0x73, target_address);
    }
    
    void emit_jz_short(std::uint8_t target_address) {
      emit_short_jump(0x74, target_address);
    }
    
    void emit_jnz_short(std::uint8_t target_address) {
      emit_short_jump(0x75, target_address);
    }
    
    void emit_jbe_short(std::uint8_t target_address) {
      emit_short_jump(0x76, target_address);
    }
    
    void emit_jnbe_short(std::uint8_t target_address) {
      emit_short_jump(0x77, target_address);
    }
    
    void emit_js_short(std::uint8_t target_address) {
      emit_short_jump(0x78, target_address);
    }
    
    void emit_jns_short(std::uint8_t target_address) {
      emit_short_jump(0x79, target_address);
    }
    
    void emit_jp_short(std::uint8_t target_address) {
      emit_short_jump(0x7A, target_address);
    }
    
    void emit_jnp_short(std::uint8_t target_address) {
      emit_short_jump(0x7B, target_address);
    }
    
    void emit_jl_short(std::uint8_t target_address) {
      emit_short_jump(0x7C, target_address);
    }
    
    void emit_jnl_short(std::uint8_t target_address) {
      emit_short_jump(0x7D, target_address);
    }
    
    void emit_jle_short(std::uint8_t target_address) {
      emit_short_jump(0x7E, target_address);
    }
    
    void emit_jnle_short(std::uint8_t target_address) {
      emit_short_jump(0x7F, target_address);
    }
    
    // ADD MEMORY START
    
    //	r/m16/32/64 	imm16/32 	
    void emit_reg_imm16_32(std::uint8_t opcode, rm_field rmf, const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t imm_size = k32bit) {
      push_byte(opcode);
      
      std::uint8_t modrm = modrm_byte(addressing_modes::reg_to_reg, rmf, x64_register[dest]);
      push_byte(modrm);
      
      emit_imm_by_size(imm, imm_size);
    }
    
    //  r/m8 	imm8
    void emit_reg8_imm8(std::uint8_t opcode, rm_field rmf, const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm) {
      push_byte(opcode);
      
      std::uint8_t modrm = modrm_byte(addressing_modes::reg_to_reg, rmf, x64_register[dest]);
      push_byte(modrm);
      
      emit_imm_by_size(imm, k8bit);
    }
    
    //	r/m16/32/64 	imm8
    void emit_reg16_64_imm8(std::uint8_t opcode, rm_field rmf, const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm) {
      push_byte(opcode);
      
      std::uint8_t modrm = modrm_byte(addressing_modes::reg_to_reg, rmf, x64_register[dest]);
      push_byte(modrm);
      
      emit_imm_by_size(imm, k8bit);
    }
    
    void emit_reg8_64_imm8_32(std::uint8_t r8_imm8_op, std::uint8_t r16_64_imm8_op, std::uint8_t r16_64_imm16_32_op, rm_field rmf, const std::string& dest,
                              std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      if (imm_size == k64bit) {
        throw std::invalid_argument("invalid immediate size for emit_reg8_64_imm8_32: " + std::to_string(imm_size));
      }
      
      if (reg_size == k64bit) {
        push_byte(prefix64[reg_size]);
      }
      else if (reg_size == k16bit) {
        push_byte(prefix64[reg_size]);
      }
      else if (reg_size == k64bit_ext_src) {
        push_byte(prefix64[reg_size]);
      }
      else if (reg_size == k64bit_ext_target) {
        push_byte(prefix64[reg_size]);
      }
      
      if (reg_size == k8bit && imm_size == k8bit) {
        emit_reg8_imm8(r8_imm8_op, rmf, dest, imm);
      }
      else if (reg_size != k8bit && imm_size == k8bit) {
        emit_reg16_64_imm8(r16_64_imm8_op, rmf, dest, imm);
      }
      else {
        emit_reg_imm16_32(r16_64_imm16_32_op, rmf, dest, imm);
      }
    }
    
    // add r8/16/32/64, imm8/16/32
    void emit_add_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b000), dest, imm, reg_size, imm_size);
    }
    
    void emit_or_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b001), dest, imm, reg_size, imm_size);
    }
    
    void emit_adc_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b010), dest, imm, reg_size, imm_size);
    }
    
    void emit_sbb_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b011), dest, imm, reg_size, imm_size);
    }
    
    void emit_and_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b100), dest, imm, reg_size, imm_size);
    }
    
    void emit_sub_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b101), dest, imm, reg_size, imm_size);
    }
    
    void emit_xor_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b110), dest, imm, reg_size, imm_size);
    }
    
    void emit_cmp_reg8_64_imm8_32(const std::string& dest, std::variant<std::uint64_t, std::int64_t> imm, std::size_t reg_size = k64bit, std::size_t imm_size = k32bit) {
      std::uint8_t add_r8_imm8 = 0x80;
      std::uint8_t add_r16_64_imm16_32 = 0x81;
      std::uint8_t add_r16_64_imm8 = 0x83;
      
      emit_reg8_64_imm8_32(add_r8_imm8, add_r16_64_imm8, add_r16_64_imm16_32, static_cast<rm_field>(0b111), dest, imm, reg_size, imm_size);
    }
    
    void emit_test_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t test_8bit = 0x84;
      std::uint8_t test = 0x85;
      
      check_prefix_size(size);
      
      emit_reg_reg_64_8(test, test_8bit, dest, src, size);
    }
    
    void emit_test_mem_reg(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t test_8bit = 0x84;
      std::uint8_t test = 0x85;
      
      check_prefix_size(size);
      
      emit_mem_reg_64_8(test, test_8bit, dest, disp, src, size);
    }
    
    void emit_xchg_reg_reg(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) {
      std::uint8_t xchg_8bit = 0x86;
      std::uint8_t xchg = 0x87;
      
      check_prefix_size(size);
      
      emit_reg_reg_64_8(xchg, xchg_8bit, dest, src, size);
    }
    
    void emit_xchg_reg_mem(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) {
      std::uint8_t xchg_8bit = 0x86;
      std::uint8_t xchg = 0x87;
      
      check_prefix_size(size);
      
      emit_reg_mem_64_8(xchg, xchg_8bit, dest, src, disp, size);
    }
    
    // ADD MEMORY END 
    
    void emit_long_jump(std::uint8_t opcode, std::int32_t target_address) {
      std::size_t current_address = get_code().size(); 
      std::int32_t displacement = target_address - (current_address + 5); 
    
      push_byte(opcode);            
      emit_imm32(displacement); 
    }
    
    void emit_call(std::int32_t addr) {
      emit_long_jump(0xE8, addr);
    }
    
    void emit_jmp(std::int32_t addr) {
      emit_long_jump(0xE9, addr);
    }
    
    void emit_short_jmp(std::uint8_t addr) {
      emit_short_jump(0xEB, addr);
    }
    
    void emit_function_prologue(std::int64_t stack_size) {
      emit_push_reg_64("rbp");
      emit_mov_reg_reg("rbp", "rsp");
      emit_sub_reg8_64_imm8_32("rsp", stack_size);
    }
    
    void emit_function_epilogue() {
      emit_mov_reg_reg("rsp", "rbp");
      emit_pop_reg_64("rbp");
    }
    
    void emit_call_reg64(const std::string& dest) {
      push_byte(0xFF);
      
      push_byte(modrm_byte(addressing_modes::direct, mem_disp32, x64_register[dest]));
    }
    
    void emit_inc_reg(const std::string& dest, const std::size_t& size = k64bit) {
      std::uint8_t inc_8bit = 0xFE;
      std::uint8_t inc = 0xFF;
      
      std::uint8_t modrm = modrm_byte(reg_to_reg, reg, x64_register[dest]); // 00 is for inc

      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(inc_8bit);
      }
      else {
        push_byte(inc);
      }
      
      push_byte(modrm);
    }
    
    void emit_dec_reg(const std::string& dest, const std::size_t& size = k64bit) {
      std::uint8_t dec_8bit = 0xFE;
      std::uint8_t dec = 0xFF;
      
      std::uint8_t modrm = modrm_byte(reg_to_reg, reg, x64_register[dest]); // 01 is for dec
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      else if (size == k16bit) {
        push_byte(prefix64[k16bit]);
      }
      else if (size == k64bit_ext_src) {
        push_byte(prefix64[k64bit_ext_src]);
      }
      else if (size == k64bit_ext_target) {
        push_byte(prefix64[k64bit_ext_target]);
      }
      
      if (size == k8bit) {
        push_byte(dec_8bit);
      }
      else {
        push_byte(dec);
      }
      
      push_byte(modrm);
    }
    
    //x87 fpu here
  };
} // namespace occult
