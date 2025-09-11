#include "ir_gen.hpp"
#include "../../lexer/number_parser.hpp"
#include <algorithm>

/*
 * This IR generation is really crappy and disorganized, I will make it organized later on, and I'll probably move to SSA or TAC
 * later on as well, but this is what I can do for now, as I want to make actual progress on the language, but we can still do some stack optimisations
 * we can use this paper (http://www.rigwit.co.uk/thesis/chap-5.pdf) as an example, and other theories too.
*/

// use string(s) to determine the current exit, entry, etc labels

// add other cases to array declaration for types

namespace occult {
  ir_function ir_gen::generate_function(cst_function* func_node) {
    ir_function function;
    
    for (const auto& c : func_node->get_children()) {
      switch(c->get_type()) {
        case cst_type::int8_datatype:
        case cst_type::int16_datatype:
        case cst_type::int32_datatype:
        case cst_type::int64_datatype:
        case cst_type::uint8_datatype:
        case cst_type::uint16_datatype:
        case cst_type::uint32_datatype:
        case cst_type::uint64_datatype:
        case cst_type::string_datatype: {
            function.type = c->to_string().substr(4, c->to_string().size());
            break;
        }
        case cst_type::functionarguments: {
            generate_function_args(function, cst::cast_raw<cst_functionargs>(c.get()));
            break;
        }
        case cst_type::identifier: {
            function.name = c->content;
            break;
        }
        case cst_type::block: {
            generate_block(function, cst::cast_raw<cst_block>(c.get()));
            break;
        }
        default: {
            break;
        }
      }
    }
    
    return function;
  }
  
  void ir_gen::generate_function_args(ir_function& function, cst_functionargs* func_args_node) {
    for (const auto& arg : func_args_node->get_children()) {
      for (const auto& c : arg->get_children()) {
        function.args.emplace_back(c->content, arg->to_string().substr(4, arg->to_string().size()));
      }
    }
  }
  
  void ir_gen::generate_arith_and_bitwise_operators(ir_function& function, cst* c) {
    switch (c->get_type()) {
      case cst_type::add_operator: {
        function.code.emplace_back(op_add);
        
        break;
      }
      case cst_type::subtract_operator: {
        function.code.emplace_back(op_sub);
        
        break;
      }
      case cst_type::multiply_operator: {
        function.code.emplace_back(op_mul);
        
        break;
      }
      case cst_type::division_operator: {
        function.code.emplace_back(op_div);
        
        break;
      }
      case cst_type::modulo_operator: {
        function.code.emplace_back(op_mod);
        
        break;
      }
      case cst_type::bitwise_and: {
        function.code.emplace_back(op_bitwise_and);

        break;
      }
      case cst_type::bitwise_or: {
        function.code.emplace_back(op_bitwise_or);

        break;
      }
      case cst_type::xor_operator: {
        function.code.emplace_back(op_bitwise_xor);

        break;
      }
      case cst_type::bitwise_lshift: {
        function.code.emplace_back(op_bitwise_lshift);

        break;
      }
      case cst_type::bitwise_rshift: {
        function.code.emplace_back(op_bitwise_rshift);

        break;
      }
      case cst_type::unary_plus_operator: { // just purely syntax
        break;
      }
      case cst_type::unary_minus_operator: {
        function.code.emplace_back(op_negate);

        break;
      }
      case cst_type::unary_bitwise_not: {
        function.code.emplace_back(op_bitwise_not);

        break;
      }
      default: {
        break;
      }
    }
  }
  
  template<typename IntType>
  void ir_gen::generate_common(ir_function& function, cst* node) {
    for (const auto& c : node->get_children()) {      
      switch(c->get_type()) {
        case cst_type::number_literal: {
          function.code.emplace_back(op_push, from_numerical_string<IntType>(c->content));
          
          break;
        }
        case cst_type::float_literal: {
          function.code.emplace_back(op_pushf, from_numerical_string<IntType>(c->content));
          
          break;
        }
        case cst_type::identifier: {
          function.code.emplace_back(op_load, c->content);
          
          break;
        }
        case cst_type::stringliteral: {
          function.code.emplace_back(op_push, c->content);
          
          break;
        }
        case cst_type::functioncall: {
          generate_function_call(function, c.get());
          
          break;
        }
        case cst_type::equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setz);
          
          break;
        }
        case cst_type::not_equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setnz);
         
