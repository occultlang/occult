#pragma once
#include "../lexer/lexer.hpp"
#include "ast.hpp"
#include <functional>

// this file contains helper functions as well as maps

namespace occult {
  std::unordered_map<token_type, int> precedence_map = {
    {unary_plus_operator_tt, 2},
    {unary_minus_operator_tt, 2},
    {unary_bitwise_not_tt, 2},
    {unary_not_operator_tt, 2}, // logical
  
    {multiply_operator_tt, 3},
    {division_operator_tt, 3},
    {modulo_operator_tt, 3},
  
    {add_operator_tt, 4},
    {subtract_operator_tt, 4},
  
    {bitwise_rshift_tt, 5},
    {bitwise_lshift_tt, 5},
  
    {greater_than_operator_tt, 6},
    {less_than_operator_tt, 6},
    {less_than_or_equal_operator_tt, 6},
    {greater_than_or_equal_operator_tt, 6},
  
    {equals_operator_tt, 7},
    {not_equals_operator_tt, 7},
  
    {bitwise_and_tt, 8},
  
    {xor_operator_tt, 9}, // bitwise
  
    {bitwise_or_tt, 10}, // bitwise
  
    {and_operator_tt, 11}, // logical
  
    {or_operator_tt, 12}, // logical
  
    {assignment_tt, 14},
    {left_paren_tt, 16}};
  
  bool is_unary(token_type tt) {
    return tt == unary_plus_operator_tt || tt == unary_minus_operator_tt ||
           tt == unary_bitwise_not_tt || tt == unary_not_operator_tt;
  }
  
  bool is_literal(token_type tt) {
    return tt == number_literal_tt || tt == float_literal_tt || tt == string_literal_tt ||
           tt == char_literal_tt || tt == false_keyword_tt || tt == true_keyword_tt;
  }
  
  bool is_binary(token_type tt) {
    return tt == add_operator_tt ||
           tt == subtract_operator_tt ||
           tt == multiply_operator_tt ||
           tt == division_operator_tt ||
           tt == modulo_operator_tt ||
           tt == bitwise_and_tt ||
           tt == bitwise_or_tt ||
           tt == xor_operator_tt ||
           tt == bitwise_lshift_tt ||
           tt == bitwise_rshift_tt ||
           tt == greater_than_operator_tt ||
           tt == less_than_operator_tt ||
           tt == equals_operator_tt ||
           tt == not_equals_operator_tt ||
           tt == less_than_or_equal_operator_tt ||
           tt == greater_than_or_equal_operator_tt ||
           tt == and_operator_tt ||
           tt == or_operator_tt;
  }
  
  template <typename Iterator> 
  int find_first_token(Iterator begin, Iterator end, token_type tt) {
      auto it = std::find_if(begin, end, [tt](token_t t) {
          return t.tt == tt; 
      });
  
      return (it != end) ? std::distance(begin, it) : -1;
  }
  
