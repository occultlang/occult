#pragma once
#include "../lexer/lexer.hpp"
#include "cst.hpp"
#include <functional>

// this file contains helper functions as well as maps

#define CST_MAP_ENTRY(token, node)                                                                 \
    {token, [](const std::string& lexeme) { return cst::new_node<node>(lexeme); }}
#define CST_MAP_ENTRY_CUSTOM_LEXEME(token, node, value)                                            \
    {token, [](const std::string&) { return cst::new_node<node>(value); }}
#define CST_MAP_ENTRY_NO_LEXEME(token, node) {token, []() { return cst::new_node<node>(); }}

namespace occult {
inline std::unordered_map<token_type, int> precedence_map = {
    {unary_plus_operator_tt, 2},
    {unary_minus_operator_tt, 2},
    {unary_bitwise_not_tt, 2},
    {unary_not_operator_tt, 2}, // logical

    {dereference_operator_tt, 2},
    {reference_operator_tt, 2},

    {multiply_operator_tt, 3},
    {division_operator_tt, 3},
    {modulo_operator_tt, 3},

    {add_operator_tt, 4},
    {subtract_operator_tt, 4},

    {bitwise_rshift_tt, 5},
    {bitwise_lshift_tt, 5},

    {greater_than_operator_tt, 8},
    {less_than_operator_tt, 8},
    {less_than_or_equal_operator_tt, 8},
    {greater_than_or_equal_operator_tt, 8},

    {equals_operator_tt, 9},
    {not_equals_operator_tt, 9},

    {bitwise_and_tt, 10},

    {xor_operator_tt, 11}, // bitwise

    {bitwise_or_tt, 12}, // bitwise

    {and_operator_tt, 13}, // logical normally 13 (space in between is 6)

    {or_operator_tt, 14}, // logical normally 14 (space in between is 7)

    {assignment_tt, 15},
    {left_paren_tt, 17}};

template <typename Iterator>
int find_first_token(Iterator begin, Iterator end, token_type tt) {
    auto it = std::find_if(begin, end, [tt](const token_t& t) { return t.tt == tt; });

    return (it != end) ? std::distance(begin, it) : -1;
}

inline std::unordered_map<int, std::function<std::unique_ptr<cst>(std::string)>> cst_map = {
    CST_MAP_ENTRY(function_call_parser_tt, cst_functioncall),
    CST_MAP_ENTRY(comma_tt, cst_comma),
    CST_MAP_ENTRY(identifier_tt, cst_identifier),
    CST_MAP_ENTRY(number_literal_tt, cst_numberliteral),
    CST_MAP_ENTRY_CUSTOM_LEXEME(false_keyword_tt, cst_numberliteral, "0"),
    CST_MAP_ENTRY_CUSTOM_LEXEME(true_keyword_tt, cst_numberliteral, "1"),
    CST_MAP_ENTRY(char_literal_tt, cst_charliteral),
    CST_MAP_ENTRY(float_literal_tt, cst_floatliteral),
    CST_MAP_ENTRY(string_literal_tt, cst_stringliteral),
    CST_MAP_ENTRY(add_operator_tt, cst_add),
    CST_MAP_ENTRY(subtract_operator_tt, cst_subtract),
    CST_MAP_ENTRY(unary_plus_operator_tt, cst_unary_plus),
    CST_MAP_ENTRY(unary_minus_operator_tt, cst_unary_minus),
    CST_MAP_ENTRY(multiply_operator_tt, cst_multiply),
    CST_MAP_ENTRY(division_operator_tt, cst_divide),
    CST_MAP_ENTRY(modulo_operator_tt, cst_modulo),
    CST_MAP_ENTRY(bitwise_and_tt, cst_bitwise_and),
    CST_MAP_ENTRY(unary_bitwise_not_tt, cst_unary_bitwise_not),
    CST_MAP_ENTRY(bitwise_or_tt, cst_bitwise_or),
    CST_MAP_ENTRY(xor_operator_tt, cst_xor),
    CST_MAP_ENTRY(bitwise_lshift_tt, cst_bitwise_lshift),
    CST_MAP_ENTRY(bitwise_rshift_tt, cst_bitwise_rshift),
    CST_MAP_ENTRY(and_operator_tt, cst_and),
    CST_MAP_ENTRY(or_operator_tt, cst_or),
    CST_MAP_ENTRY(unary_not_operator_tt, cst_unary_not),
    CST_MAP_ENTRY(equals_operator_tt, cst_equals),
    CST_MAP_ENTRY(not_equals_operator_tt, cst_not_equals),
    CST_MAP_ENTRY(greater_than_operator_tt, cst_greater_than),
    CST_MAP_ENTRY(less_than_operator_tt, cst_less_than),
    CST_MAP_ENTRY(greater_than_or_equal_operator_tt, cst_greater_than_or_equal),
    CST_MAP_ENTRY(less_than_or_equal_operator_tt, cst_less_than_or_equal),
    CST_MAP_ENTRY(dereference_operator_tt, cst_dereference),
    CST_MAP_ENTRY(reference_operator_tt, cst_reference),
}; // map for converting lexemes to nodes relatively easily

inline std::unordered_map<token_type, std::function<std::unique_ptr<cst>()>> datatype_map = {
    CST_MAP_ENTRY_NO_LEXEME(int8_keyword_tt, cst_int8),
    CST_MAP_ENTRY_NO_LEXEME(int16_keyword_tt, cst_int16),
    CST_MAP_ENTRY_NO_LEXEME(int32_keyword_tt, cst_int32),
    CST_MAP_ENTRY_NO_LEXEME(int64_keyword_tt, cst_int64),
    CST_MAP_ENTRY_NO_LEXEME(uint8_keyword_tt, cst_uint8),
    CST_MAP_ENTRY_NO_LEXEME(uint16_keyword_tt, cst_uint16),
    CST_MAP_ENTRY_NO_LEXEME(uint32_keyword_tt, cst_uint32),
    CST_MAP_ENTRY_NO_LEXEME(uint64_keyword_tt, cst_uint64),
    CST_MAP_ENTRY_NO_LEXEME(float32_keyword_tt, cst_float32),
    CST_MAP_ENTRY_NO_LEXEME(float64_keyword_tt, cst_float64),
    CST_MAP_ENTRY_NO_LEXEME(string_keyword_tt, cst_string),
    CST_MAP_ENTRY_NO_LEXEME(char_keyword_tt, cst_int8),
    CST_MAP_ENTRY_NO_LEXEME(boolean_keyword_tt, cst_bool),
};
} // namespace occult