          break;
        }
        case cst_type::greater_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setg);
          
          break;
        }
        case cst_type::less_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setl);
          
          break;
        }
        case cst_type::greater_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setge);
          
          break;
        }
        case cst_type::less_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setle);
          
          break;
        }
        case cst_type::or_operator: {
          function.code.emplace_back(op_logical_or);

          break;
        }
        case cst_type::and_operator: {
          function.code.emplace_back(op_logical_and);

          break;
        }
        case cst_type::reference: {
          function.code.emplace_back(op_reference);

          break;
        }
        case cst_type::dereference: {
          auto deref_count = from_numerical_string<std::int64_t>(c->content);
          function.code.emplace_back(op_dereference, deref_count);

          break;
        }
        default: {
          generate_arith_and_bitwise_operators(function, c.get());

          break;
        }
      }
    }
  }
  
  void ir_gen::handle_push_types(ir_function& function, cst* c) {    
    auto it = ir_typemap.find(function.type);
    
    if (it != ir_typemap.end()) {
      switch (it->second) {
        case signed_int:{
          function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(c->content));
          
          break;
        }
        case unsigned_int: {
          function.code.emplace_back(op_push, from_numerical_string<std::uint64_t>(c->content));
          
          break;
        }
        case floating_point64: {
          function.code.emplace_back(op_pushf, from_numerical_string<double>(c->content));
          
          break;
        }
        case floating_point32: {
          function.code.emplace_back(op_pushf, from_numerical_string<float>(c->content));
          
          break;
        }
        case string: {
          function.code.emplace_back(op_push, from_numerical_string<std::string>(c->content));
          
          break;
        }
      }
    }
  }
  
  void ir_gen::handle_push_types_common(ir_function& function, cst* c) {    
    auto it = ir_typemap.find(function.type);
    
    if (it != ir_typemap.end()) {
      switch (it->second) {
        case signed_int: {
          generate_common<std::int64_t>(function, c);
          
          break;
        }
        case unsigned_int: {
          generate_common<std::uint64_t>(function, c);
          
          break;
        }
        case floating_point64: {
          generate_common<double>(function, c);
          
          break;
        }
        
        case floating_point32: {
          generate_common<float>(function, c);
          
          break;
        }
        case string: {
          generate_common<std::string>(function, c);
          
          break;
        }
      }
    }
  }
  
  void ir_gen::generate_function_call(ir_function& function, cst* c) {
    auto node = cst::cast_raw<cst_functioncall>(c);
    
    auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get()); // name of call
    
    auto arg_location = 1;
    while (node->get_children().at(arg_location).get()->content != "end_call") {
      auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(arg_location).get());
      
      handle_push_types_common(function, arg_node);
      
      arg_location++;
    }
    
    function.code.emplace_back(op_call, identifier->content);
  }
  
  void ir_gen::generate_return(ir_function& function, cst_returnstmt* return_node) {
    for (const auto& c : return_node->get_children()) {
      generate_arith_and_bitwise_operators(function, c.get());
      
      switch(c->get_type()) {
        case cst_type::number_literal: {
          handle_push_types(function, c.get());
          
          break;
        }
        case cst_type::identifier: {
          function.code.emplace_back(op_load, c->content);
          
          break;
        }
        case cst_type::functioncall: {
          generate_function_call(function, c.get());
          
          break;
        }
        case cst_type::or_operator: {
          function.code.emplace_back(op_logical_or);

          break;
        }
        case cst_type::and_operator: {
          function.code.emplace_back(op_logical_and);

          break;
        }
        case cst_type::equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setz);
          
          break;
        }
        case cst_type::not_equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setnz);
         
          break;
        }
        case cst_type::greater_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setg);
          
          break;
        }
        case cst_type::less_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setl);
          
          break;
        }
        case cst_type::greater_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setge);
          
          break;
        }
        case cst_type::less_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_setle);
          
          break;
        }
        case cst_type::arrayaccess: {
          generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));

          break;
        }
        default: {
          break;
        }
      }
    }
  }

  bool is_logical(cst* node) { 
    switch (node->get_type()) { 
      case cst_type::and_operator: 
      case cst_type::or_operator: 
      case cst_type::equals_operator: 
      case cst_type::not_equals_operator: 
      case cst_type::greater_than_operator: 
      case cst_type::less_than_operator: 
      case cst_type::greater_than_or_equal_operator: 
      case cst_type::less_than_or_equal_operator: 
        return true; 
      default: 
        return false; 
    } 
  } 

  void ir_gen::generate_condition(ir_function& function, cst* node, std::string false_label, std::string true_label) {
    auto handle_internal_push = [&, this](cst* expr) -> void {
      switch(expr->get_type()) {
        case cst_type::number_literal: {
          handle_push_types(function, expr);
          
          break;
        }
        case cst_type::identifier: {
          function.code.emplace_back(op_load, expr->content);
          
          break;
        }
        case cst_type::functioncall: {
          generate_function_call(function, expr);
          
          break;
        }
        case cst_type::arrayaccess: {
          generate_array_access(function, cst::cast_raw<cst_arrayaccess>(expr));

          break;
        }
        default: {
          generate_arith_and_bitwise_operators(function, expr);
          
          break;
        }
      }
    };

    std::vector<cst*> logic_stack; 
    std::vector<cst*> eval_stack;

    for (std::size_t i = 0; i < node->get_children().size(); i++) {
      const auto& c = node->get_children().at(i);
      
      switch(c->get_type()) {
        case cst_type::expr_end:
        case cst_type::unary_not_operator:
        case cst_type::elseifstmt:
        case cst_type::elsestmt: 
        case cst_type::block: {
          break; 
        } 
        case cst_type::and_operator:
        case cst_type::or_operator: {
          logic_stack.emplace_back(c.get()); 

          break;
        }
        case cst_type::expr_start: { 
          std::size_t curr_pos = i;

          if (i > 0 && node->get_children().at(i - 1)->get_type() == cst_type::unary_not_operator) { // pre-process not's (ONLY BINARY COMPARISONS FOR NOW)
            std::size_t not_pos = i;
            std::size_t not_count = 0;

            while (not_pos > 0 && node->get_children().at(not_pos - 1)->get_type() == cst_type::unary_not_operator) {
              not_pos--;
              not_count++;
            }

            bool apply_not = not_count % 2 != 0;

            for (int j = curr_pos; j >= 0; j--) {
              const auto& curr_node = node->get_children().at(j);
              
              if (curr_node->get_type() == cst_type::expr_end) {
                break;
              }

              if (is_logical(curr_node.get())) {
                curr_node->do_not = apply_not;
              }
            }
          }

          break;
        }
        default: {
          eval_stack.emplace_back(c.get());

          break;
        }
      }
    }

    if (debug && !logic_stack.empty()) {
      std::cout << BLUE << "[IR GEN] Filled logic stack:" << RESET << std::endl;
      for (auto& n : logic_stack) {
        n->visualize(2);
      }
      std::cout << BLUE << "[IR GEN] Logic stack size: " << RESET << logic_stack.size() << std::endl;
    }
    
    if (debug && !eval_stack.empty()) {
      std::cout << BLUE << "[IR GEN] Filled eval stack:" << RESET << std::endl;
      for (auto& n : eval_stack) {
        n->visualize(2);
      }
      std::cout << BLUE << "[IR GEN] Eval stack size: " << RESET << eval_stack.size() << std::endl;
    }

    std::vector<std::vector<cst*>> eval_stack_m; // to easier parse expressions (multidimensional)
    
    std::vector<cst*> current_expr;
    for (size_t i = 0; i < eval_stack.size(); ++i) {
      current_expr.push_back(eval_stack[i]);
        
      if (is_logical(eval_stack[i])) {
        eval_stack_m.push_back(current_expr);
        current_expr.clear();
      }
    }

    if (debug && !eval_stack_m.empty()) {
      std::cout << BLUE << "[IR GEN] Filled eval stack (m):" << RESET << std::endl;
      for (auto& arr : eval_stack_m) {
        std::cout << CYAN << "{" << RESET << std::endl;
        for (auto& n : arr) {
          n->visualize(2);
        }
        std::cout << CYAN << "}" << RESET << std::endl;
      }
      std::cout << BLUE << "[IR GEN] Eval stack (m) size: " << RESET << eval_stack.size() << std::endl;
    }

    if (!logic_stack.empty()) { // if condition with logical operations (2+)
      std::size_t eval_index = 0;
      
      for (std::size_t i = 0; i < logic_stack.size(); i++) {
        auto c = logic_stack.at(i);
        
        switch(c->get_type()) {
          case cst_type::or_operator: { 
            if (eval_index < eval_stack_m.size()) {
              auto expr = eval_stack_m[eval_index];
              
              for (size_t j = 0; j < expr.size() - 1; j++) {
                handle_internal_push(expr[j]);
              }

              function.code.emplace_back(op_cmp);
              
              if (expr.back()->do_not) {
                generate_and_jump(function, expr.back(), true_label);
              }
              else {
                generate_or_jump(function, expr.back(), true_label);
              }
              
              eval_index++;
            }
            break;
          }
          case cst_type::and_operator: { 
            if (eval_index < eval_stack_m.size()) {
              auto expr = eval_stack_m[eval_index];
              
              for (size_t j = 0; j < expr.size() - 1; j++) {
                handle_internal_push(expr[j]);
              }

              function.code.emplace_back(op_cmp);

              if (expr.back()->do_not) {
                generate_or_jump(function, expr.back(), false_label);
              }
              else {
                generate_and_jump(function, expr.back(), false_label);
              }
              
              eval_index++;
            }
            break;
          }
          default: {
            break;
          }
        }
      }
      
      // handle remaining expressions
      while (eval_index < eval_stack_m.size()) {
        auto expr = eval_stack_m[eval_index];
        
        for (size_t j = 0; j < expr.size() - 1; j++) {
          handle_internal_push(expr[j]);
        }

        function.code.emplace_back(op_cmp);

        if (expr.back()->do_not) {
          generate_inverted_jump(function, expr.back(), false_label);
        }
        else {
          generate_normal_jump(function, expr.back(), false_label);
        }
        
        eval_index++;
      }
    }
    else { // if condition with normal comparisons (1)
      if (!eval_stack_m.empty()) {
        auto expr = eval_stack_m[0];
        
        for (size_t j = 0; j < expr.size() - 1; j++) {
          handle_internal_push(expr[j]);
        }

        function.code.emplace_back(op_cmp);

        if (expr.back()->do_not) {
          generate_inverted_jump(function, expr.back(), false_label);
        }
        else {
          generate_normal_jump(function, expr.back(), false_label);
        }
      }
    }
  }

  void ir_gen::generate_if(ir_function& function, cst_ifstmt* if_node, std::string current_break_label, std::string current_loop_start) {
    std::string false_label = create_label(); // exit / else or whatever
    std::string true_label = create_label(); // block
    
    generate_condition(function, if_node, false_label, true_label);

    place_label(function, true_label);
    
    for (const auto& c : if_node->get_children()) {
      if (c->get_type() == cst_type::block) {
        generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);

        break;
      }
    }
    
    place_label(function, false_label);

    for (const auto& elseif_node : if_node->get_children()) {
      if (elseif_node->get_type() == cst_type::elseifstmt) {
        std::string elseif_true_label = create_label();
        std::string elseif_false_label = create_label();

        generate_condition(function, elseif_node.get(), elseif_false_label, elseif_true_label);

        place_label(function, elseif_true_label);
        
        for (const auto& c : elseif_node->get_children()) {
          if (c->get_type() == cst_type::block) {
            generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);

            break;
          }
        }
        
        place_label(function, elseif_false_label);
      }
      else if (elseif_node->get_type() == cst_type::elsestmt) {
        for (const auto& b : elseif_node->get_children()) {
          if (b->get_type() == cst_type::block) {
            generate_block(function, cst::cast_raw<cst_block>(b.get()), current_break_label, current_loop_start);

            break;
          }
        }
      }
    }
  }

  void ir_gen::generate_or_jump(ir_function& function, cst* comparison, const std::string& true_label) {
    switch (comparison->get_type()) {
      case cst_type::equals_operator:
          function.code.emplace_back(op_jz, true_label);

          break;
      case cst_type::not_equals_operator:
          function.code.emplace_back(op_jnz, true_label);

          break;
      case cst_type::greater_than_operator:
          function.code.emplace_back(op_jg, true_label);

          break;
      case cst_type::less_than_operator:
          function.code.emplace_back(op_jl, true_label);

          break;
      case cst_type::greater_than_or_equal_operator:
        function.code.emplace_back(op_jge, true_label);

          break;
      case cst_type::less_than_or_equal_operator:
        function.code.emplace_back(op_jle, true_label);

        break;
      default:
          break;
    }
  }

  void ir_gen::generate_and_jump(ir_function& function, cst* comparison, const std::string& false_label) {
    switch (comparison->get_type()) {
      case cst_type::equals_operator:
          function.code.emplace_back(op_jnz, false_label);
          break;
      case cst_type::not_equals_operator:
          function.code.emplace_back(op_jz, false_label);
          break;
      case cst_type::greater_than_operator:
          function.code.emplace_back(op_jle, false_label);
          break;
      case cst_type::less_than_operator:
          function.code.emplace_back(op_jge, false_label);
          break;
      case cst_type::greater_than_or_equal_operator:
          function.code.emplace_back(op_jl, false_label);
          break;
      case cst_type::less_than_or_equal_operator:
          function.code.emplace_back(op_jg, false_label);
          break;
      default:
          break;
    }
  }

  void ir_gen::generate_normal_jump(ir_function& function, cst* comparison, const std::string& false_label) {
    switch (comparison->get_type()) {
      case cst_type::equals_operator:
          function.code.emplace_back(op_jnz, false_label);
          break;
      case cst_type::not_equals_operator:
          function.code.emplace_back(op_jz, false_label);
          break;
      case cst_type::greater_than_operator:
          function.code.emplace_back(op_jle, false_label);
          break;
      case cst_type::less_than_operator:
          function.code.emplace_back(op_jge, false_label);
          break;
      case cst_type::greater_than_or_equal_operator:
          function.code.emplace_back(op_jl, false_label);
          break;
      case cst_type::less_than_or_equal_operator:
          function.code.emplace_back(op_jg, false_label);
          break;
      default:
          break;
    } 
  }

  void ir_gen::generate_inverted_jump(ir_function& function, cst* comparison, const std::string& false_label) {
    switch (comparison->get_type()) {
      case cst_type::equals_operator:
          function.code.emplace_back(op_jz, false_label);
          break;
      case cst_type::not_equals_operator:
          function.code.emplace_back(op_jnz, false_label);
          break;
      case cst_type::greater_than_operator:
          function.code.emplace_back(op_jl, false_label);
          break;
      case cst_type::less_than_operator:
          function.code.emplace_back(op_jg, false_label);
          break;
      case cst_type::greater_than_or_equal_operator:
          function.code.emplace_back(op_jle, false_label);
          break;
      case cst_type::less_than_or_equal_operator:
          function.code.emplace_back(op_jge, false_label);
          break;
      default:
          break;
    } 
  }

  // b1 is break, l1 is loop start (disorganized names cuz function rewrite)
  
  void ir_gen::generate_loop(ir_function& function, cst_loopstmt* loop_node) { 
    auto L1 = create_label();
    auto B1 = create_label();
    
    place_label(function, L1);
    
    for (const auto& c : loop_node->get_children()) {
      if (c->get_type() == cst_type::block) {
        generate_block(function, cst::cast_raw<cst_block>(c.get()), B1, L1);
      }
    }
    
    function.code.emplace_back(op_jmp, L1);
    
    place_label(function, B1);
  }

  void ir_gen::generate_while(ir_function& function, cst_whilestmt* while_node) {
    std::string loop_label = create_label();
    std::string break_label = create_label();

    place_label(function, loop_label);

    generate_condition(function, while_node, break_label, loop_label);

    for (const auto& c : while_node->get_children()) {
      if (c->get_type() == cst_type::block) {
        generate_block(function, cst::cast_raw<cst_block>(c.get()), break_label, loop_label);
      }
    }
    
    function.code.emplace_back(op_jmp, loop_label);
    
    place_label(function, break_label);
  }
  
  void ir_gen::generate_for(ir_function& function, cst_forstmt* for_node) {
    auto first_node = for_node->get_children().front().get(); 
    auto condition = for_node->get_children().at(1).get();
    auto body = for_node->get_children().back().get();
    
    switch (for_node->get_children().front()->get_type()) {
      case cst_type::int8_datatype:
      case cst_type::int16_datatype:
      case cst_type::int32_datatype:
      case cst_type::int64_datatype: {
        auto identifier = cst::cast_raw<cst_identifier>(first_node->get_children().front().get()); // name
        auto assignment = cst::cast_raw<cst_assignment>(first_node->get_children().back().get()); // expression stuff
        
        generate_common<std::int64_t>(function, assignment);
        
        function.code.emplace_back(op_store, identifier->content, first_node->to_string().substr(4, first_node->to_string().size()));
        
        break;
      }
      case cst_type::uint8_datatype:
      case cst_type::uint16_datatype:
      case cst_type::uint32_datatype:
      case cst_type::uint64_datatype: {
        auto identifier = cst::cast_raw<cst_identifier>(first_node->get_children().front().get());
        auto assignment = cst::cast_raw<cst_assignment>(first_node->get_children().back().get());
        
        generate_common<std::uint64_t>(function, assignment);
        
        function.code.emplace_back(op_store, identifier->content, first_node->to_string().substr(4, first_node->to_string().size()));
        
        break;
      }
      default: {
        break;
      }
    }
    
    auto L1 = create_label();
    auto B1 = create_label();
    
    place_label(function, L1); // L1 START
    
    generate_condition(function, condition, B1, L1);
   
    generate_block(function, cst::cast_raw<cst_block>(body), B1, L1);
    
    auto new_block = cst::new_node<cst_block>(); // swapping parent
    for (auto& c : for_node->get_children().at(2)->get_children()) {
      new_block->add_child(std::move(c));
    }
    
    generate_block(function, new_block.get()); // iterator
    
    function.code.emplace_back(op_jmp, L1);
    
    place_label(function, B1);
  }

  void ir_gen::generate_array_decl(ir_function& function, cst_array* array_node) { /* maybe we need some metadata for the actual location in the CST to make it easier to map it to proper codegen...? */
    auto type = array_node->get_children().front().get();
    auto identifier = cst::cast_raw<cst_identifier>(type->get_children().front().get()); // name

    function.code.emplace_back(op_array_decl, identifier->content);
    function.code.emplace_back(op_decl_array_type,  type->to_string().substr(4, type->to_string().size()));

    auto dimensions_count_node = cst::cast_raw<cst_dimensions_count>(array_node->get_children().at(1).get());
    
    std::vector<std::size_t> dimensions;
    for (const auto& c : dimensions_count_node->get_children()) {
      if (c->get_type() == cst_type::dimension) {
        dimensions.push_back(from_numerical_string<std::size_t>(c->content));
      }
    }

    function.code.emplace_back(op_array_dimensions, std::to_string(dimensions.size()));

    for (const auto& dim : dimensions) {
      function.code.emplace_back(op_array_size, std::to_string(dim));
    }

    if (dimensions.size() > 1) {
      auto first_array_node = cst::cast_raw<cst_arraybody>(array_node->get_children().back().get()); 

      if (first_array_node == nullptr) {
        return;
      }
      
      for (std::size_t i = 0; i < dimensions.size(); ++i) { 
        for (std::size_t j = 0; j < dimensions[i]; ++j) {
          auto array_node = first_array_node->get_children().at(j).get();
          
          if (array_node->get_type() == cst_type::arraybody) {
            auto element = cst::cast_raw<cst_arrayelement>(array_node->get_children().at(i).get());
            
            for (const auto& c : element->get_children()) {
              switch(c->get_type()) { /* ADD OTHER TYPES AND CASES TO THIS */
                case cst_type::number_literal: {
                  handle_push_types(function, c.get());

                  break;
                }
                default: {
                  break;
                }
              }
            }

            function.code.emplace_back(op_array_store_element, identifier->content);
          }
        }
      }
    }    
    else if (dimensions.size() == 1) {
      auto first_array_node = cst::cast_raw<cst_arraybody>(array_node->get_children().back().get());

      if (first_array_node == nullptr) {
        return;
      }

      for (std::size_t i = 0; i < dimensions.front(); ++i) {
      
        auto element = cst::cast_raw<cst_arrayelement>(first_array_node->get_children().at(i).get());

        for (const auto& c : element->get_children()) {
          switch(c->get_type()) { /* ADD OTHER TYPES AND CASES TO THIS */
            case cst_type::number_literal: {
              handle_push_types(function, c.get());

              break;
            }
            default: {
              break;
            }
          }
        }

        function.code.emplace_back(op_array_store_element, identifier->content);
      }
    }
  }

  void ir_gen::generate_array_access(ir_function& function, cst_arrayaccess* array_access_node) {
    if (array_access_node->get_children().front()->get_type() == cst_type::identifier) { // 1-dimensional array access
      auto identifier = cst::cast_raw<cst_identifier>(array_access_node->get_children().front().get());
      auto has_assigment = array_access_node->get_children().back()->get_type() == cst_type::assignment;

      for (std::size_t i = 1; i < array_access_node->get_children().size() - (has_assigment) ? 1 : 0; i++) { 
        auto c = array_access_node->get_children().at(i).get();

        generate_arith_and_bitwise_operators(function, c);

        switch(c->get_type()) {
          case cst_type::number_literal: {
            handle_push_types(function, c);
            
            break;
          }
          case cst_type::identifier: {
            function.code.emplace_back(op_load, c->content);
            
            break;
          }
          case cst_type::functioncall: {
            generate_function_call(function, c);
            
            break;
          }
          case cst_type::or_operator: {
            function.code.emplace_back(op_logical_or);

            break;
          }
          case cst_type::and_operator: {
            function.code.emplace_back(op_logical_and);

            break;
          }
          case cst_type::equals_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setz);
            
            break;
          }
          case cst_type::not_equals_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setnz);
          
            break;
          }
          case cst_type::greater_than_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setg);
            
            break;
          }
          case cst_type::less_than_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setl);
            
            break;
          }
          case cst_type::greater_than_or_equal_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setge);
            
            break;
          }
          case cst_type::less_than_or_equal_operator: {
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_setle);
            
            break;
          }
          case cst_type::arrayaccess: {
            generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c));

            break;
          }
          default: {
            break;
          }
        }
      }

      if (array_access_node->get_children().back()->get_type() == cst_type::assignment) {
        auto assignment = cst::cast_raw<cst_assignment>(array_access_node->get_children().back().get());

        for (const auto& c : assignment->get_children()) {
          switch(c->get_type()) { /* ADD OTHER TYPES AND CASES TO THIS */
            case cst_type::number_literal: {
              handle_push_types(function, c.get());

              break;
            }
            default: {
              break;
            }
          }
        }
        
        function.code.emplace_back(op_array_store_element, identifier->content);
      }
      else {
        function.code.emplace_back(op_array_access_element, identifier->content);
      }
    }
    else if (array_access_node->get_children().at(1)->get_type() == cst_type::arrayaccess) { // 2d+
      // need to implement this
    }
  }

  void ir_gen::generate_block(ir_function& function, cst_block* block_node, std::string current_break_label, std::string current_loop_start) {
    for (const auto& c : block_node->get_children()) {
      switch(c->get_type()) {
        case cst_type::int8_datatype:
        case cst_type::int16_datatype:
        case cst_type::int32_datatype:
        case cst_type::int64_datatype: {
          auto node = c.get();
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get()); // name
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get()); // expression stuff
          
          generate_common<std::int64_t>(function, assignment);
          
          function.code.emplace_back(op_store, identifier->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::uint8_datatype:
        case cst_type::uint16_datatype:
        case cst_type::uint32_datatype: 
        case cst_type::uint64_datatype: {
          auto node = c.get();
          
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get());
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
          
          generate_common<std::uint64_t>(function, assignment);
          
          function.code.emplace_back(op_store, identifier->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::float64_datatype: {
          auto node = c.get();
          
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get());
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
          
          generate_common<double>(function, assignment);
          
          function.code.emplace_back(op_storef, identifier->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::float32_datatype: {
          auto node = c.get();
          
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get());
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
          
          generate_common<float>(function, assignment);
          
          function.code.emplace_back(op_storef, identifier->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::string_datatype: {
          auto node = c.get();
          
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get());
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
          
          generate_common<std::string>(function, assignment);
          
          function.code.emplace_back(op_store, identifier->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::array: {
          generate_array_decl(function, cst::cast_raw<cst_array>(c.get()));

          break;
        }
        case cst_type::arrayaccess: {
          generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));

          break;
        }
        case cst_type::identifier: {
          generate_common<std::int64_t>(function, c.get());
          
          function.code.emplace_back(op_store, c->content, c->to_string().substr(4, c->to_string().size()));
          
          break;
        }
        case cst_type::dereference: { // i think this shgould be good enough
          auto node = c.get();
          
          auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get());
          auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
                    
          auto deref_count = from_numerical_string<std::int64_t>(node->content);

          function.code.emplace_back(op_dereference_assign, deref_count);
          function.code.emplace_back(op_load, identifier->content);

          generate_common<std::int64_t>(function, assignment);

          function.code.emplace_back(op_store_at_addr);

          break;
        }
        case cst_type::functioncall: {
          generate_function_call(function, c.get());
          
          break;
        }
        case cst_type::ifstmt: {
          generate_if(function, cst::cast_raw<cst_ifstmt>(c.get()), current_break_label, current_loop_start);
          
          break;
        }
        case cst_type::continuestmt: {
          function.code.emplace_back(op_jmp, current_loop_start);
          
          break;
        }
        case cst_type::breakstmt: {
          function.code.emplace_back(op_jmp, current_break_label);
          
          break;
        }
        case cst_type::loopstmt: {
          generate_loop(function, cst::cast_raw<cst_loopstmt>(c.get()));
          
          break;
        }
        case cst_type::whilestmt: {
          generate_while(function, cst::cast_raw<cst_whilestmt>(c.get()));
          
          break;
        }
        case cst_type::forstmt: { // only supporting the normal for loop for now, not foreach
          generate_for(function, cst::cast_raw<cst_forstmt>(c.get()));
          
          break;
        }
        case cst_type::returnstmt: {
          generate_return(function, cst::cast_raw<cst_returnstmt>(c.get()));
          
          function.code.emplace_back(op_ret);
          
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  
  std::string ir_gen::create_label() {
    std::string label_name = "label_" + std::to_string(label_count++);
    label_map[label_name] = label_count;
    
    return label_name;
  }

  std::string ir_gen::create_temp_var() {
    std::string temp_var = "tmpv_" + std::to_string(temp_var_count++);
    temp_var_map[temp_var] = temp_var_count;
    
    return temp_var;
  }
  
  void ir_gen::place_label(ir_function& function, std::string label_name) {
    function.code.emplace_back(label, label_name);
  }

  void ir_gen::place_temp_var(ir_function& function, std::string var_name) {
    function.code.emplace_back(op_store, var_name);
  }
  
  void ir_gen::visualize_stack_ir(std::vector<ir_function> funcs) {
    for (auto& func : funcs) {
      std::cout << "\n" << func.type << "\n";
      std::cout << func.name << "\n";
      
      std::cout << "args:\n";
      for (auto& arg : func.args) {
        std::cout << "\t" << arg.type << "\n";
        std::cout << "\t" << arg.name << "\n";
      }
      
      std::cout << "code:\n";
      for (auto& i : func.code) {
        std::cout << occult::opcode_to_string(i.op) << " ";
        std::visit(visitor_stack(), i.operand);
      }
    }
  }
  
  std::vector<ir_function> ir_gen::lower_to_stack() {
    std::vector<ir_function> functions;
    
    for (const auto& c : root->get_children()) {
      auto type = c->get_type();
      
      if (type == cst_type::function) {
        functions.emplace_back(generate_function(cst::cast_raw<cst_function>(c.get())));
      }
    }

    for (auto& func : functions) { // fix unary ordering
      for (std::size_t i = 0; i < func.code.size(); i++) {
        switch (func.code.at(i).op) {
          case ir_opcode::op_bitwise_not:
          case ir_opcode::op_not:
          case ir_opcode::op_negate: {
            if (func.code.at(i + 1).op == ir_opcode::op_load || func.code.at(i + 1).op == ir_opcode::op_push) {
              std::iter_swap(func.code.begin() + i, func.code.begin() + i + 1);
            }

            break;
          }
          default: {
            break;
          }
        }
      }
    }
    
    return functions;
  }

  struct reg_visitor {
    void operator()(const float& v){ std::cout << v; };
    void operator()(const double& v){ std::cout << v; };
    void operator()(const std::int64_t& v){ std::cout << v; };
    void operator()(const std::uint64_t& v){ std::cout << v; };
    void operator()(const std::string& v){ std::cout << v; };
    void operator()(std::monostate){ std::cout << ""; };
    void operator()(ir_register v){ std::cout << "r" << static_cast<int>(v); };
  };
  
  void ir_gen::visualize_register_ir(std::vector<ir_reg_function> funcs) {
    for (auto& func : funcs) {
      std::cout << "\n" << func.type << "\n";
      std::cout << func.name << "\n";
      
      std::cout << "args:\n";
      for (auto& arg : func.args) {
        std::cout << "\t" << arg.type << "\n";
        std::cout << "\t" << arg.name << "\n";
      }
      
      std::cout << "code:\n";
      for (auto& i : func.code) {
        std::cout << occult::opcode_to_string(i.op) << " ";
        std::visit(reg_visitor(), i.dest);
        std::cout << " ";
        std::visit(reg_visitor(), i.src);
        std::cout << "\n";
      }
    }
  }
} // namespace occult
