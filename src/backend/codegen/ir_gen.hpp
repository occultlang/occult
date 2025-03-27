#pragma once
#include "../../parser/ast.hpp"

namespace occult {
  enum ir_opcode {
    op_push,
    op_pop,
    op_store,
    op_load,
    op_add,
    op_div,
    op_mod,
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
    op_syscall,
    op_fadd,
    op_fdiv,
    op_fsub,
    op_fmul,
    op_fmod,
    label,
  };
  
  inline std::string opcode_to_string(ir_opcode op) {
    switch (op) {
      case op_push:    return "op_push";
      case op_pop:     return "op_pop";
      case op_mod:     return "op_mod";
      case op_store:   return "op_store";
      case op_load:    return "op_load";
      case op_add:     return "op_add";
      case op_div:     return "op_div";
      case op_sub:     return "op_sub";
      case op_mul:     return "op_mul";
      case op_fadd:    return "op_fadd";
      case op_fdiv:    return "op_fdiv";
      case op_fsub:    return "op_fsub";
      case op_fmul:    return "op_fmul";
      case op_fmod:    return "op_fmod";
      case op_jmp:     return "op_jmp";
      case op_jz:      return "op_jz";
      case op_jnz:     return "op_jnz";
      case op_jl:      return "op_jl";
      case op_jle:     return "op_jle";
      case op_jg:      return "op_jg";
      case op_jge:     return "op_jge";
      case op_cmp:     return "op_cmp";
      case op_ret:     return "op_ret";
      case op_call:    return "op_call";
      case op_syscall: return "op_syscall"; 
      case label:      return "label";
      default:         return "unknown_opcode";
    }
  }
  
  struct ir_argument {
    std::string name;
    std::string type;
    
    ir_argument(std::string name, std::string type) : name(name), type(type) {}
  };
  
  using ir_operand = std::variant<std::monostate, std::int64_t, std::uint64_t, double, std::string>;
  
  struct ir_instr { 
    ir_opcode op;
    ir_operand operand;
    std::string type;
    
    ir_instr(ir_opcode op, ir_operand operand) : op(op), operand(operand), type("") {}
    ir_instr(ir_opcode op, ir_operand operand, std::string type) : op(op), operand(operand), type(type) {}
    ir_instr(ir_opcode op) : op(op), operand(std::monostate()) {}
  };
  
  struct ir_function {
    std::vector<ir_instr> code;
    std::vector<ir_argument> args;
    std::string name;
    std::string type;
  };
  
  enum ir_typename {
    signed_int,
    unsigned_int,
    floating_point,
    string
  };
  
  const std::unordered_map<std::string, ir_typename> ir_typemap = {
    {"int64", signed_int},
    {"int32", signed_int},
    {"int16", signed_int},
    {"int8", signed_int},
    {"uint64", unsigned_int},
    {"uint32", unsigned_int},
    {"uint16", unsigned_int},
    {"uint8", unsigned_int},
    {"float32", floating_point},
    {"float64", floating_point},
    {"bool", unsigned_int}, 
    {"char", unsigned_int}, 
    {"str", string}};
  
  class ir_gen { // conversion into a linear IR
    ast_root* root;
    int label_count;
    std::stack<std::string> label_stack;
    std::unordered_map<std::string, int> label_map;
    
    ir_function generate_function(ast_function* func_node);
    void generate_function_args(ir_function& function, ast_functionargs* func_args_node);
    void generate_arith_and_bitwise_operators(ir_function& function, ast* c);
    template<typename IntType>
    void generate_common(ir_function& function, ast* assignment_node);
    void handle_push_types(ir_function& function, ast* c);
    void handle_push_types_common(ir_function& function, ast* c);
    void generate_function_call(ir_function& function, ast* c);
    void generate_return(ir_function& function, ast_returnstmt* return_node);
    void generate_if(ir_function& function, ast_ifstmt* if_node, std::string current_break_label = "", std::string current_loop_start = "");
    void generate_loop(ir_function& function, ast_loopstmt* loop_node);
    void generate_while(ir_function& function, ast_whilestmt* while_node);
    void generate_block(ir_function& function, ast_block* block_node, std::string current_break_label = "", std::string current_loop_start = "");
    std::string create_label();
    void place_label(ir_function& function, std::string label_name);
  public:
    ir_gen(ast_root* root) : root(root), label_count(0) {}
    void visualize(std::vector<ir_function> funcs);
    std::vector<ir_function> lower();
  };
} // namespace occult
