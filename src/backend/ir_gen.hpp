#pragma once
#include "../parser/ast.hpp"

namespace occult {
  enum ir_opcode {
    op_push,
    op_pop,
    op_store,
    op_load,
    op_add,
    op_div,
    op_sub,
    op_mul,
    op_jmp,
    op_jz,//je
    op_jnz,//jne
    op_jl,
    op_jle,
    op_jg,
    op_jge,
    op_cmp,
    op_ret,
    op_call,
    op_syscall
  };
  
  struct ir_argument {
    std::string name;
    std::string type;
    
    ir_argument(std::string name, std::string type) : name(name), type(type) {}
  };
  
  struct ir_function {
    std::vector<ir_opcode> code;
    std::vector<ir_argument> args;
    std::string name;
    std::string type;
  };
  
  class ir_gen { // conversion into a linear IR
    ast_root* root;
    
    ir_function generate_function(ast_function* func_node);
    void generate_function_args(ir_function& function, ast_functionargs* func_args_node);
    void generate_block(ir_function& function, ast_block* block_node);
  public:
    ir_gen(ast_root* root) : root(root) {}
    
    std::vector<ir_function> generate();
  };
} // namespace occult
