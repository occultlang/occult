#pragma once
#include "../../parser/cst.hpp"

#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>
#include <variant>

namespace occult {
constexpr const char *kStructPtrSuffix = "_ptr";

enum ir_opcode {
  null_op,
  op_push,
  op_push_for_ret,
  op_push_single,
  // op_pushf,
  op_store,
  op_load,
  /*op_storef32,
  op_loadf32,
  op_storef64,
  op_loadf64,*/
  op_add,
  op_div,
  op_mod,
  op_sub,
  op_mul,
  op_imul,
  op_idiv,
  op_imod,
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
  op_jz,  // je
  op_jnz, // jne
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
  op_cmpf32,
  op_cmpf64,
  op_ret,
  op_call,
  op_syscall,
  op_addf32,
  op_divf32,
  op_subf32,
  op_mulf32,
  op_modf32,
  op_addf64,
  op_divf64,
  op_subf64,
  op_mulf64,
  op_modf64,
  label,
  op_array_decl,
  op_array_access_element,
  op_array_store_element,
  op_declare_where_to_store,
  op_array_dimensions,
  op_array_size,
  op_decl_array_type,
  op_reference,
  op_dereference,
  op_dereference_assign,
  op_store_at_addr,
  op_mark_for_array_access,
  op_struct_decl,
  op_member_access,
  op_member_store,
  op_struct_load,
  op_struct_store,
  op_push_shellcode,
};

inline std::string opcode_to_string(ir_opcode op) {
  switch (op) {
  case op_push:
    return "push";
  // case op_pushf:
  //     return "pushf";
  case op_store:
    return "store";
  case op_load:
    return "load";
  /*case op_loadf32:
      return "loadf32";
  case op_storef32:
      return "storef32";
  case op_loadf64:
      return "loadf64";
  case op_storef64:
      return "storef64";*/
  case op_add:
    return "add";
  case op_div:
    return "div";
  case op_mod:
    return "mod";
  case op_sub:
    return "sub";
  case op_mul:
    return "mul";
  case op_imul:
    return "imul";
  case op_idiv:
    return "idiv";
  case op_imod:
    return "imod";
  case op_logical_and:
    return "logical_and";
  case op_logical_or:
    return "logical_or";
  case op_bitwise_and:
    return "bitwise_and";
  case op_bitwise_or:
    return "bitwise_or";
  case op_bitwise_xor:
    return "bitwise_xor";
  case op_bitwise_not:
    return "bitwise_not";
  case op_not:
    return "not";
  case op_bitwise_lshift:
    return "bitwise_lshift";
  case op_bitwise_rshift:
    return "bitwise_rshift";
  case op_negate:
    return "negate";
  case op_jmp:
    return "jmp";
  case op_jz:
    return "jz";
  case op_jnz:
    return "jnz";
  case op_jl:
    return "jl";
  case op_jle:
    return "jle";
  case op_jg:
    return "jg";
  case op_jge:
    return "jge";
  case op_setz:
    return "setz";
  case op_setnz:
    return "setnz";
  case op_setl:
    return "setl";
  case op_setle:
    return "setle";
  case op_setg:
    return "setg";
  case op_setge:
    return "setge";
  case op_cmp:
    return "cmp";
  case op_cmpf32:
    return "cmpf32";
  case op_cmpf64:
    return "cmpf64";
  case op_ret:
    return "ret";
  case op_call:
    return "call";
  case op_syscall:
    return "syscall";
  case op_addf32:
    return "addf32";
  case op_addf64:
    return "addf64";
  case op_divf32:
    return "divf32";
  case op_divf64:
    return "divf64";
  case op_subf32:
    return "subf32";
  case op_subf64:
    return "subf64";
  case op_mulf32:
    return "mulf32";
  case op_mulf64:
    return "mulf64";
  case op_modf32:
    return "modf32";
  case op_modf64:
    return "modf64";
  case label:
    return "label";
  case op_array_decl:
    return "array_decl";
  case op_array_access_element:
    return "array_access_elem";
  case op_array_store_element:
    return "array_store_element";
  case op_array_dimensions:
    return "array_dimensions";
  case op_array_size:
    return "array_size";
  case op_decl_array_type:
    return "decl_array_type";
  case op_declare_where_to_store:
    return "declare_where_to_store";
  case op_reference:
    return "reference";
  case op_dereference:
    return "dereference";
  case op_dereference_assign:
    return "dereference_assign";
  case op_store_at_addr:
    return "store_at_addr";
  case op_mark_for_array_access:
    return "mark_for_array_access";
  case op_struct_decl:
    return "struct_decl";
  case op_member_access:
    return "member_access";
  case op_member_store:
    return "member_store";
  case op_struct_load:
    return "struct_load";
  case op_struct_store:
    return "struct_store";
  case op_push_for_ret:
    return "push_ret";
  case op_push_single:
    return "push_single";
  case op_push_shellcode:
    return "push_shellcode";
  default:
    return "unknown_opcode";
  }
}

struct ir_argument {
  std::string name;
  std::string type;

