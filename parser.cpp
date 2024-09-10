#include "parser.hpp"

namespace occult {
  token_t parser::peek() {
    return stream[pos];
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
      
      if (match(peek(), left_paren_tt)) {
        auto function_args_node = ast::new_node<ast_functionargs>();
        
        while (!match(peek(), right_paren_tt)) {
          auto literal = parse_literal();
          
          function_args_node->add_child(std::move(literal));
          
          // add more
        }
        
        function_node->add_child(std::move(function_args_node));
      }
      else
        throw runtime_error("Expected left parenthesis", peek());
      
      return function_node;
    }
    else
      throw runtime_error("Expected function keyword", peek());
  }
  
  std::unique_ptr<ast_block> parser::parse_block() {
    
  }
  
  std::unique_ptr<ast_binaryexpr> parser::parse_binaryexpr() {
    
  }
  
  std::unique_ptr<ast_literalexpr> parser::parse_literal() {
    
  }
  
  std::unique_ptr<ast_identifier> parser::parse_identifier() {
    
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
  
  std::unique_ptr<ast_instmt> parser::parse_in() {
    
  }
  
  std::unique_ptr<ast_root> parse() {
    
  }
} // namespace occult
