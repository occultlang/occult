#pragma once
#include "x86_64_writer.hpp"
#include "ir_lifter.hpp"

namespace occult::x86_64 {
    class codegen_v2 {    
        class register_allocator {  
            std::unordered_map<grp, bool> register_liveliness = {
                {rax, false},
                {rcx, false},
                {rdx, false},
                {rbx, false},
                {rsi, false},
                {rdi, false},
                {r8,  false},
                {r9,  false},
                {r10, false},
                {r11, false},
                {r12, false},
                {r13, false},
                {r14, false},
                {r15, false},
            };

            const std::vector<grp> allocation_order = {
                rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11,
                rbx, r12, r13, r14, r15 
            };

            public:

            register_allocator() = default;

            // returns true if reg is live
            bool check(grp reg) const {
                auto it = register_liveliness.find(reg);

                return it != register_liveliness.end() && it->second;
            }

            grp next() {
                for (grp reg : allocation_order) {
                    if (!check(reg)) {
                        return reg;
                    }
                }
                
                throw std::runtime_error("no free registers in allocator");
            }

            void allocate(grp reg) {
                register_liveliness[reg] = true;
            }

            grp allocate_next() {
                grp r = next();         

                allocate(r);

                return r;
            }

            void free(grp reg) {
                register_liveliness[reg] = false;
            }

            bool try_allocate(grp reg) { 
                if (check(reg)) { 
                    return false;
                }
                
                allocate(reg);
                
                return true;
            }
        };

        std::unique_ptr<x86_64_writer> w;
        std::vector<rir_function> reg_ir; 
        std::vector<ir_struct> ir_structs;

        void compile_function(rir_function& func) {

        }

        public:

        codegen_v2(const std::vector<rir_function>& reg_ir, const std::vector<ir_struct>& ir_structs, const bool debug = false) : reg_ir(reg_ir), ir_structs(ir_structs) {} 

        void compile()
    };
} // namespace occult::x86_64
