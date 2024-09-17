#pragma once
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <print>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace occult {
  enum token_type {
    // whitespace + comments
    whitespace_tt,
    comment_tt,
    multiline_comment_tt,

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
    exponent_operator_tt, // ^

    // logical operators
    and_operator_tt, // &&
    or_operator_tt,  // ||
    not_operator_tt, // !

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
    //in_keyword_tt,       // in
    return_keyword_tt,   // return
    break_keyword_tt,    // break
    continue_keyword_tt, // continue
    while_keyword_tt,    // while
    for_keyword_tt,      // for
    match_keyword_tt,    // match
    default_keyword_tt,  // default
    case_keyword_tt,     // case
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
    true_keyword_tt,     // true
    false_keyword_tt,    // false

    end_of_file_tt,
    unkown_tt
  };

  typedef struct token_t {
    std::uintptr_t line;
    std::uintptr_t column;
    std::string lexeme;
    token_type tt;

    static std::string get_typename(token_type tt);

    token_t(std::uintptr_t line, std::uintptr_t column, std::string lexeme, token_type tt) : line(line), column(column), lexeme(lexeme), tt(tt) {}
  } token_t; // token structure

  class lexer {
    std::string source;
    std::uintptr_t pos;
    std::uintptr_t line;
    std::uintptr_t column;
    
  public:
    lexer(std::string source) : source(source), pos(0), line(1), column(1) {}
    
    std::vector<token_t> stream;
    
    token_t get_next();             // getting next token to put into stream
    std::vector<token_t> analyze(); // returns a token stream which will be put into the parser later on
    void visualize(); // print out the AST
  };
} // namespace occult
