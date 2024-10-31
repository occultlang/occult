#include "parser.hpp"
#include "parser_maps.hpp"

namespace occult {
  token_t parser::peek(std::uintptr_t pos) {
    return stream[this->pos + pos];
  }

  token_t parser::previous() {
    if ((pos - 1) != 0) {
      return stream[pos - 1];
    } else {
      throw std::runtime_error("out of bounds parser::previous");
    }
  }

  void parser::consume() {
    pos++;
  }

  bool parser::match(token_t t, token_type tt) {
    if (t.tt == tt) {
      return true;
    } else {
      return false;
    }
  }

  bool parser::match_and_consume(token_t t, token_type tt) {
    if (match(t, tt)) {
      consume();

      return true;
    } else {
      return false;
    }
  }
  
  std::vector<token_t> parser::to_rpn(std::vector<token_t> expr) {
    std::stack<token_t> operator_stack;
    std::vector<token_t> rpn_output;

    for (std::size_t i = 0; i < expr.size(); i++) {
      auto t = expr.at(i);

      if (t.tt == semicolon_tt || t.tt == left_curly_bracket_tt) { // stop if semicolon
        break;
      }
      
      if (t.tt == identifier_tt && expr.at(i + 1).tt == left_paren_tt) {
        token_t start_call_token(t.line, t.column, "start_call", function_call_parser_tt);
        rpn_output.push_back(start_call_token);
        
        // push the function identifier/name
        rpn_output.push_back(t);
        
        // identifier and left parenthesis
        i += 2; 
      
        std::vector<token_t> call_args;
        int paren_depth = 1; // track depth to handle nested calls
      
        while (i < expr.size() && paren_depth > 0) {
          if (expr.at(i).tt == left_paren_tt) {
            paren_depth++;
            
            call_args.push_back(expr.at(i));
          }
          else if (expr.at(i).tt == right_paren_tt) {
            paren_depth--;
            if (paren_depth > 0) {
              call_args.push_back(expr.at(i));
            }
          }
          else if (expr.at(i).tt == comma_tt && paren_depth == 1) {
            std::vector<token_t> parsed_arg = to_rpn(call_args);
            rpn_output.insert(rpn_output.end(), parsed_arg.begin(), parsed_arg.end());
            
            rpn_output.push_back(expr.at(i)); // push comma to output to differentiate between arguments
            call_args.clear(); 
          }
          else {
            call_args.push_back(expr.at(i));
          }
          
          i++;
        }
        
        // parse the final argument if there is one
        if (!call_args.empty()) {
          std::vector<token_t> parsed_arg = to_rpn(call_args);
          rpn_output.insert(rpn_output.end(), parsed_arg.begin(), parsed_arg.end());
        }
      
        token_t end_call_token(t.line, t.column, "end_call", function_call_parser_tt);
        rpn_output.push_back(end_call_token);
      }
      else if (t.tt == number_literal_tt || t.tt == float_literal_tt || t.tt == identifier_tt || t.tt == string_literal_tt || t.tt == char_literal_tt || t.tt == false_keyword_tt || t.tt == true_keyword_tt) {
        rpn_output.push_back(t);
      }
      else if (is_unary(t.tt)) {
        operator_stack.push(t);
      }
      else if (is_binary(t.tt)) {
        while (!operator_stack.empty() && precedence_map[operator_stack.top().tt] <= precedence_map[t.tt]) {
          rpn_output.push_back(operator_stack.top());
          operator_stack.pop();
        }

        operator_stack.push(t);
      }
      else if (t.tt == left_paren_tt) {
        operator_stack.push(t);
      }
      else if (t.tt == right_paren_tt) {
        while (!operator_stack.empty() && operator_stack.top().tt != left_paren_tt) {
          rpn_output.push_back(operator_stack.top());
          operator_stack.pop();
        }

        if (!operator_stack.empty()) {
          operator_stack.pop();
        }
      }
    }

    while (!operator_stack.empty()) {
      rpn_output.push_back(operator_stack.top());
      operator_stack.pop();
    }

    std::vector<token_t> final_rpn_output;

    for (std::size_t i = 0; i < rpn_output.size(); i++) {
      auto t = rpn_output.at(i);

      if (t.tt != left_paren_tt && t.tt != right_paren_tt) {
        final_rpn_output.push_back(t);
      }
    }

    return final_rpn_output;
  }
  
