#pragma once
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <optional>
#include "../util/color_defs.hpp"

namespace occult {
  enum token_type {
    // whitespace + comments
    whitespace_tt,
    comment_tt,
    multiline_comment_start_tt,
    multiline_comment_end_tt,
    
    // literals
    number_literal_tt,
    string_literal_tt,
    char_literal_tt,
    float_literal_tt,

    // arithmetic operators
    add_operator_tt,      // + 
    subtract_operator_tt, // - 
    
    multiply_operator_tt, // *
    division_operator_tt, // /
    modulo_operator_tt,   // %
    
    bitwise_and_tt, // &
    unary_bitwise_not_tt, // ~
    bitwise_or_tt, // |
    xor_operator_tt, // ^
    bitwise_lshift_tt, // <<
    bitwise_rshift_tt, // >>
    
    // logical operators
    and_operator_tt, // &&
    or_operator_tt,  // ||
    unary_not_operator_tt, // ! unary

    // relational operators
    equals_operator_tt,                // ==
    not_equals_operator_tt,            // !=
    greater_than_operator_tt,          // >
    less_than_operator_tt,             // <
    greater_than_or_equal_operator_tt, // >=
    less_than_or_equal_operator_tt,    // <=

    // assignment with pseudocode
    assignment_tt, // var name = value;

    // delimiters
    right_paren_tt,
    left_paren_tt,
    right_bracket_tt,
    left_bracket_tt,
    right_curly_bracket_tt,
    left_curly_bracket_tt,
    semicolon_tt,
    comma_tt,

    // identifier + keywords
    identifier_tt,       // anything else that is not a keyword or in this list
    function_keyword_tt, // fn
    if_keyword_tt,       // if
    else_keyword_tt,     // else
    elseif_keyword_tt,   // elif
    loop_keyword_tt,     // loop
    when_keyword_tt,     // when
    do_keyword_tt,       // do
    return_keyword_tt,   // return
    break_keyword_tt,    // break
    continue_keyword_tt, // continue
    while_keyword_tt,    // while
    for_keyword_tt,      // for
    in_keyword_tt,       // in
    include_keyword_tt,  // include
    int64_keyword_tt,    // int64
    int32_keyword_tt,    // int32
    int16_keyword_tt,    // int16
    int8_keyword_tt,     // int8
    uint64_keyword_tt,   // uint64
    uint32_keyword_tt,   // uint32
    uint16_keyword_tt,   // uint16
    uint8_keyword_tt,    // uint8
    float32_keyword_tt,  // float32
    float64_keyword_tt,  // float64
    boolean_keyword_tt,  // bool,
    char_keyword_tt,     // char
    string_keyword_tt,   // str
    array_keyword_tt,    // array
    pointer_keyword_tt,  // ptr
    true_keyword_tt,     // true
    false_keyword_tt,    // false
    unary_minus_operator_tt,
    unary_plus_operator_tt,
    
    end_of_file_tt,
    unkown_tt,
    function_call_parser_tt,
    argument_count_tt,
  };

  typedef struct token_t {
    std::uintptr_t line;
    std::uintptr_t column;
    std::string lexeme;
    token_type tt;

    static std::string get_typename(const token_type& tt);

    token_t(const std::uintptr_t& line, const std::uintptr_t& column, const std::string& lexeme, const token_type& tt) : line(line), column(column), lexeme(lexeme), tt(tt) {}
  } token_t; // token structure

  class lexer {
    std::string source;
    std::uintptr_t pos;
    std::uintptr_t line;
    std::uintptr_t column;
    
    std::vector<token_t> stream;
    void increment(const std::uintptr_t& line = 0, const std::uintptr_t& pos = 0, const std::uintptr_t& column = 0);
    void handle_comment();
    void handle_whitespace();
    std::string handle_escape_sequences(const char& type);
    token_t handle_string();
    token_t handle_char();
    token_t handle_numeric();
    token_t handle_delimiter();
    token_t handle_operator(const bool& is_double);
    token_t handle_symbol();
    token_t get_next_token();
  public:
    lexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}
    
    std::vector<token_t> analyze();
    void visualize(const std::optional<std::vector<token_t>>& o_s = std::nullopt); 
  };
} // namespace occult
