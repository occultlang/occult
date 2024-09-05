#include "lexer.hpp"
#include "lexer_maps.hpp"
/*
 * The reason for all of the maps & sets is to avoid using C library functions like isalnum, isdigit etc.
 *
 * TODO: floating points + scientific notation, hexadecimal, octal and binary for numbers
 * */

#define DONT_USE_WHITESPACES 1
#define DONT_USE_COMMENTS 1

namespace occult {
  std::string token_t::get_typename(token_type tt) {
    if (token_typename_map.contains(tt))
      return token_typename_map[tt];
    else
      throw std::runtime_error("Can't find token typename");
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

    // chatgpt helped me with the string literal escape sequencing logic, credits to openai
    // i just put it in a lambda because its simpler

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

    // float literal

    // int
    if (numeric_set.contains(source[pos])) {
      std::string lexeme = "";

      while (pos < source.size() && numeric_set.contains(source[pos])) {
        lexeme += source[pos];
        pos++;
        column++;
      }

      return token_t(line, column, lexeme, number_literal_tt);
    }

    // delimiters
    if (delimiter_map.contains(source[pos])) {
      pos++;
      column++;

      return token_t(line, column, std::string(1, source[pos - 1]), delimiter_map[source[pos - 1]]);
    }

    // double operators
    if (operator_map_double.contains(std::string() + source[pos] + source[pos + 1])) {
      pos += 2;
      column += 2;

      return token_t(line, column, std::string() + source[pos - 2] + source[pos - 1], operator_map_double[std::string() + source[pos - 2] + source[pos - 1]]);
    }

    // single operators
    if (operator_map_single.contains(source[pos])) {
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

      token_stream.push_back(token);
      token = get_next();
    }

    token_stream.push_back(token);

    return token_stream;
  }

} // namespace occult