  ir_argument(std::string name, std::string type)
      : name(std::move(name)), type(std::move(type)) {}
};

using ir_operand =
    std::variant<std::monostate, std::int64_t, std::uint64_t, std::int32_t,
                 std::uint32_t, std::int16_t, std::uint16_t, std::int8_t,
                 std::uint8_t, double, float, std::string>;

struct ir_instr {
  ir_opcode op;
  ir_operand operand;
  std::string type;

  ir_instr(const ir_opcode op, ir_operand operand)
      : op(op), operand(std::move(operand)) {}

  ir_instr(const ir_opcode op, ir_operand operand, std::string type)
      : op(op), operand(std::move(operand)), type(std::move(type)) {}

  explicit ir_instr(const ir_opcode op) : op(op), operand(std::monostate()) {}
};

struct ir_function {
  std::vector<ir_instr> code;
  std::vector<ir_argument> args;
  std::string name;
  std::string type;
  bool uses_shellcode = false;
  bool is_external = false;

  bool operator==(const ir_function &other) const { return name == other.name; }
};

struct ir_function_hasher {
  size_t operator()(const ir_function &ir_func) const {
    return std::hash<std::string>{}(ir_func.name);
  }
};

struct ir_struct_member {
  std::string
      datatype;     // name of the datatype / or structure for custom datatype
  std::string name; // name of the member variable

  ir_struct_member() = default;

  ir_struct_member(std::string datatype, std::string name)
      : datatype(std::move(datatype)), name(std::move(name)) {}
};

struct ir_struct {      // used for custom data types (structures in the IR)
  std::string datatype; // name of the structure / custom data type
  std::vector<ir_struct_member> members; // list of members

  ir_struct() = default;

  ir_struct(std::string datatype, std::vector<ir_struct_member> members)
      : datatype(std::move(datatype)), members(std::move(members)) {}
};

enum ir_typename : std::uint8_t {
  int8,
  int16,
  int32,
  int64,
  uint8,
  uint16,
  uint32,
  uint64,
  float32,
  float64,
  string,
  boolean
};

struct visitor_stack {
  void operator()(const float &v) const { std::cout << v << "\n"; };
  void operator()(const double &v) const { std::cout << v << "\n"; };
  void operator()(const std::int64_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::uint64_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::int32_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::uint32_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::int16_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::uint16_t &v) const { std::cout << v << "\n"; };
  void operator()(const std::int8_t &v) const {
    std::cout << static_cast<int>(v) << "\n";
  };
  void operator()(const std::uint8_t &v) const {
    std::cout << static_cast<int>(v) << "\n";
  };
  void operator()(const std::string &v) const { std::cout << v << "\n"; };
  void operator()(std::monostate) const { std::cout << "\n"; };
};

const std::unordered_map<std::string, ir_typename> ir_typemap = {
    {"int64", ir_typename::int64},     {"int32", ir_typename::int32},
    {"int16", ir_typename::int16},     {"int8", ir_typename::int8},
    {"uint64", ir_typename::uint64},   {"uint32", ir_typename::uint32},
    {"uint16", ir_typename::uint16},   {"uint8", ir_typename::uint8},
    {"float32", ir_typename::float32}, {"float64", ir_typename::float64},
    {"bool", ir_typename::boolean},    {"char", ir_typename::int8},
    {"str", ir_typename::string},
};

static std::unordered_map<std::string, bool> is_signed = {
    {"int64", true},   {"int32", true},   {"int16", true},   {"int8", true},
    {"uint64", false}, {"uint32", false}, {"uint16", false}, {"uint8", false},
    {"bool", true},    {"char", true},
};

class ir_gen { // conversion into a linear IR
  cst_root *root;
  int label_count;
  std::stack<std::string> label_stack;
  std::unordered_map<std::string, int> label_map;
  bool debug;
  std::unordered_map<std::string, cst *> custom_type_map;
  std::unordered_map<std::string, ir_function> func_map;
  std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                     ir_function_hasher>
      local_variable_map; // function -> variable name -> type

