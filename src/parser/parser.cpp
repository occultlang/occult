#include "parser.hpp"
#include "parser_maps.hpp"

#define DEVELOP false

namespace occult {
  token_t parser::peek(std::uintptr_t pos) {
    return stream[this->pos + pos];
  }

  token_t parser::previous() {
    if ((pos - 1) != 0) {
      return stream[pos - 1];
    }
    else {
      throw std::runtime_error("out of bounds parser::previous");
    }
  }

  void parser::consume(std::uintptr_t amt) {
    pos += amt;
  }

  bool parser::match(token_t t, token_type tt) {
    if (t.tt == tt) {
      return true;
    }
    else {
      return false;
    }
  }
  
  /*
   * parses a given stream of tokens and turns it into reverse polish notation
   * handles function calls as well
  */
  std::vector<std::unique_ptr<ast>> parser::parse_expression(std::vector<token_t> expr) {
    std::vector<std::unique_ptr<ast>> expr_ast;
    std::stack<token_t> operator_stack;
    
    auto is_end = false;
    
    for (std::size_t i = 0; i < expr.size() && !is_end; i++) {
      auto t = expr.at(i);
      
      switch(t.tt) {
        case semicolon_tt:
        case left_curly_bracket_tt: {
          is_end = true; // marking the end of the statement
          break;
        }
        
        case identifier_tt: {
          if (expr.at(i + 1).tt == left_paren_tt) { // function calls
            i++;
            
            auto start_node = ast_map[function_call_parser_tt]("start_call"); // start call
            
            start_node->add_child(ast_map[t.tt](t.lexeme)); // function name
            
            auto paren_depth = 1;
            std::vector<token_t> current_args;
            
            while(i + 1 < expr.size() && 0 < paren_depth) {
              i++;
              
              auto current_token = expr.at(i);
              
              if (current_token.tt == left_paren_tt) {
                paren_depth++;
                
                current_args.push_back(current_token);
              } 
              else if (current_token.tt == right_paren_tt) {
                paren_depth--;
                if (paren_depth > 0) {
                  current_args.push_back(current_token); 
                }
              }
              else if (current_token.tt == comma_tt && paren_depth == 1) {
                auto arg_node = ast::new_node<ast_functionarg>();
                
                if (current_args.size() == 1 && current_args[0].tt == identifier_tt) {
                  arg_node->add_child(ast_map[identifier_tt](current_args.at(0).lexeme));
                }
                else {
                  auto parsed_args = parse_expression(current_args);
                  
                  for (auto& c : parsed_args) {
                    arg_node->add_child(std::move(c));
                  }
                }
                
                start_node->add_child(std::move(arg_node));
                
                current_args.clear();
              }
              else {
                current_args.push_back(current_token);
              }
            }
            
            if (!current_args.empty()) {
              auto arg_node = ast::new_node<ast_functionarg>();
              
              if (current_args.size() == 1 && current_args[0].tt == identifier_tt) {
                arg_node->add_child(ast_map[identifier_tt](current_args.at(0).lexeme));
              }
              else {
                auto parsed_args = parse_expression(current_args);
                
                for (auto& c : parsed_args) {
                  arg_node->add_child(std::move(c));
                }
              }
              
              start_node->add_child(std::move(arg_node));
            }
            
            auto end_node = ast_map[function_call_parser_tt]("end_call"); // start call
            start_node->add_child(std::move(end_node));

            expr_ast.push_back(std::move(start_node));
          }
          else {
            expr_ast.push_back(ast_map[t.tt](t.lexeme));
          }
          
          break;
        }
        
        case number_literal_tt: 
        case float_literal_tt: 
        case string_literal_tt:
        case char_literal_tt:
        case false_keyword_tt:
        case true_keyword_tt:
        case unary_bitwise_not_tt:
        case unary_minus_operator_tt:
        case unary_plus_operator_tt:
        case unary_not_operator_tt: {
          expr_ast.push_back(ast_map[t.tt](t.lexeme));
          break;
        }
        
        case left_paren_tt: {
          operator_stack.push(t);
          break;
        }
        
        case right_paren_tt: {
          while(!operator_stack.empty()) {
            t = operator_stack.top();
            
            operator_stack.pop();
            
            if (t.tt == left_paren_tt) {
              break;
            }
            
            expr_ast.push_back(ast_map[t.tt](t.lexeme));
          }
          
          break;
        }
        
        default: {
          while(!operator_stack.empty() && precedence_map[t.tt] >= precedence_map[operator_stack.top().tt]) {
            expr_ast.push_back(ast_map[operator_stack.top().tt](operator_stack.top().lexeme));
            
            operator_stack.pop();
          }
          
          operator_stack.push(t);
          
          break;
        }
      }
    }
    
    while (!operator_stack.empty()) {
      expr_ast.push_back(ast_map[operator_stack.top().tt](operator_stack.top().lexeme));
      operator_stack.pop();
    }
    
    return expr_ast;
  }
  
