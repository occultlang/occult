#include "parser.hpp"

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

  std::unordered_map<token_type, int> precedence_map = {
      {unary_plus_operator_tt, 2},
      {unary_minus_operator_tt, 2},
      {unary_bitwise_not_tt, 2},
      {unary_not_operator_tt, 2}, // logical

      {multiply_operator_tt, 3},
      {division_operator_tt, 3},
      {modulo_operator_tt, 3},

      {add_operator_tt, 4},
      {subtract_operator_tt, 4},

      {bitwise_rshift_tt, 5},
      {bitwise_lshift_tt, 5},

      {greater_than_operator_tt, 6},
      {less_than_operator_tt, 6},
      {less_than_or_equal_operator_tt, 6},
      {greater_than_or_equal_operator_tt, 6},

      {equals_operator_tt, 7},
      {not_equals_operator_tt, 7},

      {bitwise_and_tt, 8},

      {xor_operator_tt, 9}, // bitwise

      {bitwise_or_tt, 10}, // bitwise

      {and_operator_tt, 11}, // logical

      {or_operator_tt, 12}, // logical

      {assignment_tt, 14},

      {comma_tt, 15}};

  // true is right associative, false is left
  std::unordered_map<token_type, bool> associativity_map = {
      {unary_plus_operator_tt, true},
      {unary_minus_operator_tt, true},
      {unary_bitwise_not_tt, true},
      {unary_not_operator_tt, true}, // logical

      {multiply_operator_tt, false},
      {division_operator_tt, false},
      {modulo_operator_tt, false},

      {add_operator_tt, false},
      {subtract_operator_tt, false},

      {bitwise_rshift_tt, false},
      {bitwise_lshift_tt, false},

      {greater_than_operator_tt, false},
      {less_than_operator_tt, false},
      {less_than_or_equal_operator_tt, false},
      {greater_than_or_equal_operator_tt, false},

      {equals_operator_tt, false},
      {not_equals_operator_tt, false},

      {bitwise_and_tt, false},

      {xor_operator_tt, false}, // bitwise

      {bitwise_or_tt, false}, // bitwise

      {and_operator_tt, false}, // logical

      {or_operator_tt, false}, // logical

      {assignment_tt, true},

      {comma_tt, false}};

  bool is_unary(token_type tt) {
    return tt == unary_plus_operator_tt || tt == unary_minus_operator_tt ||
           tt == unary_bitwise_not_tt || tt == unary_not_operator_tt;
  }

  bool is_binary(token_type tt) {
    return tt == add_operator_tt ||
           tt == subtract_operator_tt ||
           tt == multiply_operator_tt ||
           tt == division_operator_tt ||
           tt == modulo_operator_tt ||
           tt == bitwise_and_tt ||
           tt == bitwise_or_tt ||
           tt == xor_operator_tt ||
           tt == bitwise_lshift_tt ||
           tt == bitwise_rshift_tt ||
           tt == greater_than_operator_tt ||
           tt == less_than_operator_tt ||
           tt == equals_operator_tt ||
           tt == not_equals_operator_tt ||
           tt == less_than_or_equal_operator_tt ||
           tt == greater_than_or_equal_operator_tt ||
           tt == and_operator_tt ||
           tt == or_operator_tt;
  }

  std::vector<token_t> parser::to_rpn(std::vector<token_t> expr) {
    std::stack<token_t> operator_stack;
    std::vector<token_t> rpn_output;

    for (std::size_t i = 0; i < expr.size(); i++) {
      auto t = expr.at(i);

      if (t.tt == semicolon_tt) { // stop if semicolon
        break;
      }
      
      if (t.tt == identifier_tt && expr.at(i + 1).tt == left_paren_tt) { // function call
        token_t temp_tk1(t.line, t.column, "start_call", function_call_parser_tt);
        rpn_output.push_back(temp_tk1);
        
        rpn_output.push_back(t);
        i++;
        
        operator_stack.push(expr.at(i)); // (
        
        while (!operator_stack.empty()) {
          i++;
          t = expr.at(i);
          
          if (t.tt == right_paren_tt) {
            while (!operator_stack.empty() && operator_stack.top().tt != left_paren_tt) {
              rpn_output.push_back(operator_stack.top());
              operator_stack.pop();
            }
            
            operator_stack.pop();
            
            break;
          }
          else if (t.tt == comma_tt) {
            while (!operator_stack.empty() && operator_stack.top().tt != left_paren_tt) {
              rpn_output.push_back(operator_stack.top());
              operator_stack.pop();
            }
          }
          else {
            rpn_output.push_back(t);
          }
        }
        
        token_t temp_tk2(t.line, t.column, "end_call", function_call_parser_tt);
        
        rpn_output.push_back(temp_tk2);
      }
      else if (t.tt == number_literal_tt || t.tt == float_literal_tt || t.tt == identifier_tt) {
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

      consume();
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
    
    for (auto t : expr_rpn) {
      std::println("{}: {}", t.get_typename(t.tt), t.lexeme);
    }
    
    std::vector<std::unique_ptr<ast>> expr_ast;
    
    for (auto t : expr_rpn) {
      switch (t.tt) {
        case function_call_parser_tt: { // this way we can actually use function calls properly
          auto node = ast::new_node<ast_functioncall>();
          
          break;
        }
        case number_literal_tt: {
          auto node = ast::new_node<ast_numberliteral>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case identifier_tt: {
          auto node = ast::new_node<ast_identifier>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case float_literal_tt: {
          auto node = ast::new_node<ast_floatliteral>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case add_operator_tt: {
          auto node = ast::new_node<ast_add>();
          node->content = t.lexeme; 
          expr_ast.push_back(std::move(node));
          break;
        }
        case subtract_operator_tt: {
          auto node = ast::new_node<ast_subtract>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case unary_plus_operator_tt: {
          auto node = ast::new_node<ast_unary_plus>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case unary_minus_operator_tt: {
          auto node = ast::new_node<ast_unary_minus>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case multiply_operator_tt: {
          auto node = ast::new_node<ast_multiply>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case division_operator_tt: {
          auto node = ast::new_node<ast_divide>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case modulo_operator_tt: {
          auto node = ast::new_node<ast_modulo>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case bitwise_and_tt: {
          auto node = ast::new_node<ast_bitwise_and>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case unary_bitwise_not_tt: {
          auto node = ast::new_node<ast_unary_bitwise_not>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case bitwise_or_tt: {
          auto node = ast::new_node<ast_bitwise_or>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case xor_operator_tt: {
          auto node = ast::new_node<ast_xor>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case bitwise_lshift_tt: {
          auto node = ast::new_node<ast_bitwise_lshift>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case bitwise_rshift_tt: {
          auto node = ast::new_node<ast_bitwise_rshift>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case and_operator_tt: {
          auto node = ast::new_node<ast_and>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case or_operator_tt: {
          auto node = ast::new_node<ast_or>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case unary_not_operator_tt: {
          auto node = ast::new_node<ast_unary_not>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case equals_operator_tt: {
          auto node = ast::new_node<ast_equals>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case not_equals_operator_tt: {
          auto node = ast::new_node<ast_not_equals>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case greater_than_operator_tt: {
          auto node = ast::new_node<ast_greater_than>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case less_than_operator_tt: {
          auto node = ast::new_node<ast_less_than>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case greater_than_or_equal_operator_tt: {
          auto node = ast::new_node<ast_greater_than_or_equal>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        case less_than_or_equal_operator_tt: {
          auto node = ast::new_node<ast_less_than_or_equal>();
          node->content = t.lexeme;
          expr_ast.push_back(std::move(node));
          break;
        }
        default:
          break;
      }
    }

    return expr_ast;
  }
  
  std::unique_ptr<ast> parser::parse_datatype() { // add more
    if (match(peek(), int8_keyword_tt)) {
      consume();
      
      auto node = ast::new_node<ast_int8>();
      
      if (peek().tt == identifier_tt) {
        node->add_child(parse_identifier());
      }
      
      return node;
    }
    else {
      throw runtime_error("invalid datatype", peek());
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
        throw runtime_error("expected right parenthesis", peek());
      }
      else {
        consume();
      }
      
      func_node->add_child(std::move(func_args_node));
    }
    else {
      throw runtime_error("expected left parenthesis", peek());
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
        throw runtime_error("expected right curly brace", peek());
      }
      else {
        consume();
      }
      
      return block_node;
    }
    else {
      throw runtime_error("expected left curly brace", peek());
    }
  }

  std::unique_ptr<ast_assignment> parser::parse_assignment() {
    consume();

    auto node = ast::new_node<ast_assignment>();

    return node;
  }

  std::unique_ptr<ast_int8> parser::parse_int8() {
    consume(); // consume int8 keyword

    auto i8_node = ast::new_node<ast_int8>();

    if (match(peek(), identifier_tt)) {
      i8_node->add_child(parse_identifier()); // add identifier as a child node
    } else {
      throw runtime_error("expected identifier", peek());
    }

    if (match(peek(), assignment_tt)) {
      i8_node->add_child(parse_assignment());
      
      if (match(peek(), semicolon_tt)) {
        throw runtime_error("expected expression", peek());
      }
      
      std::vector<token_t> sub_stream = {stream.begin() + pos, stream.end()}; 
      auto converted_rpn = parse_expression(sub_stream);
      
      for (auto &c : converted_rpn) { // adding all the children of the converted expression into the i8_node
        i8_node->get_children().at(1)->add_child(std::move(c));
      }
    }

    if (match(peek(), semicolon_tt)) { // end of declaration
      consume();
    } else {
      throw runtime_error("expected semicolon", peek());
    }

    return i8_node;
  }
  
  std::unique_ptr<ast_returnstmt> parser::parse_return() {
    consume();
    
    auto return_node = ast::new_node<ast_returnstmt>();
    
    std::vector<token_t> sub_stream = {stream.begin() + pos, stream.end()}; 
    auto converted_rpn = parse_expression(sub_stream);
    
    for (auto &c : converted_rpn) { 
      return_node->add_child(std::move(c));
    }
    
    if (match(peek(), semicolon_tt)) { 
      consume();
    } else {
      throw runtime_error("expected semicolon", peek());
    }
    
    return return_node;
  }

  std::unique_ptr<ast> parser::parse_keyword(bool nested_function) {
    if (nested_function) {
      if (match(peek(), function_keyword_tt)) {
        return parse_function();
      }
    }

    if (match(peek(), int8_keyword_tt)) {
      return parse_int8();
    }
    else if (match(peek(), return_keyword_tt)) {
      return parse_return();
    }
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
