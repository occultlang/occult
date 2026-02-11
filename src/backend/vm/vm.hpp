#pragma once

/*
 * unfortunately, some actual foul shit is about to happen.
 * anyone thats looking at this source, i apologize for whats about to happen.
 */
#include "../ir_gen.hpp"

namespace occult {
class vm {
public:
  vm(std::vector<ir_instr> &program, size_t memory_size = 1024)
      : program_counter(0), bytecode(program), memory(memory_size, 0) {
    stack.reserve(128);
    call_stack.reserve(64);
  }

  void run();

private:
  size_t program_counter = 0;
  std::vector<int> stack;
  std::vector<ir_instr> bytecode;
  std::vector<int> memory;
  std::vector<int> call_stack; // lord have mercy

  inline void push(int value);

  inline void pop();

  inline void mod();

  inline void store(int addr);

  inline void load(int addr);

  inline void add();

  inline void div();

  inline void sub();

  inline void mul();

  inline void fadd();

  inline void fsub();

  inline void fmul();

  inline void fmod();

  inline void jmp(int addr);

  inline void jz(int addr); // haha jizz
  inline void jnz(int addr);

  inline void jl(int addr);

  inline void jle(int addr);

  inline void jg(int addr);

  inline void jge(int addr);

  inline void cmp();

  inline void ret();

  inline void call(int addr);

  inline void syscall();

  inline void label();
};
}; // namespace occult