  std::unique_ptr<ast> parser::parse_datatype() {     
   auto it = datatype_map.find(peek().tt);
   
   if (it != datatype_map.end()) {
     consume();  
     auto node = it->second();  // create the ast node
     
     if (peek().tt == identifier_tt) {
       node->add_child(parse_identifier());
     }
     
     return node;
   }
   else {
     throw runtime_error("invalid datatype", peek(), pos);
   }
  }

  std::unique_ptr<ast_identifier> parser::parse_identifier() {
    consume();

    auto node = ast::new_node<ast_identifier>();

    node->content = previous().lexeme;

    return node;
  }

  std::unique_ptr<ast_function> parser::parse_function() {
    auto func_node = ast::new_node<ast_function>();
    
    consume(); // consume function keyword
    
    auto name = parse_identifier();
    
    func_node->add_child(std::move(name));
    
    if (match(peek(), left_paren_tt)) {
      consume();
      
      auto func_args_node = ast::new_node<ast_functionargs>();
      
      while (!match(peek(), right_paren_tt)) {
        auto arg = parse_datatype();
        
        func_args_node->add_child(std::move(arg));
        
        if (match(peek(), comma_tt)) {
          consume();
        }
      }
      
      if (!match(peek(), right_paren_tt)) {
        throw runtime_error("expected right parenthesis", peek(), pos);
      }
      else {
        consume();
      }
      
      func_node->add_child(std::move(func_args_node));
    }
    else {
      throw runtime_error("expected left parenthesis", peek(), pos);
    }
    
    auto return_type = parse_datatype();
    func_node->add_child(std::move(return_type));
    
    auto body = parse_block();  
    func_node->add_child(std::move(body));
    
    return func_node;
  }
  
  std::unique_ptr<ast_block> parser::parse_block() { 
    if (match(peek(), left_curly_bracket_tt)) {
      consume();
      
      auto block_node = ast::new_node<ast_block>();
      
      while (!match(peek(), right_curly_bracket_tt)) {
        block_node->add_child(parse_keyword()); 
      }
      
      if (!match(peek(), right_curly_bracket_tt)) {
        throw runtime_error("expected right curly brace", peek(), pos);
      }
      else {
        consume();
      }
      
      return block_node;
    }
    else {
      throw runtime_error("expected left curly brace", peek(), pos);
    }
  }

  std::unique_ptr<ast_assignment> parser::parse_assignment() {
    consume();

    auto node = ast::new_node<ast_assignment>();

    return node;
  }
  
  template<typename IntegerAstType>
  std::unique_ptr<IntegerAstType> parser::parse_integer_type() {
    consume(); // consume keyword

    auto node = ast::new_node<IntegerAstType>();

    if (match(peek(), identifier_tt)) {
      node->add_child(parse_identifier()); // add identifier as a child node
    }
    else {
      throw runtime_error("expected identifier", peek(), pos);
    }

    if (match(peek(), assignment_tt)) {
      node->add_child(parse_assignment());
      
      if (match(peek(), semicolon_tt)) {
        throw runtime_error("expected expression", peek(), pos);
      }
      
      auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
      pos += first_semicolon_pos;
      auto converted_rpn = parse_expression(sub_stream);
      
      for (auto &c : converted_rpn) { // adding all the children of the converted expression into the integer node
        node->get_children().at(1)->add_child(std::move(c));
      }
    }

    if (match(peek(), semicolon_tt)) { // end of declaration
      consume();
    }
    else {
      throw runtime_error("expected semicolon", peek(), pos);
    }

    return node;
  }
  
