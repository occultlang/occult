  #pragma once
#include <vector>
#include <memory>
#include <variant>

#include "../lexer/lexer.hpp"

namespace occult {
  enum class ast_type {
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
  };
  
  class ast {
    std::vector<std::unique_ptr<ast>> children;
  public:
    ast() = default;
    
    std::string content = ""; // base class
    
    template<typename BaseAst = ast>
    static std::unique_ptr<BaseAst> new_node() {
      return std::make_unique<BaseAst>();
    }
    
    template<typename BaseAst = ast>
    static std::unique_ptr<BaseAst> new_node(std::string content) {
      auto node = std::make_unique<BaseAst>();
      node->content = content;
      
      return node;
    }
    
    template<typename BaseAst = ast>
    static std::unique_ptr<BaseAst> cast(ast* node) {
      if (auto casted_node = dynamic_cast<BaseAst*>(node)) {
        return std::unique_ptr<BaseAst>(casted_node);
      }
      else {
        return nullptr;
      }
    }
    
    template<typename BaseAst = ast>
    static BaseAst* cast_raw(ast* node) {
      if (auto casted_node = dynamic_cast<BaseAst*>(node)) {
        return casted_node;
      }
      else {
        return nullptr;
      }
    }
    
    void add_child(std::unique_ptr<ast> child) {
      children.push_back(std::move(child));
    }
    
    std::vector<std::unique_ptr<ast>>& get_children() {
      return children;
    }
    
    virtual ast_type get_type() = 0;
    
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
    
      std::cout << output << "\n";
    
      for (const auto& child : children) {
        child->visualize(depth + 1);
      }
    }
  };
  
  #define NODE(tt, nn)  \
  class nn : public ast { \
    public: \
    ast_type get_type() override { \
      return ast_type::tt; \
    } \
    std::string to_string() override { \
      return #nn; \
    } \
  };
  
  NODE(root, ast_root)
  NODE(block, ast_block)
  NODE(identifier, ast_identifier)
  NODE(function, ast_function)
  NODE(forcondition, ast_forcondition)
  NODE(foriterexpr, ast_foriterexpr)
  NODE(functionarguments, ast_functionargs)
  NODE(functionargument, ast_functionarg)
  NODE(callarg, ast_callarg)
  NODE(functioncall, ast_functioncall)
  NODE(ifstmt, ast_ifstmt)
  NODE(elsestmt, ast_elsestmt)
  NODE(elseifstmt, ast_elseifstmt)
  NODE(loopstmt, ast_loopstmt)
  NODE(whilestmt, ast_whilestmt)
  NODE(forstmt, ast_forstmt)
  NODE(continuestmt, ast_continuestmt)
  NODE(breakstmt, ast_breakstmt)
  NODE(returnstmt, ast_returnstmt)
  NODE(assignment, ast_assignment)
  NODE(int8_datatype, ast_int8)
  NODE(int16_datatype, ast_int16)
  NODE(int32_datatype, ast_int32)
  NODE(int64_datatype, ast_int64)
  NODE(uint8_datatype, ast_uint8)
  NODE(uint16_datatype, ast_uint16)
  NODE(uint32_datatype, ast_uint32)
  NODE(uint64_datatype, ast_uint64)
  NODE(float32_datatype, ast_float32)
  NODE(float64_datatype, ast_float64)
  NODE(string_datatype, ast_string)
  NODE(number_literal, ast_numberliteral)
  NODE(float_literal, ast_floatliteral)
  NODE(add_operator, ast_add)
  NODE(subtract_operator, ast_subtract)
  NODE(unary_plus_operator, ast_unary_plus)
  NODE(unary_minus_operator, ast_unary_minus)
  NODE(multiply_operator, ast_multiply)
  NODE(division_operator, ast_divide)
  NODE(modulo_operator, ast_modulo)
  NODE(bitwise_and, ast_bitwise_and)
  NODE(unary_bitwise_not, ast_unary_bitwise_not)
  NODE(bitwise_or, ast_bitwise_or)
  NODE(xor_operator, ast_xor)
  NODE(bitwise_lshift, ast_bitwise_lshift)
  NODE(bitwise_rshift, ast_bitwise_rshift)
  NODE(and_operator, ast_and)
  NODE(or_operator, ast_or)
  NODE(unary_not_operator, ast_unary_not)
  NODE(equals_operator, ast_equals)
  NODE(not_equals_operator, ast_not_equals)
  NODE(greater_than_operator, ast_greater_than)
  NODE(less_than_operator, ast_less_than)
  NODE(greater_than_or_equal_operator, ast_greater_than_or_equal)
  NODE(less_than_or_equal_operator, ast_less_than_or_equal)
  NODE(comma, ast_comma)
  NODE(include, ast_includestmt)
  NODE(stringliteral, ast_stringliteral)
  NODE(charliteral, ast_charliteral)
  NODE(label, ast_label)
  NODE(array, ast_array)
  NODE(dimensions_count, ast_dimensions_count)
  NODE(dimension, ast_dimension)
  NODE(arraybody, ast_arraybody)
  NODE(arrayelem, ast_arrayelement)
  NODE(pointer, ast_pointer)
} // namespace occult
