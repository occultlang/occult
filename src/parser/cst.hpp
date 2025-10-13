  #pragma once
#include <vector>
#include <memory>
#include <variant>

#include "../lexer/lexer.hpp"

namespace occult {
  enum class cst_type {
    root,
    block,
    identifier,
    function,
    ifstmt,
    elsestmt,
    elseifstmt,
    loopstmt,
    whilestmt,
    forstmt,
    continuestmt,
    breakstmt,
    returnstmt,
    functionarguments,
    functioncall,
    assignment,
    int8_datatype, // char as well as boolean
    int16_datatype,
    int32_datatype,
    int64_datatype,
    uint8_datatype,
    uint16_datatype,
    uint32_datatype,
    uint64_datatype,
    float32_datatype,
    float64_datatype,
    string_datatype,
    number_literal,
    float_literal,
    add_operator,
    subtract_operator,
    unary_plus_operator,
    unary_minus_operator,
    multiply_operator,
    division_operator,
    modulo_operator,
    bitwise_and,
    unary_bitwise_not,
    bitwise_or,
    xor_operator,
    bitwise_lshift,
    bitwise_rshift,
    and_operator,
    or_operator,
    unary_not_operator,
    equals_operator,
    not_equals_operator,
    greater_than_operator,
    less_than_operator,
    greater_than_or_equal,
    less_than_or_equal,
    greater_than_or_equal_operator,
    less_than_or_equal_operator,
    callarg,
    comma,
    include,
    stringliteral,
    forcondition, // only used in for loops, special condition
    foriterexpr, // only used in for loops, special for only iteration of integers
    charliteral,
    functionargument,
    label,
    array,
    pointer,
    dimensions_count,
    dimension,
    arraybody,
    arrayelem,
    arrayaccess,
    dereference,
    reference,
    expr_start,
    expr_end,
    structure,
    customtype,
    memberaccess,
  };
  
  class cst {
    std::vector<std::unique_ptr<cst>> children;
  public:
    cst() = default;
    
    std::string content = ""; // base class
    std::size_t num_pointers = 0;
    bool do_not = false;
    
    template<typename BaseCst = cst>
    static std::unique_ptr<BaseCst> new_node() {
      return std::make_unique<BaseCst>();
    }
    
    template<typename BaseCst = cst>
    static std::unique_ptr<BaseCst> new_node(std::string content) {
      auto node = std::make_unique<BaseCst>();
      node->content = content;
      
      return node;
    }
    
    template<typename BaseCst = cst>
    static std::unique_ptr<BaseCst> cast(cst* node) {
      if (auto casted_node = dynamic_cast<BaseCst*>(node)) {
        return std::unique_ptr<BaseCst>(casted_node);
      }
      else {
        return nullptr;
      }
    }
    
    template<typename BaseCst = cst>
    static BaseCst* cast_raw(cst* node) {
      if (auto casted_node = dynamic_cast<BaseCst*>(node)) {
        return casted_node;
      }
      else {
        return nullptr;
      }
    }
    
    void add_child(std::unique_ptr<cst> child) {
      children.push_back(std::move(child));
    }
    
    std::vector<std::unique_ptr<cst>>& get_children() {
      return children;
    }
    
    virtual cst_type get_type() = 0;
    
    virtual std::string to_string() = 0;
    
    void visualize(int depth = 0) {
      std::string indent(depth * 2, ' ');
      std::string output = indent + to_string();
      std::string content_copy = content;
      
      if (!content_copy.empty()) {
        size_t pos = 0;
        while ((pos = content_copy.find('\n', pos)) != std::string::npos) {
          content_copy.replace(pos, 1, "\\n");
          pos += 2;
        }
        output += ": " + content_copy;
      }

      if (do_not) {
        output += GREEN " (do_not = true)" RESET;
      }
      
      if (num_pointers > 0) {
        output += YELLOW " (num_pointers = " + std::to_string(num_pointers) + ")" + RESET;
      }

      std::cout << output << "\n";
    
      for (const auto& child : children) {
        child->visualize(depth + 1);
      }
    }
  };
  
