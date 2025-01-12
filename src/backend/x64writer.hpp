#pragma once
#include "writer.hpp"

namespace occult {
  std::unordered_map<std::string, std::uint8_t> prefix64 = {
    {"64", 0x48},
    {"32", 0x00},
  }; // prefixes for sizes when doing mov
  
  std::unordered_map<std::string, std::uint8_t> x64_register = {
    {"rax", 0x00},
    {"rcx", 0x01},
    {"rdx", 0x02},
    {"rbx", 0x03},
    {"rsp", 0x04},
    {"rip", 0x05},
    {"rsi", 0x06},
    {"rdi", 0x07}
  };
  
  struct x64writer : writer {
    x64writer(const std::size_t& size) : writer(size) {}
    
    void ret() { push_byte(0xC3); }
    
    void syscall() { push_bytes({0x0F, 0x05}); }
    
    // size is 32-64 bit
    void mov_reg_imm(const std::string& size, const std::string& reg, std::int64_t imm) {
      if (prefix64.find(size) == prefix64.end()) {
        throw std::invalid_argument("invalid operand size: " + size);
      }
      
      if (size == "64") {
        push_byte(prefix64["64"]);
        push_byte(0xC7);  // mov imm/r64 ??
        push_byte(0xC0 + x64_register[reg]);  // modrm (register) 
        
        for (int i = 0; i < 8; ++i) {
          push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
        }
      }
      else if (size == "32") {
        push_byte(prefix64["64"]); // for 64 bit
        push_byte(0xC7); 
        push_byte(0xC0 + x64_register[reg]);
        
        for (int i = 0; i < 4; ++i) {
          push_byte(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
        }
      }
    }
    
    // x64 rip (instruction pointer)
    void lea_from_rip(const std::int32_t& disp) {
      push_byte(prefix64["64"]); 
      push_byte(0x8D);  // lea
      
      push_byte(0x35);  // rip addressing mode
      
      for (int i = 0; i < 4; ++i) {
        push_byte(static_cast<uint8_t>((disp >> (i * 8)) & 0xFF));
      }
    }
    
    void push_reg(const std::string& reg) {
      push_byte(0x50 + x64_register[reg]);
    }
    
    void pop_reg(const std::string& reg) {
      push_byte(0x58 + x64_register[reg]);
    }
  };
} // namespace occult
