#pragma once
#include "lexemeer.hpp"
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
  
      {comma_tt, 15}};
  
  // true is right associative, false is left
  std::unordered_map<token_type, bool> associativity_map = {
      {unary_plus_operator_tt, true},
      {unary_minus_operator_tt, true},
      {unary_bitwise_not_tt, true},
      {unary_not_operator_tt, true}, // logical
  
      {multiply_operator_tt, false},
      {division_operator_tt, false},
      {modulo_operator_tt, false},
  
      {add_operator_tt, false},
      {subtract_operator_tt, false},
  
      {bitwise_rshift_tt, false},
      {bitwise_lshift_tt, false},
  
      {greater_than_operator_tt, false},
      {less_than_operator_tt, false},
      {less_than_or_equal_operator_tt, false},
      {greater_than_or_equal_operator_tt, false},
  
      {equals_operator_tt, false},
      {not_equals_operator_tt, false},
  
      {bitwise_and_tt, false},
  
      {xor_operator_tt, false}, // bitwise
  
      {bitwise_or_tt, false}, // bitwise
  
      {and_operator_tt, false}, // logical
  
      {or_operator_tt, false}, // logical
  
      {assignment_tt, true},
  
      {comma_tt, false}};
  
  bool is_unary(token_type tt) {
    return tt == unary_plus_operator_tt || tt == unary_minus_operator_tt ||
           tt == unary_bitwise_not_tt || tt == unary_not_operator_tt;
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
  int find_first_semicolon(Iterator begin, Iterator end) {
      auto it = std::find_if(begin, end, [](const token_t& t) {
          return t.tt == semicolon_tt; 
      });
  
      return (it != end) ? std::distance(begin, it) : -1;
  }
  
  std::unordered_map<int, std::function<std::unique_ptr<ast>(std::string)>> ast_map = {
      {function_call_parser_tt, [](std::string lexeme) { return ast::new_node<ast_functioncall>(lexeme); }},
      {comma_tt, [](std::string lexeme) { return ast::new_node<ast_comma>(lexeme); }},
      {number_literal_tt, [](std::string lexeme) { return ast::new_node<ast_numberliteral>(lexeme); }},
      {identifier_tt, [](std::string lexeme) { return ast::new_node<ast_identifier>(lexeme); }},
      {float_literal_tt, [](std::string lexeme) { return ast::new_node<ast_floatliteral>(lexeme); }},
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
      {less_than_or_equal_operator_tt, [](std::string lexeme) { return ast::new_node<ast_less_than_or_equal>(lexeme); }},
  }; // map for converting lexemes to nodes relatively easily
}
