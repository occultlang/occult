#include "lexer.hpp"
#include "lexer_maps.hpp"
/*
 * TODO scientific notation, hexadecimal, octal and binary for numbers (it's not necessary right now)
 * */

#define DONT_USE_WHITESPACES true
#define DONT_USE_COMMENTS true

namespace occult {
  std::string token_t::get_typename(token_type tt) {
    if (token_typename_map.contains(tt))
      return token_typename_map[tt];
    else
      throw std::runtime_error("can't find token typename");
  }

  token_t lexer::get_next() {
    if (pos >= source.length())
      return token_t(line, column, "end of file", end_of_file_tt);

    // comments go here

    if (source[pos] == '/' && source[pos + 1] == '/') {
      std::string lexeme;

      pos += 2;
      column += 2;

      while (pos < source.size() && source[pos] != '\n' && source[pos] != '\r') {
        lexeme += source[pos];
        pos++;
        column++;
      }

      return token_t(line, column, lexeme, comment_tt);
    }

    if (source[pos] == '/' && source[pos + 1] == '*') {
      std::string lexeme;

      pos += 2;
      column += 2;

      while (pos < source.size() && source[pos] != '*' && source[pos + 1] != '/') {
        if (source[pos] == '\n' || source[pos] == '\r') {
          line++;
        }

        lexeme += source[pos];

        pos++;
        column++;
      }

      pos += 2;
      column += 2;

      return token_t(line, column, lexeme, multiline_comment_tt);
    }

    if (whitespace_map.contains(source[pos])) {         // whitespaces
      if (source[pos] == '\n' || source[pos] == '\r') { // newlines
        pos++;
        line++;
        column = 1;
      } else {
        pos++;
        column++;
      }

      return token_t(line, column, std::string(1, source[pos - 1]), whitespace_map[source[pos - 1]]);
    }

    auto handle_escape_sequences = [this](char str_type) -> std::string {
      std::string lexeme = "";

      while (pos < source.length() && source[pos] != str_type) {
        if (source[pos] == '\\') {
          pos++;
          column++;

          if (pos < source.length()) {
            switch (source[pos]) {
            case 'n':
              lexeme += '\n';
              break;
            case 'r':
              lexeme += '\r';
              break;
            case 't':
              lexeme += '\t';
              break;
            case '\\':
              lexeme += '\\';
              break;
            case '\"':
              lexeme += '\"';
              break;
            default:
              lexeme += source[pos];
              break;
            }
          }
        } else {
          lexeme += source[pos];
        }

        pos++;
        column++;
      }

      return lexeme;
    };

    if (source[pos] == '\"') { // string literals
      pos++;
      column++;

      std::string lexeme = handle_escape_sequences('\"');

      if (pos < source.length() && source[pos] == '\"') { // end of the string literal
        pos++;
        column++;
      }

      return token_t(line, column, lexeme, string_literal_tt);
    }

    if (source[pos] == '\'') { // character literals
      pos++;
      column++;

      std::string lexeme = handle_escape_sequences('\'');

      if (pos < source.length() && source[pos] == '\'') { // end of the char literal
        pos++;
        column++;
      }

      return token_t(line, column, lexeme, char_literal_tt);
    }
    
    if (numeric_set.contains(source[pos])) { // int literals & float literals
      std::string lexeme = "";
      
      bool isfloat = false;
      
      while (pos < source.size() && numeric_set.contains(source[pos])) {
        lexeme += source[pos];
        pos++;
        column++;
      }
      
      if (source[pos] == '.') {
        lexeme += source[pos];
        pos++;
        column++;
        
        isfloat = true;
        
        while (pos < source.size() && numeric_set.contains(source[pos])) {
          lexeme += source[pos];
          pos++;
          column++;
        }
      }

      return token_t(line, column, lexeme, (isfloat) ? float_literal_tt : number_literal_tt);
    }

    if (delimiter_map.contains(source[pos])) { // delimiters
      pos++;
      column++;

      return token_t(line, column, std::string(1, source[pos - 1]), delimiter_map[source[pos - 1]]);
    }

    if (operator_map_double.contains(std::string() + source[pos] + source[pos + 1])) { // double operators
      pos += 2;
      column += 2;

      return token_t(line, column, std::string() + source[pos - 2] + source[pos - 1], operator_map_double[std::string() + source[pos - 2] + source[pos - 1]]);
    }

    if (operator_map_single.contains(source[pos])) { // single operators
      pos++;
      column++;

      return token_t(line, column, std::string(1, source[pos - 1]), operator_map_single[source[pos - 1]]);
    }

    if (alnumeric_set.contains(source[pos])) { // keywords & identifiers
      std::uintptr_t start_pos = pos;
      std::uintptr_t start_column = column;

      while (pos < source.length() && alnumeric_set.contains(source[pos])) {
        pos++;
        column++;
      }

      std::string lexeme = source.substr(start_pos, pos - start_pos);

      if (keyword_map.contains(lexeme)) {
        return token_t(line, start_column, lexeme, keyword_map[lexeme]);
      } else {
        return token_t(line, start_column, lexeme, identifier_tt);
      }
    }

    return token_t(line, column, "unknown token", unkown_tt);
  }
  
  std::vector<token_t> lexer::analyze() {
    std::vector<token_t> token_stream;
    token_t token = get_next();
    token_type previous_token_type = end_of_file_tt;
    
    auto is_unary_context = [](token_type prev_type) {
      return prev_type == assignment_tt || prev_type == left_paren_tt ||
             prev_type == comma_tt || prev_type == semicolon_tt ||
             prev_type == end_of_file_tt;
      };
    
    while (token.tt != end_of_file_tt) {
#if DONT_USE_WHITESPACES
      if (token.tt == whitespace_tt) {
        token = get_next();
        continue;
      }
#endif

#if DONT_USE_COMMENTS
      if (token.tt == comment_tt || token.tt == multiline_comment_tt) {
        token = get_next();
        continue;
      }
#endif
      
      if (token.tt == add_operator_tt || token.tt == subtract_operator_tt) {
        if (is_unary_context(previous_token_type)) {
          token.tt = (token.tt == add_operator_tt) ? unary_plus_operator_tt : unary_minus_operator_tt;
        }
        
        if (token.tt == unary_plus_operator_tt) {
          token.lexeme = "unary_plus";
        }
        else if (token.tt == unary_minus_operator_tt) {
          token.lexeme = "unary_minus";
        }
      }
      
      token_stream.push_back(token);
      previous_token_type = token.tt;
      token = get_next();
    }

    token_stream.push_back(token);

    stream = token_stream;
    
    return token_stream;
  }
  
  void lexer::visualize(std::optional<std::vector<token_t>> o_s) {
    if (!o_s.has_value()) {
      for (auto s : stream) {
        std::println("lexeme: {}\ntype: {}\n", s.lexeme, occult::token_t::get_typename(s.tt));
      
      }
    }
    else {
      for (auto s : o_s.value()) {
        std::println("lexeme: {}\ntype: {}\n", s.lexeme, occult::token_t::get_typename(s.tt));
      
      }
    }
  }
} // namespace occult
