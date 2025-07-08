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
      case cst_type::unary_not_operator: {
        function.code.emplace_back(op_not);
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
      generate_arith_and_bitwise_operators(function, c.get());
      
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
        default: {
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
        default: {
          break;
        }
      }
    }
  }
  
  void ir_gen::generate_if(ir_function& function, cst_ifstmt* if_node, std::string current_break_label, std::string current_loop_start) {
    std::string L1 = create_label();
    std::string L2 = create_label();
    
    // expression
    for (const auto& c : if_node->get_children()) { 
      generate_arith_and_bitwise_operators(function, c.get());
    }
    
    for (const auto& c : if_node->get_children()) { 
      switch (c->get_type()) {
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
          function.code.emplace_back(op_jnz, L2);
          
          break;
        }
        case cst_type::not_equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jz, L2);
         
          break;
        }
        case cst_type::greater_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jl, L2);
          
          break;
        }
        case cst_type::less_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jg, L2);
          
          break;
        }
        case cst_type::greater_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jle, L2);
          
          break;
        }
        case cst_type::less_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jge, L2);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    // block
    for (const auto& c : if_node->get_children()) {
      if (c->get_type() == cst_type::block) {
        generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);
      }
    }
    
    function.code.emplace_back(op_jmp, L1);
    
    // label for if block
    place_label(function, L2);
    
    // process other blocks / ifelse, else etc.
    for (const auto& c1 : if_node->get_children()) {
      if (c1->get_type() == cst_type::elseifstmt) {
        L2 = create_label();
        
        for (const auto& c : c1->get_children()) { 
          generate_arith_and_bitwise_operators(function, c.get());
        }
        
        for (const auto& c : c1->get_children()) { 
          switch (c->get_type()) {
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
              function.code.emplace_back(op_jnz, L2);
              
              break;
            }
            case cst_type::not_equals_operator: {
              function.code.emplace_back(op_cmp);
              function.code.emplace_back(op_jz, L2);
             
              break;
            }
            case cst_type::greater_than_operator: {
              function.code.emplace_back(op_cmp);
              function.code.emplace_back(op_jl, L2);
              
              break;
            }
            case cst_type::less_than_operator: {
              function.code.emplace_back(op_cmp);
              function.code.emplace_back(op_jg, L2);
              
              break;
            }
            case cst_type::greater_than_or_equal_operator: {
              function.code.emplace_back(op_cmp);
              function.code.emplace_back(op_jle, L2);
              
              break;
            }
            case cst_type::less_than_or_equal_operator: {
              function.code.emplace_back(op_cmp);
              function.code.emplace_back(op_jge, L2);
              
              break;
            }
            default: {
              break;
            }
          }
        }
        
        for (const auto& c : c1->get_children()) {
          if (c->get_type() == cst_type::block) {
            generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);
          }
        }
        
        function.code.emplace_back(op_jmp, L1);
        
        place_label(function, L2);
      }
      else if (c1->get_type() == cst_type::elsestmt) {
        for (const auto& c : c1->get_children()) {
          if (c->get_type() == cst_type::block) {
            generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);
          }
        }
        
        function.code.emplace_back(op_jmp, L1);
      }
    }
    
    // lcst label for all the blocks
    place_label(function, L1); 
  }
  
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
    auto L1 = create_label();
    auto B1 = create_label();
    
    place_label(function, L1); // L1 START
    
    // expression
    for (const auto& c : while_node->get_children()) { 
      generate_arith_and_bitwise_operators(function, c.get());
    }
    
    for (const auto& c : while_node->get_children()) { 
      switch (c->get_type()) {
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
          function.code.emplace_back(op_jnz, B1); // havent tested
          
          break;
        }
        case cst_type::not_equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jz, B1); // havent tested
         
          break;
        }
        case cst_type::greater_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jle, B1);
          
          break;
        }
        case cst_type::less_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jge, B1);
          
          break;
        }
        case cst_type::greater_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jl, B1);
          
          break;
        }
        case cst_type::less_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jg, B1);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    for (const auto& c : while_node->get_children()) {
      if (c->get_type() == cst_type::block) {
        generate_block(function, cst::cast_raw<cst_block>(c.get()), B1, L1);
      }
    }
    
    function.code.emplace_back(op_jmp, L1);
    
    place_label(function, B1);
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
    
    // expression
    for (const auto& c : condition->get_children()) { 
      generate_arith_and_bitwise_operators(function, c.get());
    }
    
    for (const auto& c : condition->get_children()) { 
      switch (c->get_type()) {
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
          function.code.emplace_back(op_jnz, B1); // havent tested
          
          break;
        }
        case cst_type::not_equals_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jz, B1); // havent tested
         
          break;
        }
        case cst_type::greater_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jle, B1);
          
          break;
        }
        case cst_type::less_than_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jge, B1);
          
          break;
        }
        case cst_type::greater_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jl, B1);
          
          break;
        }
        case cst_type::less_than_or_equal_operator: {
          function.code.emplace_back(op_cmp);
          function.code.emplace_back(op_jg, B1);
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
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
            auto element = cst::cast_raw<cst_arrayelement>(array_node->get_children().at(j).get());

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
      auto index = cst::cast_raw<cst_numberliteral>(array_access_node->get_children().at(1).get());

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
        
        function.code.emplace_back(op_declare_where_to_store, index->content);
        function.code.emplace_back(op_array_store_element, identifier->content);
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
  
  void ir_gen::place_label(ir_function& function, std::string label_name) {
    function.code.emplace_back(label, label_name);
  }
  
  struct visitor {
    void operator()(const float& v){ std::cout << v << "\n"; };
    void operator()(const double& v){ std::cout << v << "\n"; };
    void operator()(const std::int64_t& v){ std::cout << v << "\n"; };
    void operator()(const std::uint64_t& v){ std::cout << v << "\n"; };
    void operator()(const std::string& v){ std::cout << v << "\n"; };
    void operator()(std::monostate){ std::cout << "\n"; };
  };
  
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
        std::visit(visitor(), i.operand);
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