  std::unordered_map<int, std::function<std::unique_ptr<ast>(std::string)>> ast_map = {
    {function_call_parser_tt, [](std::string lexeme) { return ast::new_node<ast_functioncall>(lexeme); }},
    {comma_tt, [](std::string lexeme) { return ast::new_node<ast_comma>(lexeme); }},
    {identifier_tt, [](std::string lexeme) { return ast::new_node<ast_identifier>(lexeme); }},
    {number_literal_tt, [](std::string lexeme) { return ast::new_node<ast_numberliteral>(lexeme); }},
    {false_keyword_tt, [](std::string) { return ast::new_node<ast_numberliteral>("0"); }},
    {true_keyword_tt, [](std::string) { return ast::new_node<ast_numberliteral>("1"); }},
    {char_literal_tt, [](std::string lexeme) { return ast::new_node<ast_charliteral>(lexeme); }},
    {float_literal_tt, [](std::string lexeme) { return ast::new_node<ast_floatliteral>(lexeme); }},
    {string_literal_tt, [](std::string lexeme) { return ast::new_node<ast_stringliteral>(lexeme); }},
    {add_operator_tt, [](std::string lexeme) { return ast::new_node<ast_add>(lexeme); }},
    {subtract_operator_tt, [](std::string lexeme) { return ast::new_node<ast_subtract>(lexeme); }},
    {unary_plus_operator_tt, [](std::string lexeme) { return ast::new_node<ast_unary_plus>(lexeme); }},
    {unary_minus_operator_tt, [](std::string lexeme) { return ast::new_node<ast_unary_minus>(lexeme); }},
    {multiply_operator_tt, [](std::string lexeme) { return ast::new_node<ast_multiply>(lexeme); }},
    {division_operator_tt, [](std::string lexeme) { return ast::new_node<ast_divide>(lexeme); }},
    {modulo_operator_tt, [](std::string lexeme) { return ast::new_node<ast_modulo>(lexeme); }},
    {bitwise_and_tt, [](std::string lexeme) { return ast::new_node<ast_bitwise_and>(lexeme); }},
    {unary_bitwise_not_tt, [](std::string lexeme) { return ast::new_node<ast_unary_bitwise_not>(lexeme); }},
    {bitwise_or_tt, [](std::string lexeme) { return ast::new_node<ast_bitwise_or>(lexeme); }},
    {xor_operator_tt, [](std::string lexeme) { return ast::new_node<ast_xor>(lexeme); }},
    {bitwise_lshift_tt, [](std::string lexeme) { return ast::new_node<ast_bitwise_lshift>(lexeme); }},
    {bitwise_rshift_tt, [](std::string lexeme) { return ast::new_node<ast_bitwise_rshift>(lexeme); }},
    {and_operator_tt, [](std::string lexeme) { return ast::new_node<ast_and>(lexeme); }},
    {or_operator_tt, [](std::string lexeme) { return ast::new_node<ast_or>(lexeme); }},
    {unary_not_operator_tt, [](std::string lexeme) { return ast::new_node<ast_unary_not>(lexeme); }},
    {equals_operator_tt, [](std::string lexeme) { return ast::new_node<ast_equals>(lexeme); }},
    {not_equals_operator_tt, [](std::string lexeme) { return ast::new_node<ast_not_equals>(lexeme); }},
    {greater_than_operator_tt, [](std::string lexeme) { return ast::new_node<ast_greater_than>(lexeme); }},
    {less_than_operator_tt, [](std::string lexeme) { return ast::new_node<ast_less_than>(lexeme); }},
    {greater_than_or_equal_operator_tt, [](std::string lexeme) { return ast::new_node<ast_greater_than_or_equal>(lexeme); }},
    {less_than_or_equal_operator_tt, [](std::string lexeme) { return ast::new_node<ast_less_than_or_equal>(lexeme); }}}; // map for converting lexemes to nodes relatively easily
  
  std::unordered_map<token_type, std::function<std::unique_ptr<ast>()>> datatype_map = {
    {int8_keyword_tt, []() { return ast::new_node<ast_int8>(); }},
    {int16_keyword_tt, []() { return ast::new_node<ast_int16>(); }},
    {int32_keyword_tt, []() { return ast::new_node<ast_int32>(); }},
    {int64_keyword_tt, []() { return ast::new_node<ast_int64>(); }},
    {uint8_keyword_tt, []() { return ast::new_node<ast_uint8>(); }},
    {uint16_keyword_tt, []() { return ast::new_node<ast_uint16>(); }},
    {uint32_keyword_tt, []() { return ast::new_node<ast_uint32>(); }},
    {uint64_keyword_tt, []() { return ast::new_node<ast_uint64>(); }},
    {float32_keyword_tt, []() { return ast::new_node<ast_float32>(); }},
    {float64_keyword_tt, []() { return ast::new_node<ast_float64>(); }},
    {string_keyword_tt, []() { return ast::new_node<ast_string>(); }},
    {char_keyword_tt, []() { return ast::new_node<ast_int8>(); }},
    {boolean_keyword_tt, []() { return ast::new_node<ast_int8>(); }}};
}