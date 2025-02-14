#pragma once

/*
* unfortunately, some actual foul shit is about to happen.
* anyone thats looking at this source, i apologize for whats about to happen.
*/
#include "../ir_gen.hpp"

namespace occult {
    class vm {
        public:
            void run();
        private:
            size_t program_counter = 0;
            std::vector<int> stack;
            std::vector<ir_instr> bytecode;
            std::vector<int> memory;        
            std::vector<int> call_stack; // lord have mercy    

            void push(int value);
            void pop();
            void mod();
            void store(int addr);
            void load(int addr);
            void add();
            void div();
            void sub();
            void mul();
            void fadd();
            void fsub();
            void fmul();
            void fmod();
            void jmp(int addr);
            void jz(int addr); // haha jizz
            void jnz(int addr);
            void jl(int addr);
            void jle(int addr);
            void jg(int addr);
            void jge(int addr);
            void cmp();
            void ret();
            void call(int addr);
            void syscall();
            void label();
    };
}; 
