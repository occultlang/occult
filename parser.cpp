#include "parser.hpp"

// can we implement a pre-allocated heap for occult to use as a virutal memory space to prevent buffer overflows

namespace occult {
  token_t parser::peek() {
    return stream[pos];
  }
  
  token_t parser::previous() {
    if ((pos - 1) != 0) {
      return stream[pos - 1];
    }
    else {
      throw std::runtime_error("Out of bounds parser::previous");
    }
  }
  
  void parser::consume() {
    pos++;
  }
  
  bool parser::match(token_t t, token_type tt) {
    if (t.tt == tt) {
      consume();
      
      return true;
    }
    else {
      return false;
    }
  }
  
  std::unique_ptr<ast_function> parser::parse_function() {
    if (match(peek(), function_keyword_tt)) {
      auto function_node = ast::new_node<ast_function>();
      
      auto identifier_node = parse_identifier();
      function_node->add_child(std::move(identifier_node));
      
      if (match(peek(), left_paren_tt)) {
        auto function_args_node = ast::new_node<ast_functionargs>();
        
        while (!match(peek(), right_paren_tt)) {
          auto arg = parse_datatype();
          
          function_args_node->add_child(std::move(arg));
        }
        
        function_node->add_child(std::move(function_args_node));
        
        auto return_type = parse_datatype();
        function_node->add_child(std::move(return_type));
        
        auto body = parse_block();  
        function_node->add_child(std::move(body));
      }
      else
        throw runtime_error("Expected left parenthesis", peek());
      
      return function_node;
    }
    else
      throw runtime_error("Expected function keyword", peek());
  }
  
  std::unique_ptr<ast_block> parser::parse_block() { // i think this is done
    if (match(peek(), left_curly_bracket_tt)) {
      auto block_node = ast::new_node<ast_block>();
      
      while (!match(peek(), right_curly_bracket_tt)) {
        block_node->add_child(parse_keyword()); 
      }
      
      return block_node;
    }
    else {
      throw runtime_error("Can't find left curly bracket", peek());
    }
  }
  
  std::unique_ptr<ast_datatype> parser::parse_datatype() { 
    if (match(peek(), int32_keyword_tt)) {
      auto int32_node = ast::new_node<ast_datatype>();
      int32_node->content = previous().lexeme;
      
      if (peek().tt == identifier_tt) {
        int32_node->add_child(parse_identifier());
      }
        
      return int32_node;
    }
    else {
      throw runtime_error("Invalid datatype", peek());
    }
  }
  
  std::unique_ptr<ast_binaryexpr> parser::parse_binaryexpr() {
    // TODO
  }
  
  std::unique_ptr<ast_literal> parser::parse_number_literal() {
    
  }
  
  std::unique_ptr<ast_identifier> parser::parse_identifier() {
    if (match(peek(), identifier_tt)) {
      auto identifier = ast::new_node<ast_identifier>();
      identifier->content = previous().lexeme;
      
      return identifier;
    }
    else {
      throw runtime_error("Expected identifier", peek());
    }
  }
  
  std::unique_ptr<ast> parser::parse_keyword() { // parsing keywords
    if (match(peek(), if_keyword_tt)) {
      return parse_if();
    }
    else if (match(peek(), elseif_keyword_tt)) {
      return parse_elseif();
    }
    else if (match(peek(), else_keyword_tt)) {
      return parse_else();
    }
    else if (match(peek(), loop_keyword_tt)) {
      return parse_loop();
    }
    else if (match(peek(), while_keyword_tt)) {
      return parse_while();
    }
    else if (match(peek(), for_keyword_tt)) {
      return parse_for();
    }
    else if (match(peek(), match_keyword_tt)) {
      return parse_match();
    }
    else if (match(peek(), case_keyword_tt)) {
      return parse_case();
    }
    else if (match(peek(), default_keyword_tt)) {
      return parse_defaultcase();
    }
    else if (match(peek(), continue_keyword_tt)) {
      return parse_continue();
    }
    else if (match(peek(), break_keyword_tt)) {
      return parse_break();
    }
    else if (match(peek(), return_keyword_tt)) {
      return parse_return();
    }
    else if (match(peek(), number_literal_tt)) {
      return parse_number_literal();
    }
    else {
      throw runtime_error("Can't find keyword", peek());
    }
  }
  
  std::unique_ptr<ast_ifstmt> parser::parse_if() {
    
  }
  
  std::unique_ptr<ast_elseifstmt> parser::parse_elseif() {
    
  }
  
  std::unique_ptr<ast_elsestmt> parser::parse_else() {
    
  }
  
  std::unique_ptr<ast_loopstmt> parser::parse_loop() {
    
  }
  
  std::unique_ptr<ast_whilestmt> parser::parse_while() {
    
  }
  
  std::unique_ptr<ast_forstmt> parser::parse_for() {
    
  }
  
  std::unique_ptr<ast_matchstmt> parser::parse_match() {
    
  }
  
  std::unique_ptr<ast_caseblock> parser::parse_case() {
    
  }
  
  std::unique_ptr<ast_defaultcase> parser::parse_defaultcase() {
    
  }
  
  std::unique_ptr<ast_continuestmt> parser::parse_continue() {
    
  }
  
  std::unique_ptr<ast_breakstmt> parser::parse_break() {
    
  }
  
  std::unique_ptr<ast_returnstmt> parser::parse_return() {
    
  }
  
  /*std::unique_ptr<ast_instmt> parser::parse_in() {
    
  }*/
  
  std::unique_ptr<ast_root> parser::parse() {
    while(!match(peek(), end_of_file_tt)) {
      root->add_child(parse_function());
      
      if (match(peek(), end_of_file_tt)) {
        break;
      }
    }
    
    return std::move(root);
  }
} // namespace occult
