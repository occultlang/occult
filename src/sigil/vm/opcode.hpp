#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <iostream>

namespace sigil {
  constexpr std::int64_t inplace = 0;
  
  enum opcode : std::uint8_t {
    push,
    pop,
    load,
    store,
    fadd,
    fsub, 
    fmul,  
    fdiv, 
    fmod,
    add,
    sub, 
    mul, 
    div,
    mod,
    cout,
    call,
    ret,
    jmp,
    jne,
    je,
    jge,
    jle,
    jg,
    jl,
    jz,
    jnz,
    jg_eq,
    jl_eq,
  };
  
  using register_t = std::variant<std::int64_t, double, std::string>; // will add rest of types later on
  
  const std::unordered_map<opcode, std::string> opcode_names = {
    {opcode::push, "push"},
    {opcode::pop, "pop"},
    {opcode::load, "load"},
    {opcode::store, "store"},
    {opcode::fadd, "fadd"},
    {opcode::fsub, "fsub"},
    {opcode::fmul, "fmul"},
    {opcode::fdiv, "fdiv"},
    {opcode::fmod, "fmod"},
    {opcode::add, "add"},
    {opcode::sub, "sub"},
    {opcode::mul, "mul"},
    {opcode::div, "div"},
    {opcode::mod, "mod"},
    {opcode::cout, "cout"},
    {opcode::call, "call"},
    {opcode::ret, "ret"},
    {opcode::jmp, "jmp"},
    {opcode::jne, "jne"},
    {opcode::je, "je"},
    {opcode::jge, "jge"},
    {opcode::jle, "jle"},
    {opcode::jg, "jg"},
    {opcode::jl, "jl"},
    {opcode::jz, "jz"},
    {opcode::jnz, "jnz"},
    {opcode::jg_eq, "jg_eq"},
    {opcode::jl_eq, "jl_eq"}
  };
  
  static std::int64_t main_location = 0;
  
  struct instruction_t {
    opcode op;
    register_t operand;
    
    void print() {
      auto it = opcode_names.find(op);
      std::string op_name = it->second;
    
      std::cout << op_name;
    
      std::visit([](const auto& value) {
        std::cout << " " << value;
      }, operand);
    
      std::cout << std::endl;
    }
    
    instruction_t(opcode op, register_t operand) : op(op), operand(operand) {}
  };
} // namespace sigil