  std::unique_ptr<ast_returnstmt> parser::parse_return() {
    consume();
    
    auto return_node = ast::new_node<ast_returnstmt>();
    
    auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1}; 
    pos += first_semicolon_pos;
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto &c : converted_rpn) { 
      return_node->add_child(std::move(c));
    }
    
    if (match(peek(), semicolon_tt)) { 
      consume();
    }
    else {
      throw runtime_error("expected semicolon", peek(), pos);
    }
    
    return return_node;
  }
  
  std::unique_ptr<ast_ifstmt> parser::parse_if() {
    consume(); // consume if
    
    auto if_node = ast::new_node<ast_ifstmt>();
 
    auto first_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos + 1};
    pos += first_bracket_pos;
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto &c : converted_rpn) { 
      if_node->add_child(std::move(c));
    }
    
    auto body = parse_block();  
    if_node->add_child(std::move(body));
    
    while (match(peek(), elseif_keyword_tt)) {
      if_node->add_child(parse_elseif());
    }
    
    if (match(peek(), else_keyword_tt)) {
      if_node->add_child(parse_else());
    }
    
    return if_node;
  }
  
  std::unique_ptr<ast_elseifstmt> parser::parse_elseif() {
    consume(); 
    
    auto elseif_node = ast::new_node<ast_elseifstmt>();

    auto first_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos + 1};
    pos += first_bracket_pos;
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto &c : converted_rpn) { 
      elseif_node->add_child(std::move(c));
    }
    
    auto body = parse_block();  
    elseif_node->add_child(std::move(body));
    
    return elseif_node;
  }
  
  std::unique_ptr<ast_elsestmt> parser::parse_else() {
    consume(); 
    
    auto else_node = ast::new_node<ast_elsestmt>();
    
    auto body = parse_block();  
    else_node->add_child(std::move(body));
    
    return else_node;
  }
  
  std::unique_ptr<ast_loopstmt> parser::parse_loop() {
    consume(); 
    
    auto loop_node = ast::new_node<ast_loopstmt>();
    
    auto body = parse_block();  
    loop_node->add_child(std::move(body));
    
    return loop_node;
  }
  
  std::unique_ptr<ast_breakstmt> parser::parse_break() {
    consume(); 
    
    auto break_node = ast::new_node<ast_breakstmt>();
    
    if (match(peek(), semicolon_tt)) {
      consume();
    }
    else {
      throw runtime_error("expected semicolon", peek(), pos);
    }
    
    return break_node;
  }
  
  std::unique_ptr<ast_continuestmt> parser::parse_continue() {
    consume(); 
    
    auto continue_node = ast::new_node<ast_continuestmt>();
    
    if (match(peek(), semicolon_tt)) {
      consume();
    }
    else {
      throw runtime_error("expected semicolon", peek(), pos);
    }
    
    return continue_node;
  }
  
  std::unique_ptr<ast_whilestmt> parser::parse_while() {
    consume(); 
    
    auto while_node = ast::new_node<ast_whilestmt>();
    
    auto first_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos + 1};
    pos += first_bracket_pos;
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto &c : converted_rpn) { 
      while_node->add_child(std::move(c));
    }
    
    auto body = parse_block();  
    while_node->add_child(std::move(body));
    
    return while_node;
  }
  
  std::unique_ptr<ast_string> parser::parse_string() {
    consume(); // consume string keyword
    
    auto node = ast::new_node<ast_string>();
    
    if (match(peek(), identifier_tt)) {
      node->add_child(parse_identifier()); // add identifier as a child node
    }
    else {
      throw runtime_error("expected identifier", peek(), pos);
    }
    
    if (match(peek(), assignment_tt)) {
      auto assignment = parse_assignment();
      
      if (match(peek(), semicolon_tt)) {
        throw runtime_error("expected expression", peek(), pos);
      }
      
      auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
      pos += first_semicolon_pos;
      auto converted_rpn = parse_expression(sub_stream);
      
      for (auto &c : converted_rpn) { 
        assignment->add_child(std::move(c));
      }
      
      node->add_child(std::move(assignment));
    }
    
    if (match(peek(), semicolon_tt)) { // end of declaration
      consume();
    }
    else {
      throw runtime_error("expected semicolon", peek(), pos);
    }
    
    return node;
  }
  
  std::unique_ptr<ast_forstmt> parser::parse_for() { // for expr; in expr; { }
    consume();
    
    auto for_node = ast::new_node<ast_forstmt>();

    auto in_pos = find_first_token(stream.begin() + pos, stream.end(), in_keyword_tt); // we're going to insert a semicolon
    stream.insert(stream.begin() + pos + in_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
    
    for_node->add_child(parse_keyword()); // first expr
    
    if (match(peek(), in_keyword_tt)) {
      consume();
      
      if (match(peek(), identifier_tt) && peek(1).tt != left_paren_tt) {
        for_node->add_child(parse_identifier());
      }
      else {
        auto left_curly_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt); 
        stream.insert(stream.begin() + pos + left_curly_bracket_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
        
        for_node->add_child(parse_keyword()); // 2nd expr
      }
      
      auto body = parse_block();  
      for_node->add_child(std::move(body));
      
      return for_node;
    }
    else {
      throw runtime_error("expected in keyword", peek(), pos);
    }
  }

  std::unique_ptr<ast> parser::parse_keyword(bool nested_function) {
    if (nested_function) {
      if (match(peek(), function_keyword_tt)) {
        return parse_function();
      }
    }
    
    if (match(peek(), int8_keyword_tt)) {
      return parse_integer_type<ast_int8>();
    }
    else if (match(peek(), int16_keyword_tt)) {
      return parse_integer_type<ast_int16>();
    }
    else if (match(peek(), int32_keyword_tt)) {
      return parse_integer_type<ast_int32>();
    }
    else if (match(peek(), int64_keyword_tt)) {
      return parse_integer_type<ast_int64>();
    }
    else if (match(peek(), uint8_keyword_tt)) {
      return parse_integer_type<ast_uint8>();
    }
    else if (match(peek(), uint16_keyword_tt)) {
      return parse_integer_type<ast_uint16>();
    }
    else if (match(peek(), uint32_keyword_tt)) {
      return parse_integer_type<ast_uint32>();
    }
    else if (match(peek(), uint64_keyword_tt)) {
      return parse_integer_type<ast_uint64>();
    }
    else if (match(peek(), float32_keyword_tt)) {
      return parse_integer_type<ast_float32>();
    }
    else if (match(peek(), float64_keyword_tt)) {
      return parse_integer_type<ast_float64>();;
    }
    else if (match(peek(), string_keyword_tt)) {
      return parse_string();
    }
    else if (match(peek(), char_keyword_tt)) {
      return parse_integer_type<ast_int8>();
    }
    else if (match(peek(), boolean_keyword_tt)) {
      return parse_integer_type<ast_int8>();
    }
    else if (match(peek(), if_keyword_tt)) {
      return parse_if();
    }
    else if (match(peek(), loop_keyword_tt)) {
      return parse_loop();
    }
    else if (match(peek(), break_keyword_tt)) {
      return parse_break();
    }
    else if (match(peek(), continue_keyword_tt)) {
      return parse_continue();
    }
    else if (match(peek(), while_keyword_tt)) {
      return parse_while();
    }
    else if (match(peek(), for_keyword_tt)) {
      return parse_for();
    }
    else if (match(peek(), return_keyword_tt)) {
      return parse_return();
    }
    else if (match(peek(), identifier_tt) && peek(1).tt == left_paren_tt) { // fn call
      auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
      pos += first_semicolon_pos;
      auto converted_rpn = parse_expression(sub_stream);
      
      if (converted_rpn.size() == 1 && converted_rpn.at(0)->content == "start_call") {
        if (match(peek(), semicolon_tt)) { 
          consume();
        }
        else {
          throw runtime_error("expected semicolon", peek(), pos);
        }
        
        return std::move(converted_rpn.at(0));
      }
      else {
        throw runtime_error("something is wrong with the function call no idea what though :)", peek(), pos);
      }
    }
    else if (match(peek(), identifier_tt)) {
      auto id = parse_identifier();
      
      if (match(peek(), assignment_tt)) {
        id->add_child(parse_assignment());
        
        if (match(peek(), semicolon_tt)) {
          throw runtime_error("expected expression", peek(), pos);
        }
        
        auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
        std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
        pos += first_semicolon_pos;
        auto converted_rpn = parse_expression(sub_stream);
        
        for (auto &c : converted_rpn) { 
          id->add_child(std::move(c));
        }
        
        if (match(peek(), semicolon_tt)) { 
          consume();
        }
        else {
          throw runtime_error("expected semicolon", peek(), pos);
        }
        
        return id;
      }
    }
    
    throw runtime_error("unexpected keyword", peek(), pos);
  }

  std::unique_ptr<ast_root> parser::parse() {
    while (!match(peek(), end_of_file_tt)) {
      root->add_child(parse_keyword(true)); // not nested

      if (match(peek(), end_of_file_tt)) {
        break;
      }
    }

    return std::move(root);
  }
} // namespace occult
