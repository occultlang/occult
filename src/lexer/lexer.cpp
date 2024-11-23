#include "lexer.hpp"
#include "lexer_maps.hpp"
 
namespace occult {
  std::string token_t::get_typename(const token_type& tt) {
    if (token_typename_map.contains(tt))
      return token_typename_map[tt];
    else
      throw std::runtime_error("can't find token typename");
  }
  
  void lexer::increment(const std::uintptr_t& line, const std::uintptr_t& pos, const std::uintptr_t& column) {
    this->line += line;
    this->pos += pos;
    this->column += column;
  }
  
  void lexer::handle_comment() {
    if (const auto& lexeme = std::string() + source[pos] + source[pos + 1]; comment_map.contains(lexeme)) {
      increment(0, 2, 2); // beginning comment
      
      if (comment_map[lexeme] == comment_tt) {
        while (pos < source.size() && source[pos] != '\n' && source[pos] != '\r') {
          increment(0, 1, 1);
        }
      }
      else if (comment_map[lexeme] == multiline_comment_start_tt) {
        while (pos < source.size() && source[pos] != '*' && source[pos + 1] != '/') {
          if (source[pos] == '\n' || source[pos] == '\r') {
            increment(1, 0, 0);
          }
          
          increment(0, 1, 1);
        }
        
        increment(0, 2, 2); // end multiline comment
      }
    }
  }
  
  void lexer::handle_whitespace() {
    if (whitespace_map.contains(source[pos])) {
      if (source[pos] == '\n' || source[pos] == '\r') {
        increment(1, 1, 0);
        column = 1;
      }
      else {
        increment(0, 1, 1);
      }
    }
  }

  token_t lexer::handle_string() {
    increment(0, 1, 1);
  }
  
  token_t lexer::handle_char() {
    
  }
  
  token_t lexer::handle_numeric() {
    
  }
  
  token_t lexer::handle_delimiter() {
    increment(0, 1, 1);
    
    return token_t(line, column, std::string(1, source[pos - 1]), delimiter_map[source[pos - 1]]);
  }
  
  token_t lexer::handle_operator(const bool& is_double) {
    if (is_double) {
      increment(0, 2, 2);
      
      return token_t(line, column, std::string() + source[pos - 2] + source[pos - 1], operator_map_double[std::string() + source[pos - 2] + source[pos - 1]]);
    }
    else {
      increment(0, 1, 1);
      
      return token_t(line, column, std::string(1, source[pos - 1]), operator_map_single[source[pos - 1]]);
    }
  }
  
  token_t lexer::handle_symbol() {
    const auto& start_pos = pos;
    const auto& start_column = column;
    
    while (pos < source.length() && alnumeric_set.contains(source[pos])) {
      increment(0, 1, 1);
    }
    
    const auto& lexeme = source.substr(start_pos, pos - start_pos);
    
    if (keyword_map.contains(lexeme)) {
      return token_t(line, start_column, lexeme, keyword_map[lexeme]);
    }
    else {
      return token_t(line, start_column, lexeme, identifier_tt);
    }
  }

  token_t lexer::get_next_token() {
    if (pos >= source.length()) {
      return token_t(line, column, "end of file", end_of_file_tt);
    }
    
    handle_comment();
    handle_whitespace();
    
    if (source[pos] == '\"') {
      return handle_string();
    }
    
    if (source[pos] == '\'') {
      return handle_char();
    }
    
    if (numeric_set.contains(source[pos])) {
      return handle_numeric();
    }
    
    if (delimiter_map.contains(source[pos])) {
      return handle_delimiter();
    }
    
    if (operator_map_double.contains(std::string() + source[pos] + source[pos + 1])) {
      return handle_operator(true);
    }
    
    if (operator_map_single.contains(source[pos])) {
      return handle_operator(false);
    }
    
    if (alnumeric_set.contains(source[pos])) {
      return handle_symbol();
    }
    
    return token_t(line, column, "unknown token", unkown_tt);
  }
  
  std::vector<token_t> lexer::analyze() {
    std::vector<token_t> token_stream;
    token_t token = get_next_token();
    
    while (token.tt != end_of_file_tt) {            
      token_stream.push_back(token);

      token = get_next_token();
    }

    token_stream.push_back(token);

    stream = token_stream;
    
    return token_stream;
  }
  
  void lexer::visualize(const std::optional<std::vector<token_t>>& o_s) {
    if (!o_s.has_value()) {
      for (size_t i = 0; i < stream.size(); ++i) {
        auto s = stream.at(i); 
        std::println("lexeme: {}\ntype: {}\nposition in stream: {}\n", s.lexeme, occult::token_t::get_typename(s.tt), i);
      }
    }
    else {
      for (size_t i = 0; i < o_s.value().size(); ++i) {
        auto s = o_s.value().at(i);
        std::println("lexeme: {}\ntype: {}\nposition in stream: {}\n", s.lexeme, occult::token_t::get_typename(s.tt), i);
      }
    }
  }
} // namespace occult
