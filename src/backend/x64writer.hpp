#pragma once
#include "writer.hpp"

// witchcraft at its finest... - anthony
// a lot of this is with http://ref.x86asm.net/coder64.html & https://defuse.ca/online-x86-assembler.htm
// quick note right now, memory support may be limited as i have not really dealt with SIB yet, so bear with me please... (no rsp support yet)

namespace occult {
  std::unordered_map<std::size_t, std::uint8_t> prefix64 = {
    {64, 0x48},
    {32, 0x00},
    {16, 0x66}, // ?  
    {8, 0x66}  // ?
  }; // prefixes for sizes
  
  std::unordered_map<std::string, std::uint8_t> x64_register = {
    {"rax", 0x00},
    {"rcx", 0x01},
    {"rdx", 0x02},
    {"rbx", 0x03},
    {"rsp", 0x04},
    {"rbp", 0x05},
    {"rsi", 0x06},
    {"rdi", 0x07} // don't have the other ones r8-15
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
  
  constexpr std::size_t k64bit = 64;
  constexpr std::size_t k32bit = 32;
  
  struct x64writer : writer {
    x64writer(const std::size_t& size) : writer(size) {}
    
    void emit_ret() { push_byte(0xC3); }
    
    void emit_syscall() { push_bytes({0x0F, 0x05}); }
    
    void emit_imm32(std::int32_t imm) {
      for (int i = 0; i < 4; ++i) {
        push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
      }
    }
    
    void emit_imm64(std::int64_t imm) {
      for (int i = 0; i < 8; ++i) {
        push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
      }
    }
    
    void check_prefix_size(const std::size_t& size) {
      if (prefix64.find(size) == prefix64.end()) {
        throw std::invalid_argument("invalid operand size: " + std::to_string(size));
      }
    }
    
    void emit_unsigned_imm64(std::uint64_t imm) {
      for (int i = 0; i < 8; ++i) {
        push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
      }
    }
    
    void emit_unsigned_imm32(std::uint32_t imm) {
      for (int i = 0; i < 4; ++i) {
        push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
      }
    }
    
    // move register to register
    void emit_mov_r_r(const std::string& dest, const std::string& src, const std::size_t& size = k64bit) { // mov reg, reg
      std::uint8_t mov = 0x89; // MOV 	r/m16/32/64 	r16/32/64
      
      check_prefix_size(size);
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      
      std::uint8_t modrm = modrm_byte(addressing_modes::reg_to_reg, x64_register[dest], x64_register[src]);
      
      push_byte(mov);
      push_byte(modrm);
    }
    
    // move memory to register
    void emit_mov_r_m(const std::string& dest, const std::string& src, std::int64_t disp, const std::size_t& size = k64bit) { // mov reg, [disp/register + disp]
      std::uint8_t mov = 0x8B; // 	MOV 	r16/32/64 	r/m16/32/64 	
      std::uint8_t modrm = 0;
      
      check_prefix_size(size);
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      
      push_byte(mov);
      
      if (disp != 0) { // reg + disp
        if (src == "rip") { // rip-relative addressing is an alternate addressing mode? 
          modrm = modrm_byte(addressing_modes::direct, x64_register[dest], rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          emit_imm32(disp);
        }
        else {
          modrm = modrm_byte(addressing_modes::disp32, x64_register[dest], x64_register[src]);
          
          push_byte(modrm);
          emit_imm32(disp);
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, x64_register[dest], x64_register[src]);
        
        push_byte(modrm);
      }
    }
    
    // move register to memory
    void emit_mov_m_r(const std::string& dest, std::int64_t disp, const std::string& src, const std::size_t& size = k64bit) { // mov [disp/register + disp], reg
      std::uint8_t mov = 0x89; // MOV 	r/m16/32/64 	r16/32/64
      std::uint8_t modrm = 0;
      
      check_prefix_size(size);
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      
      push_byte(mov);
      
      if (disp != 0) { // reg + disp
        if (dest == "rip") {
          modrm = modrm_byte(addressing_modes::direct, x64_register[src], rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          emit_imm32(disp);
        }
        else {
          modrm = modrm_byte(addressing_modes::disp32, x64_register[src], x64_register[dest]);
          
          push_byte(modrm);
          emit_imm32(disp);
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, x64_register[src], x64_register[dest]);
        
        push_byte(modrm);
      }
    }
    
    // move immediate to memory
    void emit_mov_m_imm(const std::string& dest, std::int64_t disp, std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k64bit) { // mov [disp/register + disp], imm
      std::uint8_t mov = 0xC7;
      std::uint8_t modrm = 0;
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      
      push_byte(mov);
      
      if (disp != 0) { // reg + disp
        if (dest == "rip") {
          modrm = modrm_byte(addressing_modes::direct, no_register, rm_field::alt_mem_disp32); 
          
          push_byte(modrm);
          emit_imm32(disp);
        }
        else {
          modrm = modrm_byte(addressing_modes::disp32, no_register, x64_register[dest]);
          
          push_byte(modrm);
          emit_imm32(disp);
        }
      }
      else if (disp == 0) { // reg
        modrm = modrm_byte(addressing_modes::direct, no_register, x64_register[dest]);
        
        push_byte(modrm);
      }
      
      if (std::holds_alternative<std::uint64_t>(imm)) {
        emit_unsigned_imm64(std::get<std::uint64_t>(imm));
      }
      else if (std::holds_alternative<std::int64_t>(imm)) {
        emit_imm64(std::get<std::int64_t>(imm));
      }
    }
    
    // move immediate to register
    void emit_mov_r_imm(const std::string& reg, std::variant<std::uint64_t, std::int64_t> imm, const std::size_t& size = k64bit) { // mov reg, imm
      std::uint8_t mov32 = 0xB8;
      
      check_prefix_size(size);
      
      if (size == k64bit) {
        push_byte(prefix64[k64bit]);
      }
      
      push_byte(mov32 + x64_register[reg]);

      if (std::holds_alternative<std::uint64_t>(imm)) {
        emit_unsigned_imm64(std::get<std::uint64_t>(imm));
      }
      else if (std::holds_alternative<std::int64_t>(imm)) {
        emit_imm64(std::get<std::int64_t>(imm));
      }
    }
    
    void push(const std::string& reg) {
      push_byte(0x50 + x64_register[reg]);
    }
    
    void pop(const std::string& reg) {
      push_byte(0x58 + x64_register[reg]);
    }
  };
} // namespace occult
