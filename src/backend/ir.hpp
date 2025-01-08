#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <iostream>

namespace occult {
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
    cmp,
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
    label,
  };
  
  using register_t = std::variant<std::int64_t, std::uint64_t, double, std::string>; 
  
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
    {opcode::jl_eq, "jl_eq"},
    {opcode::cmp, "cmp"},
    {opcode::label, "label"}
  };
  
  static std::int64_t main_location = 0;
  
  struct instruction_t {
    opcode op;
    register_t operand;
    
    void print() {
      auto it = opcode_names.find(op);
      std::string op_name = it->second;
    
      std::cout << op_name;
      
      if ((op == opcode::store ||
           op == opcode::load  ||
           op == opcode::label ||
           op == opcode::jmp   ||
           op == opcode::jne   ||
           op == opcode::je    ||
           op == opcode::jge   ||
           op == opcode::jle   ||
           op == opcode::jg    ||
           op == opcode::jl    ||
           op == opcode::jz    ||
           op == opcode::jnz   ||
           op == opcode::jg_eq ||
           op == opcode::jl_eq) && std::holds_alternative<std::string>(operand)) {
        std::cout << " " << std::get<std::string>(operand); 
      }
      else {
        std::visit([](const auto& value) {
          if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
            std::cout << " \"" << value << "\"";
          }
          else {
            std::cout << " " << value;
          }
        }, operand);
      }
    
      std::cout << std::endl;
    }
    
    instruction_t(opcode op, register_t operand) : op(op), operand(operand) {}
  };
} // namespace occult
