#pragma once
#include "../../parser/cst.hpp"

// write assignment for booleans (comparison w OR, and AND)

namespace occult {
  enum ir_opcode {
    null_op,
    op_push,
    op_pushf,
    op_pop,
    op_store,
    op_load,
    op_storef,
    op_loadf,
    op_add,
    op_div,
    op_mod,
    op_sub,
    op_mul,
    op_logical_and,
    op_logical_or,
    op_bitwise_and,
    op_bitwise_or,
    op_bitwise_xor,
    op_bitwise_not,
    op_not,
    op_bitwise_lshift,
    op_bitwise_rshift,
    op_negate,
    op_jmp,
    op_jz,//je
    op_jnz,//jne
    op_jl,
    op_jle,
    op_jg,
    op_jge,
    op_setz,
    op_setnz,
    op_setl,
    op_setle,
    op_setg,
    op_setge,
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
    op_array_decl,
    op_array_access_element,
    op_array_store_element,
    op_declare_where_to_store,
    op_array_dimensions,
    op_array_size,
    op_decl_array_type,
    op_mov,
    op_test,
    op_reference,
    op_dereference,
    op_dereference_assign,
    op_store_at_addr
  };
  
  inline std::string opcode_to_string(ir_opcode op) {
    switch (op) {
      case op_push:         return "push";
      case op_pushf:        return "pushf";
      case op_pop:          return "pop";
      case op_store:        return "store";
      case op_load:         return "load";
      case op_storef:       return "storef";
      case op_loadf:        return "loadf";
      case op_add:          return "add";
      case op_div:          return "div";
      case op_mod:          return "mod";
      case op_sub:          return "sub";
      case op_mul:          return "mul";
      case op_logical_and:  return "logical_and";
      case op_logical_or:   return "logical_or";
      case op_bitwise_and:  return "bitwise_and";
      case op_bitwise_or:   return "bitwise_or";
      case op_bitwise_xor:  return "bitwise_xor";
      case op_bitwise_not:  return "bitwise_not";
      case op_not:          return "not";
      case op_bitwise_lshift: return "bitwise_lshift";
      case op_bitwise_rshift: return "bitwise_rshift";
      case op_negate:       return "negate";
      case op_jmp:          return "jmp";
      case op_jz:           return "jz";
      case op_jnz:          return "jnz";
      case op_jl:           return "jl";
      case op_jle:          return "jle";
      case op_jg:           return "jg";
      case op_jge:          return "jge";
      case op_setz:         return "setz";
      case op_setnz:        return "setnz";
      case op_setl:         return "setl";
      case op_setle:        return "setle";
      case op_setg:         return "setg";
      case op_setge:        return "setge";
      case op_cmp:          return "cmp";
      case op_ret:          return "ret";
      case op_call:         return "call";
      case op_syscall:      return "syscall";
      case op_fadd:         return "fadd";
      case op_fdiv:         return "fdiv";
      case op_fsub:         return "fsub";
      case op_fmul:         return "fmul";
      case op_fmod:         return "fmod";
      case label:           return "label";
      case op_array_decl:   return "array_decl";
      case op_array_access_element: return "array_access_elem";
      case op_array_store_element: return "array_store_element";
      case op_array_dimensions: return "array_dimensions";
      case op_array_size:   return "array_size";
      case op_decl_array_type: return "decl_array_type";
      case op_declare_where_to_store: return "declare_where_to_store";
      case op_mov: return "mov";
      case op_test: return "test";
      case op_reference: return "reference";
      case op_dereference: return "dereference";
      case op_dereference_assign: return "dereference_assign";
      case op_store_at_addr: return "store_at_addr";
      default:              return "unknown_opcode";
    }
  }
  
  struct ir_argument {
    std::string name;
    std::string type;
    
    ir_argument(std::string name, std::string type) : name(name), type(type) {}
  };
  
  using ir_operand = std::variant<std::monostate, std::int64_t, std::uint64_t, double, float, std::string>;
  
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

  enum class ir_register : std::uint8_t {
    r0, r1, r2, r3,
    r4, r5, r6, r7, 
    r8, r9, r10, r11, 
    r12, r13, r14, r15
  }; // 16 registers

  using reg_ir_operand = std::variant<std::monostate, std::int64_t, std::uint64_t, double, float, std::string, ir_register>;

  struct ir_reg_instr {
    ir_opcode op;
    reg_ir_operand dest;
    reg_ir_operand src;
    std::string type;

    ir_reg_instr() = default;
  };

  struct ir_reg_function {
    std::vector<ir_reg_instr> code;
    std::vector<ir_argument> args;
    std::string name;
    std::string type;
  };
  
