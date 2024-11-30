#include "parser.hpp"
#include "parser_maps.hpp"

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
  
  // derived partially from https://github.com/kamyu104/LintCode/blob/master/C%2B%2B/convert-expression-to-reverse-polish-notation.cpp
  std::vector<token_t> parser::to_rpn(std::vector<token_t> expr) {
    std::stack<token_t> operator_stack;
    std::vector<token_t> rpn_output;
    
    for (std::size_t i = 0; i < expr.size(); i++) {
      auto t = expr.at(i);
      
      if (t.tt == semicolon_tt || t.tt == left_curly_bracket_tt) { // stop if semicolon
        break;
      }
      
      if (t.tt == identifier_tt && expr.at(i + 1).tt != left_paren_tt) {
        rpn_output.push_back(t);
      }
      else if (t.tt == identifier_tt && expr.at(i + 1).tt == left_paren_tt) {
        i++;
        rpn_output.emplace_back(t.line, t.column, "start_call", function_call_parser_tt);
        rpn_output.push_back(t);
              
        int paren_depth = 1;
        std::vector<token_t> current_arg;
              
        while (i + 1 < expr.size() && paren_depth > 0) {
          i++;
          token_t current_token = expr.at(i);
      
          if (current_token.tt == left_paren_tt) {
            paren_depth++;
            
            current_arg.push_back(current_token);
          } 
          else if (current_token.tt == right_paren_tt) {
            paren_depth--;
            
            if (paren_depth > 0) {
              current_arg.push_back(current_token);
            }
          }
          else if (current_token.tt == comma_tt && paren_depth == 1) {
            if (current_arg.size() == 1 && current_arg[0].tt == identifier_tt) { // check if current_arg contains only a single identifier token
              rpn_output.push_back(current_arg[0]);
            }
            else { // normal rpn conversion
              std::vector<token_t> parsed_arg = to_rpn(current_arg);
              rpn_output.insert(rpn_output.end(), parsed_arg.begin(), parsed_arg.end());
            }
            
            rpn_output.emplace_back(current_token.line, current_token.column, ",", comma_tt);
            current_arg.clear();
          }
          else {
            current_arg.push_back(current_token);
          }
        }
      
        if (!current_arg.empty()) {
          if (current_arg.size() == 1 && current_arg[0].tt == identifier_tt) { // check if the last argument is a single identifier
            rpn_output.push_back(current_arg[0]);
          }
          else { // normal rpn conversion
            std::vector<token_t> parsed_arg = to_rpn(current_arg);
            rpn_output.insert(rpn_output.end(), parsed_arg.begin(), parsed_arg.end());
          }
        }
      
        rpn_output.emplace_back(t.line, t.column, "end_call", function_call_parser_tt);
      }
      else if (is_literal(t.tt)) {
        rpn_output.push_back(t);
      }
      else if (is_unary(t.tt)) { // if unary is broken, its something wrong with unary_context in the lexer.
        rpn_output.push_back(t);
      }
      else if (t.tt == left_paren_tt) {
        operator_stack.push(t); 
      }
      else if (t.tt == right_paren_tt) {
        while(!operator_stack.empty()) {
          t = operator_stack.top();
          operator_stack.pop();
          
          if (t.tt == left_paren_tt) {
            break;
          }
          
          rpn_output.push_back(t);
        }
      }
      else {
        while(!operator_stack.empty() && precedence_map[t.tt] >= precedence_map[operator_stack.top().tt]) {
          rpn_output.push_back(operator_stack.top());
          operator_stack.pop();
        }
        
        operator_stack.push(t);
      }
    }
    
    while (!operator_stack.empty()) {
      rpn_output.push_back(operator_stack.top());
      operator_stack.pop();
    }
    
    if (verbose_parser) {
      std::println("rpn output size: {}", rpn_output.size());
      for (auto t : rpn_output) {
        std::print("{} ", t.lexeme);
      }
      
      std::println();
    }
    
    return rpn_output;
  }
  
  // converts normal expression into a vector of nodes in rpn
  std::vector<std::unique_ptr<ast>> parser::parse_expression(std::vector<token_t> expr) {
    auto expr_rpn = to_rpn(expr);
  
    if (verbose_parser) {
      for (auto t : expr_rpn) {
        std::println("{}: {}", t.get_typename(t.tt), t.lexeme);
      }
      std::println();
    }
  
    std::vector<std::unique_ptr<ast>> expr_ast;
    std::stack<std::unique_ptr<ast>> call_stack;
  
    for (const auto& t : expr_rpn) {
      if (t.tt == function_call_parser_tt && t.lexeme == "start_call") {
        auto call_node = ast::new_node<ast_functioncall>("start_call"); 
  
        call_stack.push(std::move(call_node)); // push it to the stack as the current function call
      }
      else if (t.tt == function_call_parser_tt && t.lexeme == "end_call") { // finalize the current function call node
        auto completed_call = std::move(call_stack.top());
        call_stack.pop();
  
        if (!call_stack.empty()) { // if there's a parent call node, make this a child; otherwise, add to ast root
          call_stack.top()->add_child(std::move(completed_call));
        }
        else {
          expr_ast.push_back(std::move(completed_call));
        }
      }
      else { // regular token handling, add arguments to the current call node if in a function call
        auto it = ast_map.find(t.tt);
        if (it != ast_map.end()) {
          auto arg_node = it->second(t.lexeme);
  
          if (!call_stack.empty()) {
            call_stack.top()->add_child(std::move(arg_node));
          }
          else {
            expr_ast.push_back(std::move(arg_node));
          }
        }
      }
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
      
      for (auto &c : converted_rpn) { // adding all the children of the converted expression into the i8_node
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
    } else {
      throw runtime_error("expected identifier", peek(), pos);
    }
    
    if (match(peek(), assignment_tt)) {
      node->add_child(parse_assignment());
      
      if (match(peek(), semicolon_tt)) {
        throw runtime_error("expected expression", peek(), pos);
      }
      
      consume();
      
      auto stringliteral = ast::new_node<ast_stringliteral>();
      
      stringliteral->content = previous().lexeme;
      
      node->add_child(std::move(stringliteral));
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
        auto left_curly_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt); // we're going to insert a semicolon
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
    else if (match(peek(), elseif_keyword_tt)) {
      return parse_elseif();
    }
    else if (match(peek(), else_keyword_tt)) {
      return parse_else();
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