  #define NODE(tt, nn)  \
  class nn : public cst { \
    public: \
    cst_type get_type() override { \
      return cst_type::tt; \
    } \
    std::string to_string() override { \
      return #nn; \
    } \
  };
  
  NODE(root, cst_root)
  NODE(block, cst_block)
  NODE(identifier, cst_identifier)
  NODE(function, cst_function)
  NODE(forcondition, cst_forcondition)
  NODE(foriterexpr, cst_foriterexpr)
  NODE(functionarguments, cst_functionargs)
  NODE(functionargument, cst_functionarg)
  NODE(callarg, cst_callarg)
  NODE(functioncall, cst_functioncall)
  NODE(ifstmt, cst_ifstmt)
  NODE(elsestmt, cst_elsestmt)
  NODE(elseifstmt, cst_elseifstmt)
  NODE(loopstmt, cst_loopstmt)
  NODE(whilestmt, cst_whilestmt)
  NODE(forstmt, cst_forstmt)
  NODE(continuestmt, cst_continuestmt)
  NODE(breakstmt, cst_breakstmt)
  NODE(returnstmt, cst_returnstmt)
  NODE(assignment, cst_assignment)
  NODE(int8_datatype, cst_int8)
  NODE(int16_datatype, cst_int16)
  NODE(int32_datatype, cst_int32)
  NODE(int64_datatype, cst_int64)
  NODE(uint8_datatype, cst_uint8)
  NODE(uint16_datatype, cst_uint16)
  NODE(uint32_datatype, cst_uint32)
  NODE(uint64_datatype, cst_uint64)
  NODE(float32_datatype, cst_float32)
  NODE(float64_datatype, cst_float64)
  NODE(string_datatype, cst_string)
  NODE(number_literal, cst_numberliteral)
  NODE(float_literal, cst_floatliteral)
  NODE(add_operator, cst_add)
  NODE(subtract_operator, cst_subtract)
  NODE(unary_plus_operator, cst_unary_plus)
  NODE(unary_minus_operator, cst_unary_minus)
  NODE(multiply_operator, cst_multiply)
  NODE(division_operator, cst_divide)
  NODE(modulo_operator, cst_modulo)
  NODE(bitwise_and, cst_bitwise_and)
  NODE(unary_bitwise_not, cst_unary_bitwise_not)
  NODE(bitwise_or, cst_bitwise_or)
  NODE(xor_operator, cst_xor)
  NODE(bitwise_lshift, cst_bitwise_lshift)
  NODE(bitwise_rshift, cst_bitwise_rshift)
  NODE(and_operator, cst_and)
  NODE(or_operator, cst_or)
  NODE(unary_not_operator, cst_unary_not)
  NODE(equals_operator, cst_equals)
  NODE(not_equals_operator, cst_not_equals)
  NODE(greater_than_operator, cst_greater_than)
  NODE(less_than_operator, cst_less_than)
  NODE(greater_than_or_equal_operator, cst_greater_than_or_equal)
  NODE(less_than_or_equal_operator, cst_less_than_or_equal)
  NODE(comma, cst_comma)
  NODE(include, cst_includestmt)
  NODE(stringliteral, cst_stringliteral)
  NODE(charliteral, cst_charliteral)
  NODE(label, cst_label)
  NODE(array, cst_array)
  NODE(structure, cst_struct)
  NODE(dimensions_count, cst_dimensions_count)
  NODE(dimension, cst_dimension)
  NODE(arraybody, cst_arraybody)
  NODE(arrayelem, cst_arrayelement)
  NODE(arrayaccess, cst_arrayaccess)
  NODE(dereference, cst_dereference)
  NODE(reference, cst_reference)
  NODE(expr_start, cst_expr_start) // not used
  NODE(expr_end, cst_expr_end) // used to mark an end of an expression for comparisons
  NODE(customtype, cst_customtype)
  NODE(memberaccess, cst_memberaccess)
} // namespace occult