  enum ir_typename {
    signed_int,
    unsigned_int,
    floating_point32,
    floating_point64,
    string
  };
  
  struct visitor_stack {
    void operator()(const float& v){ std::cout << v << "\n"; };
    void operator()(const double& v){ std::cout << v << "\n"; };
    void operator()(const std::int64_t& v){ std::cout << v << "\n"; };
    void operator()(const std::uint64_t& v){ std::cout << v << "\n"; };
    void operator()(const std::string& v){ std::cout << v << "\n"; };
    void operator()(std::monostate){ std::cout << "\n"; };
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
    {"float32", floating_point32},
    {"float64", floating_point64},
    {"bool", unsigned_int}, 
    {"char", unsigned_int}, 
    {"str", string}}; 

  enum class reg_usage : std::uint8_t {
      not_in_use,
      in_use
  }; // just did an enum class because its more readable than true or false  
  
  class ir_register_pool {
    std::array<reg_usage, 16> pool; // registers in use (index corresponds with the register)
  public:
    ir_register_pool() { pool.fill(reg_usage::not_in_use); }
    
    ir_register allocate() {
      for (std::size_t i = 0; i < pool.size(); i++) {
        if (pool.at(i) == reg_usage::not_in_use) {
          pool.at(i) = reg_usage::in_use;

          return static_cast<ir_register>(i);
        }
      }
      
      throw std::runtime_error("all virtual registers are in use");
    }

    void free(const ir_register& r) {
      pool.at(static_cast<std::size_t>(r)) = reg_usage::not_in_use;
    }

    reg_usage is_in_use(const ir_register& r) {
      for (std::size_t i = 0; i < pool.size(); i++) {
        if (i == static_cast<std::size_t>(r) && pool.at(i) == reg_usage::in_use) {
          return reg_usage::in_use;
        }
      }

      return reg_usage::not_in_use;
    }

    void visualize_allocated_registers() {
      for (std::size_t i = 0; i < pool.size(); i++) {
        if (pool.at(i) == reg_usage::in_use) {
          std::cout << "r" << i << ": in use\n";
        }
        else {
          std::cout << "r" << i << ": not in use\n";
        }
      }
    }
  };

  class ir_gen { // conversion into a linear IR
    cst_root* root;
    int label_count;
    int temp_var_count;
    std::stack<std::string> label_stack;
    std::unordered_map<std::string, int> label_map;
    std::unordered_map<std::string, int> temp_var_map;
    bool debug;

    ir_function generate_function(cst_function* func_node);
    void generate_function_args(ir_function& function, cst_functionargs* func_args_node);
    void generate_arith_and_bitwise_operators(ir_function& function, cst* c);
    template<typename IntType>
    void generate_common(ir_function& function, cst* assignment_node);
    void handle_push_types(ir_function& function, cst* c);
    void handle_push_types_common(ir_function& function, cst* c);
    void generate_function_call(ir_function& function, cst* c);
    void generate_return(ir_function& function, cst_returnstmt* return_node);
    void generate_or_jump(ir_function& function, cst* comparison, const std::string& true_label);
    void generate_and_jump(ir_function& function, cst* comparison, const std::string& false_label);  
    void generate_inverted_jump(ir_function& function, cst* comparison, const std::string& false_label);
    void generate_normal_jump(ir_function& function, cst* comparison, const std::string& false_label);
    void generate_condition(ir_function& function, cst* node, std::string false_label, std::string true_label);
    void generate_if(ir_function& function, cst_ifstmt* if_node, std::string current_break_label = "", std::string current_loop_start = "");
    void generate_loop(ir_function& function, cst_loopstmt* loop_node);
    void generate_while(ir_function& function, cst_whilestmt* while_node);
    void generate_for(ir_function& function, cst_forstmt* for_node);
    void generate_array_decl(ir_function& function, cst_array* array_node);
    void generate_array_access(ir_function& function, cst_arrayaccess* array_access_node);
    void generate_block(ir_function& function, cst_block* block_node, std::string current_break_label = "", std::string current_loop_start = "");
    std::string create_label();
    std::string create_temp_var();
    void place_label(ir_function& function, std::string label_name);
    void place_temp_var(ir_function& function, std::string var_name);
  public:
    ir_gen(cst_root* root, bool debug = false) : root(root), label_count(0), temp_var_count(0), debug(debug) {}
    void visualize_stack_ir(std::vector<ir_function> funcs);
    std::vector<ir_function> lower_to_stack();
    void visualize_register_ir(std::vector<ir_reg_function> funcs);
    //std::vector<ir_reg_function> translate_to_register(std::vector<ir_function> funcs);
  };
} // namespace occult