  std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                     ir_function_hasher>
      local_array_map; // function -> array name -> type

  enum class type_of_push : std::uint8_t {
    normal, // normal push (more than one register)
    ret,    // pushes to return in codegen
    single  // pushes to a single register in codegen
  };

  ir_function generate_function(cst_function *func_node);

  void generate_function_args(ir_function &function,
                              cst_functionargs *func_args_node);

  void generate_arith_and_bitwise_operators(
      ir_function &function, cst *c,
      std::optional<std::string> type = std::nullopt);

  void emit_comparison(ir_function &function, std::optional<std::string> type);

  void generate_boolean_value(ir_function &function, cst *node,
                              type_of_push type_push = type_of_push::normal);

  template <typename IntType>
  void generate_common_generic(ir_function &function, cst *assignment_node,
                               std::optional<std::string> type = std::nullopt,
                               type_of_push type_push = type_of_push::normal);

  void handle_push_types(ir_function &function, cst *c,
                         std::optional<std::string> type = std::nullopt);

  void generate_common(ir_function &function, cst *c, std::string type,
                       type_of_push type_push = type_of_push::normal);

  void generate_function_call(ir_function &function, cst *c);

  void generate_return(ir_function &function, cst_returnstmt *return_node);

  void generate_or_jump(ir_function &function, cst *comparison,
                        const std::string &true_label);

  void generate_and_jump(ir_function &function, cst *comparison,
                         const std::string &false_label);

  void generate_inverted_jump(ir_function &function, cst *comparison,
                              const std::string &false_label);

  void generate_normal_jump(ir_function &function, cst *comparison,
                            const std::string &false_label);

  void generate_condition(ir_function &function, cst *node,
                          const std::string &false_label,
                          const std::string &true_label);

  void generate_if(ir_function &function, cst_ifstmt *if_node,
                   const std::string &current_break_label = "",
                   const std::string &current_loop_start = "");

  void generate_loop(ir_function &function, cst_loopstmt *loop_node);

  void generate_while(ir_function &function, cst_whilestmt *while_node);

  void generate_for(ir_function &function, cst_forstmt *for_node);

  void generate_array_decl(ir_function &function, cst_array *array_node);

  void generate_array_access(ir_function &function,
                             cst_arrayaccess *array_access_node);

  void generate_struct_decl(ir_function &function, cst_struct *struct_node);

  void generate_member_access(ir_function &function,
                              cst_memberaccess *member_access_node);

  void generate_block(ir_function &function, cst_block *block_node,
                      std::string current_break_label = "",
                      std::string current_loop_start = "");

  std::string create_label();

  void place_label(ir_function &function, std::string label_name);

public:
  ir_gen(cst_root *root, std::unordered_map<std::string, cst *> custom_type_map,
         const bool debug = false)
      : root(root), label_count(0), debug(debug),
        custom_type_map(std::move(custom_type_map)) {}

  static void visualize_stack_ir(const std::vector<ir_function> &funcs);

  static void visualize_structs(const std::vector<ir_struct> &structs);

  std::vector<ir_function> lower_functions();

  std::vector<ir_struct> lower_structs();
};
} // namespace occult
