#include "parser.hpp"
#include "parser_maps.hpp"
#include <fstream>
#include <sstream>
#include "../lexer/number_parser.hpp"

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
  
  void parser::parse_function_call_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, std::vector<token_t>& expr_ref, token_t& curr_tok_ref, std::size_t& i_ref) {
    i_ref++;
    
    auto start_node = cst_map[function_call_parser_tt]("start_call"); // start call
    
    start_node->add_child(cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme)); // function name
    
    auto paren_depth = 1;
    std::vector<token_t> current_args;
    
    while(i_ref + 1 < expr_ref.size() && 0 < paren_depth) {
      i_ref++;
      
      auto current_token = expr_ref.at(i_ref);
      
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
        auto arg_node = cst::new_node<cst_functionarg>();
        
        if (current_args.size() == 1 && current_args[0].tt == identifier_tt) {
          arg_node->add_child(cst_map[identifier_tt](current_args.at(0).lexeme));
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
      auto arg_node = cst::new_node<cst_functionarg>();
      
      if (current_args.size() == 1 && current_args[0].tt == identifier_tt) {
        arg_node->add_child(cst_map[identifier_tt](current_args.at(0).lexeme));
      }
      else {
        auto parsed_args = parse_expression(current_args);
        
        for (auto& c : parsed_args) {
          arg_node->add_child(std::move(c));
        }
      }
      
      start_node->add_child(std::move(arg_node));
    }
    
    auto end_node = cst_map[function_call_parser_tt]("end_call"); // start call
    start_node->add_child(std::move(end_node));

    expr_cst_ref.push_back(std::move(start_node));
  }

  void parser::parse_array_access_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, std::vector<token_t>& expr_ref, token_t& curr_tok_ref, std::size_t& i_ref) {
    auto array_access_node = cst::new_node<cst_arrayaccess>();
    array_access_node->add_child(cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme));
  
    i_ref++;
    int bracket_depth = 1;
    std::vector<token_t> index_tokens;
  
    while (i_ref + 1 < expr_ref.size() && bracket_depth > 0) {
      i_ref++;
      auto& tok = expr_ref.at(i_ref);
  
      if (tok.tt == left_bracket_tt) {
        if (bracket_depth > 0) {
          auto nested_array_access = cst::new_node<cst_arrayaccess>();
          nested_array_access->add_child(std::move(array_access_node));
          array_access_node = std::move(nested_array_access);
        }
        
        bracket_depth++;
        
        if (bracket_depth > 1) {
          index_tokens.push_back(tok);
        }
      } 
      else if (tok.tt == right_bracket_tt) {
        bracket_depth--;
        if (bracket_depth == 0) {
          if (!index_tokens.empty()) {
            auto index_nodes = parse_expression(index_tokens);
            for (auto& n : index_nodes) {
              array_access_node->add_child(std::move(n));
            }
            index_tokens.clear();
          }
        } 
        else {
          index_tokens.push_back(tok);
        }
      } 
      else if (tok.tt == comma_tt && bracket_depth == 1) {
        if (!index_tokens.empty()) {
          auto index_nodes = parse_expression(index_tokens);
          for (auto& n : index_nodes) {
            array_access_node->add_child(std::move(n));
          }
          index_tokens.clear();
        }
      } 
      else {
        index_tokens.push_back(tok);
      }
    }
  
    expr_cst_ref.push_back(std::move(array_access_node));
  }

  void parser::shunting_yard(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref, token_t& curr_tok_ref) {
    switch (curr_tok_ref.tt) {
      case number_literal_tt:
      case float_literal_tt:
      case string_literal_tt:
      case char_literal_tt:
      case false_keyword_tt:
      case true_keyword_tt:
      case identifier_tt: {    
        auto n = cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme);
        
        expr_cst_ref.push_back(std::move(n));

        break;
      }
      case unary_bitwise_not_tt:
      case unary_minus_operator_tt:
      case unary_plus_operator_tt:
      case unary_not_operator_tt: {
        stack_ref.push(curr_tok_ref); // push unary ops to stack
        
        break;
      }
      case left_paren_tt: {
        stack_ref.push(curr_tok_ref);
        expr_cst_ref.push_back(cst::new_node<cst_expr_end>());
        
        break;
      }
      case right_paren_tt: {
        while (!stack_ref.empty() && stack_ref.top().tt != left_paren_tt) {
          auto n = cst_map[stack_ref.top().tt](stack_ref.top().lexeme);

          expr_cst_ref.push_back(std::move(n));
          stack_ref.pop();
        }
        if (!stack_ref.empty() && stack_ref.top().tt == left_paren_tt) {
          stack_ref.pop(); 
        } 
        else {
          throw parsing_error("matching left parenthesis", curr_tok_ref, pos, std::source_location::current().function_name());
        }
        
        while (!stack_ref.empty() && // pop unary ops after paren
              (stack_ref.top().tt == unary_bitwise_not_tt ||
                stack_ref.top().tt == unary_minus_operator_tt ||
                stack_ref.top().tt == unary_plus_operator_tt ||
                stack_ref.top().tt == unary_not_operator_tt)) {
          expr_cst_ref.push_back(cst_map[stack_ref.top().tt](stack_ref.top().lexeme));
          stack_ref.pop();
        }

        expr_cst_ref.push_back(cst::new_node<cst_expr_start>());

        break;
      }
      default: { // binary ops
        while (!stack_ref.empty() && stack_ref.top().tt != left_paren_tt &&
              precedence_map[curr_tok_ref.tt] > precedence_map[stack_ref.top().tt]) {
          auto n = cst_map[stack_ref.top().tt](stack_ref.top().lexeme);
          
          expr_cst_ref.push_back(std::move(n));
          stack_ref.pop();
        }
        stack_ref.push(curr_tok_ref);

        break;
      }
    }
  }
  
  void parser::shunting_yard_stack_cleanup(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref) {
    while (!stack_ref.empty()) {
      if (stack_ref.top().tt == left_paren_tt) {
        throw parsing_error("unmatched left parenthesis", stack_ref.top(), pos, std::source_location::current().function_name());
      }
      
      expr_cst_ref.push_back(cst_map[stack_ref.top().tt](stack_ref.top().lexeme));
      stack_ref.pop();
    }
  }

  /* 
    after careful consideration, for now, occult will only support binary comparisons for now. no singular comparisons. (e.g !X instead use X == false) 
    although, we can NOT expressions in parenthesis still. but they can NOT be singular. 

    if you think about it, more verbosity is a double edged sword. its a weird language thing for now... until i actually add singular comparisons 
  */
  std::vector<std::unique_ptr<cst>> parser::parse_expression(std::vector<token_t> expr) {
    std::vector<std::unique_ptr<cst>> expr_cst;
    std::stack<token_t> operator_stack;
    
    for (std::size_t i = 0; i < expr.size(); i++) {
      auto t = expr.at(i);

      if (t.tt == semicolon_tt || t.tt == left_curly_bracket_tt || t.tt == comma_tt) {
        break; // end of expr
      } 
      
      else if (t.tt == reference_operator_tt) {
        auto count = 0;

        while (expr.at(i).tt == reference_operator_tt) {
          i++;
          count++;
        }

        i--; // fix align
        
        expr_cst.push_back(cst_map[t.tt](std::to_string(count)));
      }
      else if (t.tt == dereference_operator_tt) {
        auto count = 0;

        while (expr.at(i).tt == dereference_operator_tt) {
          i++;
          count++;
        }

        i--; // fix align

        expr_cst.push_back(cst_map[t.tt](std::to_string(count)));
      }
      /*else if (t.tt == reference_operator_tt || t.tt == dereference_operator_tt) {
        expr_cst.push_back(cst_map[t.tt](t.lexeme));
      }*/
      else if (t.tt == identifier_tt) {
        if (i + 1 < expr.size() && expr.at(i + 1).tt == left_paren_tt) {
          parse_function_call_expr(expr_cst, expr, t, i);
        } 
        else if (i + 1 < expr.size() && expr.at(i + 1).tt == left_bracket_tt) {
          parse_array_access_expr(expr_cst, expr, t, i);
        } 
        else {
          expr_cst.push_back(cst_map[t.tt](t.lexeme));
        }
      }  
      else {
        shunting_yard(operator_stack, expr_cst, t);
      }
    }

    shunting_yard_stack_cleanup(operator_stack, expr_cst);

    return expr_cst;
  }

  std::unique_ptr<cst> parser::parse_datatype() {     
   auto it = datatype_map.find(peek().tt);
   
   if (it != datatype_map.end()) {
     consume();  
     auto node = it->second();  // create the cst node

     if (match(peek(), multiply_operator_tt)) { 
      while (match(peek(), multiply_operator_tt)) {
        consume();
        node->num_pointers++;
      }
     }

     if (peek().tt == identifier_tt) {
       node->add_child(parse_identifier());
     }
     
     return node;
   }
   else if (match(peek(), array_keyword_tt)) {
    consume();

    auto node = cst::new_node<cst_array>();

    auto dimension_count = 0;
    while (match(peek(), left_bracket_tt) && match(peek(1), right_bracket_tt)) {
      consume(2);
      dimension_count++;
    }

    node->add_child(cst::new_node<cst_dimensions_count>(std::to_string(dimension_count)));

    it = datatype_map.find(peek().tt);
    if (it != datatype_map.end()) {
      consume();  
      auto datatype_node = it->second();

      node->add_child(std::move(datatype_node));

      return node;
    }
    else {
      throw parsing_error("<dataype>", peek(), pos, std::source_location::current().function_name());
    }
   }
   else {
     throw parsing_error("<dataype>", peek(), pos, std::source_location::current().function_name());
   }
  }

  std::unique_ptr<cst_identifier> parser::parse_identifier() {
    consume();

    auto node = cst::new_node<cst_identifier>();

    node->content = previous().lexeme;

    return node;
  }

  std::unique_ptr<cst_function> parser::parse_function() {
    auto func_node = cst::new_node<cst_function>();
    
    consume(); // consume function keyword
    
    auto name = parse_identifier();
    
    func_node->add_child(std::move(name));
    
    if (match(peek(), left_paren_tt)) {
      consume();
      
      auto func_args_node = cst::new_node<cst_functionargs>();
      
      while (!match(peek(), right_paren_tt)) {
        auto arg = parse_datatype();
        
        func_args_node->add_child(std::move(arg));
        
        if (match(peek(), comma_tt)) {
          consume();
        }
      }
      
      if (!match(peek(), right_paren_tt)) {
        throw parsing_error(")", peek(), pos, std::source_location::current().function_name());
      }
      else {
        consume();
      }
      
      func_node->add_child(std::move(func_args_node));
    }
    else {
      throw parsing_error("(", peek(), pos, std::source_location::current().function_name());
    }
    
    auto return_type = parse_datatype();
    func_node->add_child(std::move(return_type));
    
    auto body = parse_block();  
    func_node->add_child(std::move(body));
    
    return func_node;
  }
  
  std::unique_ptr<cst_block> parser::parse_block() { 
    if (match(peek(), left_curly_bracket_tt)) {
      consume();
      
      auto block_node = cst::new_node<cst_block>();
      
      while (!match(peek(), right_curly_bracket_tt)) {
        block_node->add_child(parse_keyword()); 
      }
      
      if (!match(peek(), right_curly_bracket_tt)) {
        throw parsing_error("}", peek(), pos, std::source_location::current().function_name());
      }
      else {
        consume();
      }
      
      return block_node;
    }
    else {
      throw parsing_error("{", peek(), pos, std::source_location::current().function_name());
    }
  }

  std::unique_ptr<cst_assignment> parser::parse_assignment() {
    consume();

    auto node = cst::new_node<cst_assignment>();

    return node;
  }

  template<typename ParentNode>
  void parser::parse_expression_until(ParentNode* parent, token_type t) {
    auto first_pos = find_first_token(stream.begin() + pos, stream.end(), t);
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_pos + 1};
    pos += first_pos;
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto& c : converted_rpn) {
      parent->add_child(std::move(c));
    }
  }
  
  template<typename IntegercstType>
  std::unique_ptr<IntegercstType> parser::parse_integer_type() {
    consume(); // consume keyword

    auto node = cst::new_node<IntegercstType>();

    if (match(peek(), multiply_operator_tt)) { 
      while (match(peek(), multiply_operator_tt)) {
        consume();
        node->num_pointers++;
      }
    }

    if (match(peek(), identifier_tt)) {
      node->add_child(parse_identifier()); // add identifier as a child node
    }
    else {
      throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
    }

    if (match(peek(), assignment_tt)) {
      node->add_child(parse_assignment());
      
      if (match(peek(), semicolon_tt)) {
        throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
      }

      parse_expression_until(node->get_children().at(1).get(), semicolon_tt); // parse the expression until the semicolon
    }

    if (match(peek(), semicolon_tt)) { // end of declaration
      consume();
    }
    else {
      throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
    }

    return node;
  }
  
  std::unique_ptr<cst_returnstmt> parser::parse_return() {
    consume();
    
    auto return_node = cst::new_node<cst_returnstmt>();
  
    parse_expression_until(return_node.get(), semicolon_tt); // parse the expression until the semicolon
    
    if (match(peek(), semicolon_tt)) { 
      consume();
    }
    else {
      throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
    }
    
    return return_node;
  }
  
  std::unique_ptr<cst_ifstmt> parser::parse_if() {
    consume(); // consume if
    
    auto if_node = cst::new_node<cst_ifstmt>();

    parse_expression_until(if_node.get(), left_curly_bracket_tt); // parse the expression until the left curly bracket
    
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
  
  std::unique_ptr<cst_elseifstmt> parser::parse_elseif() {
    consume(); 
    
    auto elseif_node = cst::new_node<cst_elseifstmt>();

    parse_expression_until(elseif_node.get(), left_curly_bracket_tt); // parse the expression until the left curly bracket
    
    auto body = parse_block();  
    elseif_node->add_child(std::move(body));
    
    return elseif_node;
  }
  
  std::unique_ptr<cst_elsestmt> parser::parse_else() {
    consume(); 
    
    auto else_node = cst::new_node<cst_elsestmt>();
    
    auto body = parse_block();  
    else_node->add_child(std::move(body));
    
    return else_node;
  }
  
  std::unique_ptr<cst_loopstmt> parser::parse_loop() {
    consume(); 
    
    auto loop_node = cst::new_node<cst_loopstmt>();
    
    auto body = parse_block();  
    loop_node->add_child(std::move(body));
    
    return loop_node;
  }
  
  std::unique_ptr<cst_breakstmt> parser::parse_break() {
    consume(); 
    
    auto break_node = cst::new_node<cst_breakstmt>();
    
    if (match(peek(), semicolon_tt)) {
      consume();
    }
    else {
      throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
    }
    
    return break_node;
  }
  
  std::unique_ptr<cst_continuestmt> parser::parse_continue() {
    consume(); 
    
    auto continue_node = cst::new_node<cst_continuestmt>();
    
    if (match(peek(), semicolon_tt)) {
      consume();
    }
    else {
      throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
    }
    
    return continue_node;
  }
  
  std::unique_ptr<cst_whilestmt> parser::parse_while() {
    consume(); 
    
    auto while_node = cst::new_node<cst_whilestmt>();
 
    parse_expression_until(while_node.get(), left_curly_bracket_tt); // parse the expression until the left curly bracket
    
    auto body = parse_block();  
    while_node->add_child(std::move(body));
    
    return while_node;
  }
  
  std::unique_ptr<cst_string> parser::parse_string() {
    consume(); // consume string keyword
    
    auto node = cst::new_node<cst_string>();
    
    if (match(peek(), identifier_tt)) {
      node->add_child(parse_identifier()); // add identifier as a child node
    }
    else {
      throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
    }
    
    if (match(peek(), assignment_tt)) {
      auto assignment = parse_assignment();
      
      if (match(peek(), semicolon_tt)) {
        throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
      }
    
      parse_expression_until(assignment.get(), semicolon_tt); // parse the expression until the semicolon
      
      node->add_child(std::move(assignment));
    }
    
    if (match(peek(), semicolon_tt)) { // end of declaration
      consume();
    }
    else {
      throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
    }
    
    return node;
  }
  
  std::unique_ptr<cst_forstmt> parser::parse_regular_for(std::unique_ptr<cst_forstmt> existing_for_node) { // for expr when condition do expr {}
    auto when_pos = find_first_token(stream.begin() + pos, stream.end(), when_keyword_tt);
    stream.insert(stream.begin() + pos + when_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
    
    existing_for_node->add_child(parse_keyword());
    
    if (match(peek(), when_keyword_tt)) {      
      consume(); // consume when
      
      auto do_pos = find_first_token(stream.begin() + pos, stream.end(), do_keyword_tt);
      stream.insert(stream.begin() + pos + do_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + do_pos + 2};
      pos += do_pos + 1;
      auto converted_rpn = parse_expression(sub_stream);
      
      auto forcond_node = cst::new_node<cst_forcondition>();
      
      for (auto &c : converted_rpn) { 
        forcond_node->add_child(std::move(c));
      }
      
      existing_for_node->add_child(std::move(forcond_node));
      
      if (match(peek(), do_keyword_tt)) {
        consume(); // consume do
        
        auto left_curly_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt); 
        stream.insert(stream.begin() + pos + left_curly_bracket_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
        
        auto foriter_node = cst::new_node<cst_foriterexpr>();
        
        foriter_node->add_child(parse_keyword());
        
        existing_for_node->add_child(std::move(foriter_node));
      }
      else {
        throw parsing_error("do", peek(), pos, std::source_location::current().function_name());
      }
      
      auto body = parse_block();  
      existing_for_node->add_child(std::move(body));
      
      return existing_for_node;
    }
    else {
      throw parsing_error("when", peek(), pos, std::source_location::current().function_name());
    }
  }
  
  std::unique_ptr<cst_forstmt> parser::parse_for() { // for expr; in expr; { }
    consume();
    
    auto for_node = cst::new_node<cst_forstmt>();
  
    if (find_first_token(stream.begin() + pos, stream.end(), when_keyword_tt) != -1) {
      return parse_regular_for(std::move(for_node));
    }
    
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
      throw parsing_error("in", peek(), pos, std::source_location::current().function_name());
    }
  }

  std::unique_ptr<cst_array> parser::parse_array() { // array <dimensions> <datatype> <identifier> = { ... };
    consume(); // consume array

    auto node = cst::new_node<cst_array>();
    std::vector<std::size_t> dimensions; 

    while (match(peek(), left_bracket_tt)) {
      consume(); // consume [

      if (!match(peek(), number_literal_tt)) {
        throw parsing_error("number literal in array dimension", peek(), pos, std::source_location::current().function_name());
      }

      std::size_t dimension_size = from_numerical_string<std::size_t>(peek().lexeme); // get dimension and store for later
      dimensions.push_back(dimension_size); 
      consume(); // consume the number literal

      match(peek(), right_bracket_tt); // expect ]
      consume(); // consume ]
    }

    // check if the next token is valid datatype
    if (!(match(peek(), int8_keyword_tt) ||
          match(peek(), int16_keyword_tt) ||
          match(peek(), int32_keyword_tt) ||
          match(peek(), int64_keyword_tt) ||
          match(peek(), uint8_keyword_tt) ||
          match(peek(), uint16_keyword_tt) ||
          match(peek(), uint32_keyword_tt) ||
          match(peek(), uint64_keyword_tt) ||
          match(peek(), float32_keyword_tt) ||
          match(peek(), float64_keyword_tt) ||
          match(peek(), string_keyword_tt) ||
          match(peek(), boolean_keyword_tt) ||
          match(peek(), char_keyword_tt))) {
      throw parsing_error("valid <datatype>", peek(), pos, std::source_location::current().function_name());
    }

    // parse datatype and identifier
    node->add_child(parse_datatype());

    auto dimensions_count = cst::new_node<cst_dimensions_count>(std::to_string(dimensions.size()));

    for (const auto& dim : dimensions) {
      auto dimension_node = cst::new_node<cst_dimension>(std::to_string(dim));
      dimensions_count->add_child(std::move(dimension_node));
    }

    node->add_child(std::move(dimensions_count));

    if (match(peek(), assignment_tt)) {
      consume(); // consume =
    
      if (!match(peek(), left_curly_bracket_tt)) {
        throw parsing_error("'{' to start array body", peek(), pos, std::source_location::current().function_name());
      }
    
      std::function<std::unique_ptr<cst>(void)> parse_array_body;
      parse_array_body = [&]() -> std::unique_ptr<cst> {
        if (!match(peek(), left_curly_bracket_tt)) {
          throw parsing_error("'{' in array body", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume {
    
        auto body_node = cst::new_node<cst_arraybody>();
    
        while (!match(peek(), right_curly_bracket_tt)) {
          if (match(peek(), left_curly_bracket_tt)) {
            body_node->add_child(parse_array_body()); // nested array
          } 
          else {
            std::vector<token_t> element_tokens;
            int paren_depth = 0;
            while (!(match(peek(), comma_tt) && paren_depth == 0) &&
                   !(match(peek(), right_curly_bracket_tt) && paren_depth == 0)) {
              if (match(peek(), left_paren_tt) || match(peek(), left_bracket_tt) || match(peek(), left_curly_bracket_tt)) {
                paren_depth++;
              } 
              else if (match(peek(), right_paren_tt) || match(peek(), right_bracket_tt) || match(peek(), right_curly_bracket_tt)) {
                paren_depth--;
              }

              element_tokens.push_back(peek());
              consume();
            }

            if (!element_tokens.empty()) {
              auto expr_nodes = parse_expression(element_tokens);
              
              auto elem = cst::new_node<cst_arrayelement>();
              for (auto& n : expr_nodes) {
                elem->add_child(std::move(n));
              }

              body_node->add_child(std::move(elem));
            }
          }
          if (match(peek(), comma_tt)) {
            consume(); // skip comma
          }
        }
    
        consume(); // consume }
    
        return body_node;
      };
    
      node->add_child(parse_array_body());
    }

    if (!match(peek(), semicolon_tt)) {
      throw parsing_error("; at end of array decl", peek(), pos, std::source_location::current().function_name());
    }

    consume(); // consume ;

    return node;
  }

  std::unique_ptr<cst> parser::parse_keyword(bool nested_function) {
    if (nested_function) {
      if (match(peek(), function_keyword_tt)) {
        return parse_function();
      }
    }
    
    if (match(peek(), include_keyword_tt)) { // this is slower than it could be, but it works for now. will change later on
      consume();

      if (match(peek(), string_literal_tt)) {
        consume();
        auto string_token = previous();
        
        std::ifstream file(string_token.lexeme);
        if (!file.is_open()) {
          throw parsing_error("could not open file", string_token, pos, std::source_location::current().function_name());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string src = buffer.str();

        lexer l(src);
        auto included_stream = l.analyze();

        parser p(included_stream);
        auto included_cst = p.parse();

        return included_cst;
      }
      else {
        throw parsing_error("string literal", peek(), pos, std::source_location::current().function_name());
      }
    }
    else if (match(peek(), int8_keyword_tt)) {
      return parse_integer_type<cst_int8>();
    }
    else if (match(peek(), int16_keyword_tt)) {
      return parse_integer_type<cst_int16>();
    }
    else if (match(peek(), int32_keyword_tt)) {
      return parse_integer_type<cst_int32>();
    }
    else if (match(peek(), int64_keyword_tt)) {
      return parse_integer_type<cst_int64>();
    }
    else if (match(peek(), uint8_keyword_tt)) {
      return parse_integer_type<cst_uint8>();
    }
    else if (match(peek(), uint16_keyword_tt)) {
      return parse_integer_type<cst_uint16>();
    }
    else if (match(peek(), uint32_keyword_tt)) {
      return parse_integer_type<cst_uint32>();
    }
    else if (match(peek(), uint64_keyword_tt)) {
      return parse_integer_type<cst_uint64>();
    }
    else if (match(peek(), float32_keyword_tt)) {
      return parse_integer_type<cst_float32>();
    }
    else if (match(peek(), float64_keyword_tt)) {
      return parse_integer_type<cst_float64>();
    }
    else if (match(peek(), string_keyword_tt)) {
      return parse_string();
    }
    else if (match(peek(), char_keyword_tt)) {
      return parse_integer_type<cst_int8>();
    }
    else if (match(peek(), boolean_keyword_tt)) {
      return parse_integer_type<cst_int8>();
    }
    else if (match(peek(), dereference_operator_tt)) {
      auto deref_count = 0;
      while (!match(peek(), identifier_tt)) {
        deref_count++;
        consume();
      }

      if (match(peek(), identifier_tt)) {
        auto identifier = parse_identifier();

        auto deref_node = cst::new_node<cst_dereference>(std::to_string(deref_count));

        deref_node->add_child(std::move(identifier));

        if (match(peek(), assignment_tt)) {
          auto assignment = parse_assignment();
          
          if (match(peek(), semicolon_tt)) {
            throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
          }

          parse_expression_until(assignment.get(), semicolon_tt); // parse the expression until the semicolon
          
          deref_node->add_child(std::move(assignment));
        }

        if (match(peek(), semicolon_tt)) { 
          consume();
        }
        else {
          throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return deref_node;
      }
      else {
        throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
      }
    }
    else if (match(peek(), reference_operator_tt)) { 
      consume();
      
      if (match(peek(), identifier_tt)) {
        auto identifier = parse_identifier();

        auto ref_node = cst::new_node<cst_reference>();

        ref_node->add_child(std::move(identifier));

        if (match(peek(), assignment_tt)) {
          ref_node->add_child(parse_assignment());
          
          if (match(peek(), semicolon_tt)) {
            throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
          }

          parse_expression_until(ref_node.get(), semicolon_tt); // parse the expression until the semicolon
        }

        if (match(peek(), semicolon_tt)) { 
          consume();
        }
        else {
          throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return ref_node;
      }
      else {
        throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
      }
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
    else if (match(peek(), array_keyword_tt)) {
      return parse_array();
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
          throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }
        
        return std::move(converted_rpn.at(0));
      }
      else {
        throw parsing_error("<FUNC_CALL_START>", peek(), pos, std::source_location::current().function_name()); // not sure if this is verbose enough
      }
    }
    else if (match(peek(), identifier_tt) && peek(1).tt == left_bracket_tt) { // array access
      auto id = parse_identifier();
    
      std::unique_ptr<cst> array_access_node = nullptr;
      std::unique_ptr<cst_identifier> lcst_id = std::move(id);
    
      while (match(peek(), left_bracket_tt)) {
        consume(); // consume '['
    
        auto new_array_access = cst::new_node<cst_arrayaccess>();
        if (array_access_node) {
          new_array_access->add_child(std::move(array_access_node));
        } 
        else {
          new_array_access->add_child(std::move(lcst_id));
        }
    
        int bracket_depth = 1;
        std::vector<token_t> index_tokens;
    
        while (bracket_depth > 0 && !match(peek(), end_of_file_tt)) {
          if (match(peek(), left_bracket_tt)) {
            bracket_depth++;
            if (bracket_depth > 1) {
              index_tokens.push_back(peek());
            }
            consume();
          } 
          else if (match(peek(), right_bracket_tt)) {
            bracket_depth--;
            if (bracket_depth == 0) {
              if (!index_tokens.empty()) {
                auto index_nodes = parse_expression(index_tokens);
                for (auto& n : index_nodes) {
                  new_array_access->add_child(std::move(n));
                }
                index_tokens.clear();
              }
            } 
            else {
              index_tokens.push_back(peek());
            }
            consume();
          } 
          else if (match(peek(), comma_tt) && bracket_depth == 1) {
            if (!index_tokens.empty()) {
              auto index_nodes = parse_expression(index_tokens);
              for (auto& n : index_nodes) {
                new_array_access->add_child(std::move(n));
              }
              index_tokens.clear();
            }
            consume();
          } 
          else {
            index_tokens.push_back(peek());
            consume();
          }
        }
    
        array_access_node = std::move(new_array_access);
      }
    
      if (match(peek(), assignment_tt)) {
        auto assignment = parse_assignment();
    
        if (match(peek(), semicolon_tt)) {
          throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
        }
    
        parse_expression_until(assignment.get(), semicolon_tt);
    
        array_access_node->add_child(std::move(assignment));
      }
    
      if (match(peek(), semicolon_tt)) {
        consume();
      } 
      else {
        throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
      }
    
      return array_access_node;
    }
    else if (match(peek(), identifier_tt)) {
      auto id = parse_identifier();
      
      if (match(peek(), assignment_tt)) {
        id->add_child(parse_assignment());
        
        if (match(peek(), semicolon_tt)) {
          throw parsing_error("<expr>", peek(), pos, std::source_location::current().function_name());
        }

        parse_expression_until(id.get(), semicolon_tt); // parse the expression until the semicolon
        
        if (match(peek(), semicolon_tt)) { 
          consume();
        }
        else {
          throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }
        
        return id;
      }
    }

    throw parsing_error("<UKN_KEYWORD>", peek(), pos, std::source_location::current().function_name());
  }

  void parser::synchronize(std::string what) {
    std::cout << RED << what << RESET << std::endl;

    std::uintptr_t lcst_pos = pos; 
    while (!match(peek(), end_of_file_tt)) {
      if (match(peek(), semicolon_tt) || match(peek(), left_curly_bracket_tt) || match(peek(), right_curly_bracket_tt)) {
        consume();
        
        return;
      }
      
      if (pos == lcst_pos) {
        consume();
      }
      
      lcst_pos = pos;
    }
  }
  
  std::unique_ptr<cst_root> parser::parse() {
    while (!match(peek(), end_of_file_tt)) {
      try {
        auto node = parse_keyword(true);

        if (node->get_type() == cst_type::root) {
          for (auto& child : node->get_children()) {
            root->add_child(std::move(child)); // nested
          }
        }
        else {
          root->add_child(std::move(node)); // not nested
        }

        if (match(peek(), end_of_file_tt) && parser_state == state::neutral) {
          parser_state = state::success;
          break;
        }
      }
      catch (const parsing_error& e) {
        parser_state = state::failed;
        synchronize(e.what());
      }
    }

    return std::move(root);
  }
} // namespace occult
