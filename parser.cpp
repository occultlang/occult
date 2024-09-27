#include "parser.hpp"

/*
 * TODO
 * Function Calls : can't have its own function, has to be inside every function
 * Assignments
   * Strings
   * Integers
   * Floats
   * Bools
   * Chars
   * RPN for ints + floats
 * Conditionals (if else while, etc) & A notation for that (RPN variant?)
 * Return
 * For
 * Other keywords
 */

namespace occult {
  token_t parser::peek(std::uintptr_t pos) {
    return stream[this->pos + pos];
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
          
          match(peek(), comma_tt);
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
  
  std::unique_ptr<ast_assignment> parser::parse_assignment() { 
    
  }
  
  std::unique_ptr<ast_datatype> parser::parse_datatype() { 
    if (match(peek(), int32_keyword_tt)) {
      auto int32_node = ast::new_node<ast_datatype>();
      int32_node->content = previous().lexeme;
      
      if (peek().tt == identifier_tt) {
        int32_node->add_child(parse_identifier());
      }
        
      if (peek().tt == assignment_tt) {
        int32_node->add_child(parse_assignment());
      }
      
      match(peek(), semicolon_tt);
        
      return int32_node;
    }
    else {
      throw runtime_error("Invalid datatype", peek());
    }
  }
  
  std::unique_ptr<ast_literal> parser::parse_literal() { // maybe do matching here?
    auto literal = ast::new_node<ast_literal>();
    literal->content = peek().lexeme;
    pos++;
    
    return literal;
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
    if (peek().tt == identifier_tt && peek(1).tt == left_paren_tt) {
      auto func_call_node = ast::new_node<ast_functioncall>();
      
      func_call_node->add_child(parse_identifier());
      
      consume(); // consume left paren
      
      auto function_args_node = ast::new_node<ast_functionargs>();
      
      std::stack<std::unique_ptr<ast>> operands;
      
      while (!match(peek(), right_paren_tt)) {
        if (peek().tt == identifier_tt && peek(1).tt == left_paren_tt) {
          // function call as argument. 
        }
        // do with float as well \/
        else if (peek().tt == number_literal_tt && (peek(1).tt == right_paren_tt || peek(1).tt == comma_tt)) { // number literal NON EXPR
          auto operand = parse_literal();
          function_args_node->add_child(std::move(operand));
        }
        else {
          while (!match(peek(), comma_tt)) {
            if (peek().tt == number_literal_tt) { // FOR RPN NUMBER LITERALS
              auto operand = parse_literal();
              operands.push(std::move(operand));
            }
            // other things like strings etc.
            else {
              if (operands.size() < 2) {
                throw runtime_error("Insufficient operands for operator", peek());
              }
              
              auto right = std::move(operands.top()); operands.pop();
              auto left = std::move(operands.top()); operands.pop();
              
              auto binaryexpr = ast::new_node<ast_binaryexpr>();
              binaryexpr->content = peek().lexeme; 
              pos++; 
              
              binaryexpr->add_child(std::move(left));
              binaryexpr->add_child(std::move(right));
              operands.push(std::move(binaryexpr));
              
              function_args_node->add_child(std::move(operands.top()));
            }
          }
        }
        
        match(peek(), comma_tt);
      }
      
      if (!match(peek(), semicolon_tt)) {
        throw runtime_error("Expected semicolon", peek());
      }
    
      func_call_node->add_child(std::move(function_args_node));
      
      return func_call_node;
    }
    else if (match(peek(), if_keyword_tt)) {
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
    else if (match(peek(), continue_keyword_tt)) {
      return parse_continue();
    }
    else if (match(peek(), break_keyword_tt)) {
      return parse_break();
    }
    else if (match(peek(), return_keyword_tt)) {
      return parse_return();
    }
    else if (match(peek(), int32_keyword_tt)) {
      pos--;
      return parse_datatype();
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
  
  std::unique_ptr<ast_continuestmt> parser::parse_continue() {
    
  }
  
  std::unique_ptr<ast_breakstmt> parser::parse_break() {
    
  }
  
  std::unique_ptr<ast_returnstmt> parser::parse_return() {
    
  }
  
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