  // converts normal expression into a vector of nodes in RPN
  std::vector<std::unique_ptr<ast>> parser::parse_expression(std::vector<token_t> expr) {
    auto expr_rpn = to_rpn(expr);
    
    if (verbose_parser) {
      for (auto t : expr_rpn) {
        std::println("{}: {}", t.get_typename(t.tt), t.lexeme);
      }
      
      std::println();
    }
    
    std::vector<std::unique_ptr<ast>> expr_ast;
    
    for (const auto& t : expr_rpn) {
      auto it = ast_map.find(t.tt);
      
      if (it != ast_map.end()) {
        auto node = it->second(t.lexeme);
        expr_ast.push_back(std::move(node));
      }
    }

    return expr_ast;
  }
  
  std::unique_ptr<ast> parser::parse_datatype() { 
    std::unordered_map<token_type, std::function<std::unique_ptr<ast>()>> datatype_map = {
      {int8_keyword_tt, []() { return ast::new_node<ast_int8>(); }},
      {int16_keyword_tt, []() { return ast::new_node<ast_int16>(); }},
      {int32_keyword_tt, []() { return ast::new_node<ast_int32>(); }},
      {int64_keyword_tt, []() { return ast::new_node<ast_int64>(); }},
      {uint8_keyword_tt, []() { return ast::new_node<ast_uint8>(); }},
      {uint16_keyword_tt, []() { return ast::new_node<ast_uint16>(); }},
      {uint32_keyword_tt, []() { return ast::new_node<ast_uint32>(); }},
      {uint64_keyword_tt, []() { return ast::new_node<ast_uint64>(); }},
      {float32_keyword_tt, []() { return ast::new_node<ast_float32>(); }},
      {float64_keyword_tt, []() { return ast::new_node<ast_float64>(); }},
      {string_keyword_tt, []() { return ast::new_node<ast_string>(); }},
      {char_keyword_tt, []() { return ast::new_node<ast_int8>(); }},
      {boolean_keyword_tt, []() { return ast::new_node<ast_int8>(); }}};
    
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
        
        match(peek(), comma_tt);
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
    } else {
      throw runtime_error("expected identifier", peek(), pos);
    }

    if (match(peek(), assignment_tt)) {
      node->add_child(parse_assignment());
      
      if (match(peek(), semicolon_tt)) {
        throw runtime_error("expected expression", peek(), pos);
      }
      
     auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
     std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos};
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
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos};
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
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos};
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
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos};
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
    
    return break_node;
  }
  
  std::unique_ptr<ast_continuestmt> parser::parse_continue() {
    consume(); 
    
    auto continue_node = ast::new_node<ast_continuestmt>();
    
    return continue_node;
  }
  
  std::unique_ptr<ast_whilestmt> parser::parse_while() {
    consume(); 
    
    auto while_node = ast::new_node<ast_whilestmt>();
    
    auto first_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_bracket_pos};
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
    else if (match(peek(), return_keyword_tt)) {
      return parse_return();
    }
    else if (match(peek(), identifier_tt) && peek(1).tt == left_paren_tt) { // fn call
      auto fn_call = ast::new_node<ast_functioncall>("body_function_call");
      
      auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos};
      pos += first_semicolon_pos;
      auto converted_rpn = parse_expression(sub_stream);
      
      for (auto &c : converted_rpn) { 
        fn_call->add_child(std::move(c));
      }
      
      if (match(peek(), semicolon_tt)) { 
        consume();
      }
      else {
        throw runtime_error("expected semicolon", peek(), pos);
      }
      
      return fn_call;
    }
    else if (match(peek(), identifier_tt)) {
      auto id = parse_identifier();
      
      if (match(peek(), assignment_tt)) {
        id->add_child(parse_assignment());
        
        if (match(peek(), semicolon_tt)) {
          throw runtime_error("expected expression", peek(), pos);
        }
        
        auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
        std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos};
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
