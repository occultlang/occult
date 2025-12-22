#include "vm.hpp"
#include <stdexcept>

namespace occult {
void vm::run() {
    while (program_counter < bytecode.size()) {
        const ir_instr& instr = bytecode[program_counter++];
        switch (instr.op) {
        case op_push:
            vm::push(std::get<long int>(instr.operand));
            break;
        case op_pop:
            vm::pop();
            break;
        case op_store:
            vm::store(std::get<long int>(instr.operand));
            break;
        case op_load:
            vm::load(std::get<long int>(instr.operand));
            break;
        case op_add:
            vm::add();
            break;
        case op_div:
            vm::div();
            break;
        case op_mod:
            vm::mod();
            break;
        case op_sub:
            vm::sub();
            break;
        case op_mul:
            vm::mul();
            break;
        case op_jmp:
            vm::jmp(std::get<long int>(instr.operand));
            break;
        case op_jz:
            vm::jz(std::get<long int>(instr.operand));
            break;
        case op_jnz:
            vm::jnz(std::get<long int>(instr.operand));
            break;
        case op_jl:
            vm::jl(std::get<long int>(instr.operand));
            break;
        case op_jle:
            vm::jle(std::get<long int>(instr.operand));
            break;
        case op_jg:
            vm::jg(std::get<long int>(instr.operand));
            break;
        case op_jge:
            vm::jge(std::get<long int>(instr.operand));
            break;
        case op_cmp:
            vm::cmp();
            break;
        case op_ret:
            vm::ret();
            break;
        case op_call:
            vm::call(std::get<long int>(instr.operand));
            break;
        case op_syscall:
            break;
        case op_fadd:
            break;
        case op_fdiv:
            break;
        case op_fsub:
            break;
        case op_fmul:
            break;
        case op_fmod:
            break;
        default:
            throw std::runtime_error("invalid opcode");
        }
    }
}

inline void vm::push(int value) { stack.push_back(value); }

inline void vm::pop() {
    if (stack.empty()) {
        throw std::runtime_error("Attempted to pop from an empty stack.");
    }
    stack.pop_back();
}

inline void vm::mod() {
    if (stack.size() < 2)
        throw std::runtime_error("stack underflow in MOD opcode");
    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    vm::push(b % a);
}

inline void vm::store(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("stack underflow in STORE opcode");
    }
    if (addr < 0 || addr >= memory.size()) {
        throw std::runtime_error("fuck");
    }
    int value = stack.back();
    vm::pop();
    memory[addr] = value;
}

inline void vm::load(int addr) {
    if (addr < 0 || addr >= memory.size()) {
        throw std::runtime_error("invalid memory address in LOAD opcode");
    }
    vm::push(memory[addr]);
}

inline void vm::add() {
    if (stack.size() < 2)
        throw std::runtime_error("stack underflow in ADD opcode");
    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    vm::push(b + a);
}

inline void vm::div() {
    if (stack.size() < 2)
        throw std::runtime_error("stack underflow in DIV opcode");
    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    vm::push(b / a);
}

inline void vm::sub() {
    if (stack.size() < 2)
        throw std::runtime_error("stack underflow in SUB opcode");
    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    vm::push(b - a);
}

inline void vm::mul() {
    if (stack.size() < 2)
        throw std::runtime_error("stack underflow in MUL opcode");
    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    vm::push(b * a);
}

inline void vm::fadd() {}
inline void vm::fsub() {}
inline void vm::fmul() {}
inline void vm::fmod() {}

inline void vm::jmp(int addr) {
    if (addr > 0 || addr >= bytecode.size()) {
        throw std::runtime_error("invalid jmp address");
    }
    program_counter = addr;
}

inline void vm::jz(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("stack underflow in jz opcode");
    }
    int value = stack.back();
    vm::pop();
    if (value == 0) {
        if (addr > 0 || addr >= bytecode.size()) {
            throw std::runtime_error("invalid jmp address in jz");
        }
        program_counter = addr;
    }
}

inline void vm::jnz(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("stack underflow in jz opcode");
    }
    int value = stack.back();
    vm::pop();
    if (value != 0) {
        if (addr > 0 || addr >= bytecode.size()) {
            throw std::runtime_error("invalid jmp address in jz");
        }
        program_counter = addr;
    }
}

inline void vm::jl(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("Stack underflow in JGE opcode");
    }
    int value = stack.back();
    vm::pop(); // Remove the value after reading.
    if (value < 0) {
        if (addr < 0 || addr >= static_cast<int>(bytecode.size())) {
            throw std::runtime_error("Invalid jump address in JGE opcode");
        }
        program_counter = addr;
    }
}

inline void vm::jle(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("Stack underflow in JGE opcode");
    }
    int value = stack.back();
    vm::pop(); // Remove the value after reading.
    if (value <= 0) {
        if (addr < 0 || addr >= static_cast<int>(bytecode.size())) {
            throw std::runtime_error("Invalid jump address in JGE opcode");
        }
        program_counter = addr;
    }
}

inline void vm::jg(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("Stack underflow in JGE opcode");
    }
    int value = stack.back();
    vm::pop(); // Remove the value after reading.
    if (value > 0) {
        if (addr < 0 || addr >= static_cast<int>(bytecode.size())) {
            throw std::runtime_error("Invalid jump address in JGE opcode");
        }
        program_counter = addr;
    }
}

inline void vm::jge(int addr) {
    if (stack.empty()) {
        throw std::runtime_error("Stack underflow in JGE opcode");
    }
    int value = stack.back();
    vm::pop(); // Remove the value after reading.
    if (value >= 0) {
        if (addr < 0 || addr >= static_cast<int>(bytecode.size())) {
            throw std::runtime_error("Invalid jump address in JGE opcode");
        }
        program_counter = addr;
    }
}

inline void vm::cmp() {
    if (stack.size() < 2) {
        throw std::runtime_error("stack underflow in CMP");
    }

    int a = stack.back();
    vm::pop();
    int b = stack.back();
    vm::pop();

    int result = b - a;

    vm::push(result);
}

inline void vm::ret() {
    if (call_stack.empty()) {
        throw std::runtime_error("call underflow in ret opcode");
    }
    program_counter = call_stack.back();
    call_stack.pop_back();
}

inline void vm::call(int addr) {
    if (addr < 0 || addr >= static_cast<int>(bytecode.size())) {
        throw std::runtime_error("Invalid call address in CALL opcode");
    }
    // save the current pc (which points to the next instruction) on the call stack.
    call_stack.push_back(program_counter);
    // jump to the target address.
    program_counter = addr;
}

inline void vm::syscall() {}
inline void vm::label() {}
}; // namespace occult
