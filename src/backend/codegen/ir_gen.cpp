#include "ir_gen.hpp"
#include "../../lexer/number_parser.hpp"
#include <algorithm>
#include <functional>

/*
 * This IR generation is really crappy and disorganized, I will make it
 * organized later on, and I'll probably move to SSA or TAC later on as well,
 * but this is what I can do for now, as I want to make actual progress on the
 * language, but we can still do some stack optimisations we can use this paper
 * (http://www.rigwit.co.uk/thesis/chap-5.pdf) as an example, and other theories
 * too.
 */

// use string(s) to determine the current exit, entry, etc labels

// add other cases to array declaration for types

namespace occult {
ir_function ir_gen::generate_function(cst_function *func_node) {
  ir_function function;

  for (const auto &c : func_node->get_children()) {
    switch (c->get_type()) {
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
    case cst_type::float32_datatype: {
      function.type = "float32";

      break;
    }
    case cst_type::float64_datatype: {
      function.type = "float64";

      break;
    }
    case cst_type::structure: {
      function.type = c->content;

      break;
    }
    case cst_type::functionarguments: {
      generate_function_args(function,
                             cst::cast_raw<cst_functionargs>(c.get()));

      break;
    }
    case cst_type::identifier: {
      function.name = c->content;

      break;
    }
    case cst_type::func_uses_shellcode: {
      function.uses_shellcode = true;

      break;
    }
    case cst_type::shellcode: {
      for (auto &num_literal : c->get_children()) {
        if (num_literal->get_type() == cst_type::number_literal) {
          function.code.emplace_back(
              op_push_shellcode,
              from_numerical_string<std::uint8_t>(num_literal->content));
        } else {
          std::cout << RED << "[IR ERR] Expected number literal in shellcode.\n"
                    << RESET;

          break;
        }
      }

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

  func_map.emplace(function.name, function);

  return function;
}

void ir_gen::generate_function_args(ir_function &function,
                                    cst_functionargs *func_args_node) {
  for (const auto &arg : func_args_node->get_children()) {
    bool is_ref = false;
    if (arg->content == "reference") {
      is_ref = true;
    }

    for (const auto &c : arg->get_children()) {
      auto type = arg->to_string().substr(4, arg->to_string().size()) +
                  ((is_ref) ? "_reference" : "");

      // for struct types, use the actual struct name from content
      if (arg->get_type() == cst_type::structure) {
        type = arg->content + ((is_ref) ? "_reference" : "");
      }

      auto variable_name = c->content;

      if (debug) {
        std::cout << CYAN << "[IR GEN] Arg (type, name): " << RESET << "("
                  << type << ", " << variable_name << ")" << std::endl;
      }

      function.args.emplace_back(variable_name, type);
    }
  }
}

void ir_gen::generate_arith_and_bitwise_operators(
    ir_function &function, cst *c, std::optional<std::string> type) {
  switch (c->get_type()) {
  case cst_type::add_operator: {
    if (type.has_value() && type.value() == "float32") {
      function.code.emplace_back(op_addf32);
    } else if (type.has_value() && type.value() == "float64") {
      function.code.emplace_back(op_addf64);
    } else {
      function.code.emplace_back(op_add);
    }

    break;
  }
  case cst_type::subtract_operator: {
    if (type.has_value() && type.value() == "float32") {
      function.code.emplace_back(op_subf32);
    } else if (type.has_value() && type.value() == "float64") {
      function.code.emplace_back(op_subf64);
    } else {
      function.code.emplace_back(op_sub);
    }

    break;
  }
  case cst_type::multiply_operator: {
    if (type.has_value() && type.value() == "float32") {
      function.code.emplace_back(op_mulf32);
    } else if (type.has_value() && type.value() == "float64") {
      function.code.emplace_back(op_mulf64);
    } else {
      ir_opcode to_push = is_signed[type.value()] ? op_imul : op_mul;
      function.code.emplace_back(to_push);
    }

    break;
  }
  case cst_type::division_operator: {
    if (type.has_value() && type.value() == "float32") {
      function.code.emplace_back(op_divf32);
    } else if (type.has_value() && type.value() == "float64") {
      function.code.emplace_back(op_divf64);
    } else {
      ir_opcode to_push = is_signed[type.value()] ? op_idiv : op_div;
      function.code.emplace_back(to_push);
    }

    break;
  }
  case cst_type::modulo_operator: {
    if (type.has_value() && type.value() == "float32") {
      function.code.emplace_back(op_modf32);
    } else if (type.has_value() && type.value() == "float64") {
      function.code.emplace_back(op_modf64);
    } else {
      ir_opcode to_push = is_signed[type.value()] ? op_imod : op_mod;
      function.code.emplace_back(to_push);
    }

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
  case cst_type::unary_plus_operator: {
    // just purely syntax
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

void ir_gen::emit_comparison(ir_function &function,
                             std::optional<std::string> type) {
  if (type.has_value()) {
    if (type.value() == "float32") {
      function.code.emplace_back(op_cmpf32);
    } else if (type.value() == "float64") {
      function.code.emplace_back(op_cmpf64);
    } else {
      function.code.emplace_back(op_cmp);
    }
  } else {
    function.code.emplace_back(op_cmp);
  }
}

bool is_logical(cst *node) {
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

void ir_gen::generate_boolean_value(ir_function &function, cst *node,
                                    type_of_push type_push) {
  bool has_logical = false;
  for (const auto &c : node->get_children()) {
    if (is_logical(c.get())) {
      has_logical = true;

      if (debug) {
        std::cout << CYAN
                  << "[IR GEN] Generating boolean value for comparison.\n"
                  << RESET;
      }

      break;
    }
  }

  std::optional<std::string> expr_type = std::nullopt;
  for (const auto &c : node->get_children()) {
    if (c->get_type() == cst_type::float_literal) {
      expr_type = "float64"; // default
      break;
    } else if (c->get_type() == cst_type::identifier) {
      if (local_variable_map.find(function) != local_variable_map.end()) {
        auto &var_map = local_variable_map[function];
        if (var_map.find(c->content) != var_map.end()) {
          expr_type = var_map[c->content];
          break;
        }
      }
    }
  }

  if (!has_logical) {
    for (const auto &c : node->get_children()) {
      switch (c->get_type()) {
      case cst_type::number_literal:
        handle_push_types(function, c.get());
        break;
      case cst_type::float_literal:
        handle_push_types(function, c.get());
        break;
      case cst_type::charliteral: {
        handle_push_types(function, c.get());
        break;
      }
      case cst_type::identifier:
        function.code.emplace_back(op_load, c->content);
        break;
      case cst_type::functioncall:
        generate_function_call(function, c.get());
        break;
      case cst_type::arrayaccess:
        generate_array_access(function,
                              cst::cast_raw<cst_arrayaccess>(c.get()));
        break;
      case cst_type::equals_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setz);
        break;
      case cst_type::not_equals_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setnz);
        break;
      case cst_type::greater_than_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setg);
        break;
      case cst_type::less_than_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setl);
        break;
      case cst_type::greater_than_or_equal_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setge);
        break;
      case cst_type::less_than_or_equal_operator:
        emit_comparison(function, expr_type);
        function.code.emplace_back(op_setle);
        break;

      default:
        generate_arith_and_bitwise_operators(function, c.get());
        break;
      }
    }
    return;
  }

  const std::string true_label = create_label();
  const std::string false_label = create_label();
  const std::string end_label = create_label();

  generate_condition(function, node, false_label, true_label);

  place_label(function, true_label);
  switch (type_push) {
  case type_of_push::ret: {
    if (debug) {
      std::cout << BLUE << "[IR GEN] Push type: ret\n" << RESET;
    }
    function.code.emplace_back(op_push_for_ret, std::int64_t(1));
    break;
  }
  case type_of_push::single: {
    if (debug) {
      std::cout << BLUE << "[IR GEN] Push type: single\n" << RESET;
    }
    function.code.emplace_back(op_push_single, std::int64_t(1));
    break;
  }
  default:
    function.code.emplace_back(op_push, std::int64_t(1));
    break;
  }

  function.code.emplace_back(op_jmp, end_label);

  place_label(function, false_label);
  switch (type_push) {
  case type_of_push::ret: {
    if (debug) {
      std::cout << BLUE << "[IR GEN] Push type: ret\n" << RESET;
    }
    function.code.emplace_back(op_push_for_ret, std::int64_t(0));
    break;
  }
  case type_of_push::single: {
    if (debug) {
      std::cout << BLUE << "[IR GEN] Push type: single\n" << RESET;
    }
    function.code.emplace_back(op_push_single, std::int64_t(0));
    break;
  }
  default:
    function.code.emplace_back(op_push, std::int64_t(0));
    break;
  }
  place_label(function, end_label);
}

template <typename IntType>
void ir_gen::generate_common_generic(ir_function &function, cst *node,
                                     std::optional<std::string> type,
                                     type_of_push type_push) {
  bool needs_value = false;
  for (const auto &c : node->get_children()) {
    if (c->get_type() == cst_type::equals_operator ||
        c->get_type() == cst_type::not_equals_operator ||
        c->get_type() == cst_type::and_operator ||
        c->get_type() == cst_type::or_operator ||
        c->get_type() == cst_type::greater_than_operator ||
        c->get_type() == cst_type::less_than_operator ||
        c->get_type() == cst_type::greater_than_or_equal_operator ||
        c->get_type() == cst_type::less_than_or_equal_operator) {
      needs_value = true;
      break;
    }
  }

  if (needs_value) {
    generate_boolean_value(function, node, type_push);
    return;
  }

  for (const auto &c : node->get_children()) {
    switch (c->get_type()) {
    case cst_type::number_literal:
      function.code.emplace_back(op_push,
                                 from_numerical_string<IntType>(c->content));
      break;
    case cst_type::float_literal:
      function.code.emplace_back(op_push,
                                 from_numerical_string<IntType>(c->content));
      break;
    case cst_type::charliteral: {
      function.code.emplace_back(op_push,
                                 from_numerical_string<char>(c->content));
      break;
    }
    case cst_type::identifier:
      function.code.emplace_back(op_load, c->content);
      break;
    case cst_type::stringliteral:
      function.code.emplace_back(op_push, c->content);
      break;
    case cst_type::functioncall:
      generate_function_call(function, c.get());
      break;
    case cst_type::arrayaccess:
      generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
      break;
    case cst_type::memberaccess:
      generate_member_access(function,
                             cst::cast_raw<cst_memberaccess>(c.get()));
      break;
    case cst_type::reference:
      function.code.emplace_back(op_reference);
      break;
    case cst_type::dereference:
      function.code.emplace_back(
          op_dereference, from_numerical_string<std::int64_t>(c->content));
      break;
    default:
      generate_arith_and_bitwise_operators(
          function, c.get(), (type.has_value() ? type.value() : "int64"));
      break;
    }
  }
}

void ir_gen::handle_push_types(ir_function &function, cst *c,
                               std::optional<std::string> type) {
  if (!type.has_value()) {
    type = function.type;
  }

  if (auto it = ir_typemap.find(type.value()); it != ir_typemap.end()) {
    switch (it->second) {
    case ir_typename::int64: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::int64_t>(c->content));

      break;
    }
    case ir_typename::int32: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::int32_t>(c->content));

      break;
    }
    case ir_typename::int16: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::int16_t>(c->content));

      break;
    }
    case ir_typename::int8: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::int8_t>(c->content));

      break;
    }
    case ir_typename::boolean: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::int8_t>(c->content));

      break;
    }
    case ir_typename::uint64: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::uint64_t>(c->content));

      break;
    }
    case ir_typename::uint32: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::uint32_t>(c->content));

      break;
    }
    case ir_typename::uint16: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::uint16_t>(c->content));

      break;
    }
    case ir_typename::uint8: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::uint8_t>(c->content));

      break;
    }
    case ir_typename::float64: {
      function.code.emplace_back(op_push,
                                 from_numerical_string<double>(c->content));

      break;
    }
    case ir_typename::float32: {
      function.code.emplace_back(op_push,
                                 from_numerical_string<float>(c->content));

      break;
    }
    case ir_typename::string: {
      function.code.emplace_back(
          op_push, from_numerical_string<std::string>(c->content));

      break;
    }
    }
  } else {
    // assume struct type
    generate_common_generic<std::int64_t>(function, c);
  }
}

void ir_gen::generate_common(ir_function &function, cst *c, std::string type,
                             type_of_push type_push) {
  bool is_ref = false;

  if (type.find("_reference") != std::string::npos) {
    is_ref = true;
  }
  type = type.substr(0, type.find("_reference"));

  if (auto it = ir_typemap.find(type); it != ir_typemap.end()) {
    if (is_ref) {
      function.code.emplace_back(op_reference);
    }

    switch (it->second) {
    case ir_typename::int64: {
      generate_common_generic<std::int64_t>(function, c, "int64", type_push);

      break;
    }
    case ir_typename::int32: {
      generate_common_generic<std::int32_t>(function, c, "int32", type_push);

      break;
    }
    case ir_typename::int16: {
      generate_common_generic<std::int16_t>(function, c, "int16", type_push);

      break;
    }
    case ir_typename::int8: {
      generate_common_generic<std::int8_t>(function, c, "int8", type_push);

      break;
    }
    case ir_typename::uint64: {
      generate_common_generic<std::uint64_t>(function, c, "uint64", type_push);

      break;
    }
    case ir_typename::uint32: {
      generate_common_generic<std::uint32_t>(function, c, "uint32", type_push);

      break;
    }
    case ir_typename::uint16: {
      generate_common_generic<std::uint16_t>(function, c, "uint16", type_push);

      break;
    }
    case ir_typename::uint8: {
      generate_common_generic<std::uint8_t>(function, c, "uint8", type_push);

      break;
    }
    case ir_typename::float64: {
      generate_common_generic<double>(function, c, "float64", type_push);

      break;
    }
    case ir_typename::float32: {
      generate_common_generic<float>(function, c, "float32", type_push);

      break;
    }
    case ir_typename::string: {
      generate_common_generic<std::string>(function, c, "str", type_push);

      break;
    }
    case ir_typename::boolean: {
      generate_common_generic<std::int8_t>(function, c, "bool", type_push);

      break;
    }
    }
  } else {
    // assume struct type
    generate_common_generic<std::int64_t>(function, c);
  }
}

void ir_gen::generate_function_call(ir_function &function, cst *c) {
  const auto node = cst::cast_raw<cst_functioncall>(c);

  const auto identifier = cst::cast_raw<cst_identifier>(
      node->get_children().front().get()); // name of call

  const auto func_it = func_map.find(identifier->content);
  if (func_it == func_map.end()) {
    if (debug) {
      std::cout << CYAN << "[IR GEN] External/built-in function call: "
                << identifier->content << RESET << std::endl;
    }

    auto arg_location = 1;
    while (node->get_children().at(arg_location).get()->content != "end_call") {
      const auto arg_node = cst::cast_raw<cst_functionarg>(
          node->get_children().at(arg_location).get());

      generate_common(function, arg_node, "int64");

      ++arg_location;
    }

    function.code.emplace_back(op_call, identifier->content);

    return;
  }

  if (debug) {
    std::cout << CYAN << "[IR GEN] Args for call " << identifier->content
              << ": " << RESET << func_it->second.args.size() << std::endl;
  }

  auto arg_location = 1;
  while (node->get_children().at(arg_location).get()->content != "end_call") {
    const auto arg_node = cst::cast_raw<cst_functionarg>(
        node->get_children().at(arg_location).get());

    const auto argument_type = func_it->second.args.at(arg_location - 1).type;

    if (debug) {
      std::cout << CYAN << "[IR GEN] Generating argument for call (type, loc): "
                << RESET << "(" << argument_type << ", " << arg_location - 1
                << ")" << std::endl;
    }

    generate_common(function, arg_node, argument_type);

    ++arg_location;
  }

  function.code.emplace_back(op_call, identifier->content);
}

void ir_gen::generate_return(ir_function &function,
                             cst_returnstmt *return_node) {
  generate_common(function, return_node, function.type, type_of_push::ret);
  function.code.emplace_back(op_ret);
}

void ir_gen::generate_condition(ir_function &function, cst *node,
                                const std::string &false_label,
                                const std::string &true_label) {
  std::optional<std::string> expr_type =
      std::nullopt; // determine type for fpu comparison and or integer
  for (const auto &c : node->get_children()) {
    if (c->get_type() == cst_type::float_literal) {
      expr_type = "float64";
      break;
    } else if (c->get_type() == cst_type::identifier) {
      if (local_variable_map.find(function) != local_variable_map.end()) {
        auto &var_map = local_variable_map[function];
        if (var_map.find(c->content) != var_map.end()) {
          expr_type = var_map[c->content];
          break;
        }
      }
    }
  }

  auto handle_internal_push = [&, this](cst *expr) -> void {
    switch (expr->get_type()) {
    case cst_type::number_literal: {
      handle_push_types(function, expr);
      break;
    }
    case cst_type::float_literal: {
      handle_push_types(function, expr);
      break;
    }
    case cst_type::charliteral: {
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

  std::vector<cst *> logic_stack;
  std::vector<cst *> eval_stack;

  for (std::size_t i = 0; i < node->get_children().size(); i++) {
    const auto &c = node->get_children().at(i);

    switch (c->get_type()) {
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
      const std::size_t curr_pos = i;

      if (i > 0 && node->get_children().at(i - 1)->get_type() ==
                       cst_type::unary_not_operator) {
        std::size_t not_pos = i;
        std::size_t not_count = 0;

        while (not_pos > 0 &&
               node->get_children().at(not_pos - 1)->get_type() ==
                   cst_type::unary_not_operator) {
          not_pos--;
          not_count++;
        }

        const bool apply_not = not_count % 2 != 0;

        for (int j = static_cast<int>(curr_pos); j >= 0; j--) {
          const auto &curr_node = node->get_children().at(j);

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
    for (const auto &n : logic_stack) {
      n->visualize(2);
    }
    std::cout << BLUE << "[IR GEN] Logic stack size: " << RESET
              << logic_stack.size() << std::endl;
  }

  if (debug && !eval_stack.empty()) {
    std::cout << BLUE << "[IR GEN] Filled eval stack:" << RESET << std::endl;
    for (const auto &n : eval_stack) {
      n->visualize(2);
    }
    std::cout << BLUE << "[IR GEN] Eval stack size: " << RESET
              << eval_stack.size() << std::endl;
  }

  std::vector<std::vector<cst *>> eval_stack_m;

  std::vector<cst *> current_expr;
  for (auto &i : eval_stack) {
    current_expr.push_back(i);

    if (is_logical(i)) {
      eval_stack_m.push_back(current_expr);
      current_expr.clear();
    }
  }

  if (debug && !eval_stack_m.empty()) {
    std::cout << BLUE << "[IR GEN] Filled eval stack (m):" << RESET
              << std::endl;
    for (auto &arr : eval_stack_m) {
      std::cout << CYAN << "{" << RESET << std::endl;
      for (const auto &n : arr) {
        n->visualize(2);
      }
      std::cout << CYAN << "}" << RESET << std::endl;
    }
    std::cout << BLUE << "[IR GEN] Eval stack (m) size: " << RESET
              << eval_stack.size() << std::endl;
  }

  if (!logic_stack.empty()) {
    std::size_t eval_index = 0;

    for (const auto c : logic_stack) {
      switch (c->get_type()) {
      case cst_type::or_operator: {
        if (eval_index < eval_stack_m.size()) {
          const auto &expr = eval_stack_m[eval_index];

          for (size_t j = 0; j < expr.size() - 1; j++) {
            handle_internal_push(expr[j]);
          }

          emit_comparison(function, expr_type);

          if (expr.back()->do_not) {
            generate_and_jump(function, expr.back(), true_label);
          } else {
            generate_or_jump(function, expr.back(), true_label);
          }

          eval_index++;
        }
        break;
      }
      case cst_type::and_operator: {
        if (eval_index < eval_stack_m.size()) {
          const auto &expr = eval_stack_m[eval_index];

          for (size_t j = 0; j < expr.size() - 1; j++) {
            handle_internal_push(expr[j]);
          }

          emit_comparison(function, expr_type);

          if (expr.back()->do_not) {
            generate_or_jump(function, expr.back(), false_label);
          } else {
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

    while (eval_index < eval_stack_m.size()) {
      const auto &expr = eval_stack_m[eval_index];

      for (size_t j = 0; j < expr.size() - 1; j++) {
        handle_internal_push(expr[j]);
      }

      emit_comparison(function, expr_type);

      if (expr.back()->do_not) {
        generate_inverted_jump(function, expr.back(), false_label);
      } else {
        generate_normal_jump(function, expr.back(), false_label);
      }

      eval_index++;
    }
  } else {
    if (!eval_stack_m.empty()) {
      const auto &expr = eval_stack_m[0];

      for (size_t j = 0; j < expr.size() - 1; j++) {
        handle_internal_push(expr[j]);
      }

      emit_comparison(function, expr_type);

      if (expr.back()->do_not) {
        generate_inverted_jump(function, expr.back(), false_label);
      } else {
        generate_normal_jump(function, expr.back(), false_label);
      }
    }
  }
}

void ir_gen::generate_if(ir_function &function, cst_ifstmt *if_node,
                         const std::string &current_break_label,
                         const std::string &current_loop_start) {
  const std::string false_label = create_label(); // exit / else or whatever
  const std::string true_label = create_label();  // block
  const std::string end_label =
      create_label(); // end of entire if/elseif/else chain

  generate_condition(function, if_node, false_label, true_label);

  place_label(function, true_label);

  for (const auto &c : if_node->get_children()) {
    if (c->get_type() == cst_type::block) {
      generate_block(function, cst::cast_raw<cst_block>(c.get()),
                     current_break_label, current_loop_start);
      break;
    }
  }

  function.code.emplace_back(op_jmp, end_label);

  place_label(function, false_label);

  for (const auto &elseif_node : if_node->get_children()) {
    if (elseif_node->get_type() == cst_type::elseifstmt) {
      std::string elseif_true_label = create_label();
      std::string elseif_false_label = create_label();

      generate_condition(function, elseif_node.get(), elseif_false_label,
                         elseif_true_label);

      place_label(function, elseif_true_label);

      for (const auto &c : elseif_node->get_children()) {
        if (c->get_type() == cst_type::block) {
          generate_block(function, cst::cast_raw<cst_block>(c.get()),
                         current_break_label, current_loop_start);
          break;
        }
      }

      function.code.emplace_back(op_jmp, end_label);

      place_label(function, elseif_false_label);
    } else if (elseif_node->get_type() == cst_type::elsestmt) {
      for (const auto &b : elseif_node->get_children()) {
        if (b->get_type() == cst_type::block) {
          generate_block(function, cst::cast_raw<cst_block>(b.get()),
                         current_break_label, current_loop_start);
          break;
        }
      }
    }
  }

  place_label(function, end_label);
}

void ir_gen::generate_or_jump(ir_function &function, cst *comparison,
                              const std::string &true_label) {
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

void ir_gen::generate_and_jump(ir_function &function, cst *comparison,
                               const std::string &false_label) {
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

void ir_gen::generate_normal_jump(ir_function &function, cst *comparison,
                                  const std::string &false_label) {
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

void ir_gen::generate_inverted_jump(ir_function &function, cst *comparison,
                                    const std::string &false_label) {
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

void ir_gen::generate_loop(ir_function &function, cst_loopstmt *loop_node) {
  auto L1 = create_label();
  const auto B1 = create_label();

  place_label(function, L1);

  for (const auto &c : loop_node->get_children()) {
    if (c->get_type() == cst_type::block) {
      generate_block(function, cst::cast_raw<cst_block>(c.get()), B1, L1);
    }
  }

  function.code.emplace_back(op_jmp, L1);

  place_label(function, B1);
}

void ir_gen::generate_while(ir_function &function, cst_whilestmt *while_node) {
  std::string loop_label = create_label();
  const std::string break_label = create_label();

  place_label(function, loop_label);

  generate_condition(function, while_node, break_label, loop_label);

  for (const auto &c : while_node->get_children()) {
    if (c->get_type() == cst_type::block) {
      generate_block(function, cst::cast_raw<cst_block>(c.get()), break_label,
                     loop_label);
    }
  }

  function.code.emplace_back(op_jmp, loop_label);

  place_label(function, break_label);
}

void ir_gen::generate_for(ir_function &function, cst_forstmt *for_node) {
  const auto first_node = for_node->get_children().front().get();
  const auto condition = for_node->get_children().at(1).get();
  const auto body = for_node->get_children().back().get();

  switch (for_node->get_children().front()->get_type()) {
  case cst_type::int8_datatype:
  case cst_type::int16_datatype:
  case cst_type::int32_datatype:
  case cst_type::int64_datatype: {
    const auto identifier = cst::cast_raw<cst_identifier>(
        first_node->get_children().front().get()); // name
    const auto assignment =
        cst::cast_raw<cst_assignment>(first_node->get_children().back().get());
    // expression stuff

    generate_common_generic<std::int64_t>(function, assignment);

    function.code.emplace_back(
        op_store, identifier->content,
        first_node->to_string().substr(4, first_node->to_string().size()));

    break;
  }
  case cst_type::uint8_datatype:
  case cst_type::uint16_datatype:
  case cst_type::uint32_datatype:
  case cst_type::uint64_datatype: {
    const auto identifier =
        cst::cast_raw<cst_identifier>(first_node->get_children().front().get());
    const auto assignment =
        cst::cast_raw<cst_assignment>(first_node->get_children().back().get());

    generate_common_generic<std::uint64_t>(function, assignment);

    function.code.emplace_back(
        op_store, identifier->content,
        first_node->to_string().substr(4, first_node->to_string().size()));

    break;
  }
  default: {
    break;
  }
  }

  auto L1 = create_label();
  const auto B1 = create_label();

  place_label(function, L1); // L1 START

  generate_condition(function, condition, B1, L1);

  generate_block(function, cst::cast_raw<cst_block>(body), B1, L1);

  const auto new_block = cst::new_node<cst_block>(); // swapping parent
  for (auto &c : for_node->get_children().at(2)->get_children()) {
    new_block->add_child(std::move(c));
  }

  generate_block(function, new_block.get()); // iterator

  function.code.emplace_back(op_jmp, L1);

  place_label(function, B1);
}

void ir_gen::generate_array_decl(ir_function &function, cst_array *array_node) {
  auto type = array_node->get_children().front().get();

  auto identifier =
      cst::cast_raw<cst_identifier>(type->get_children().front().get());

  local_array_map[function][identifier->content] =
      type->to_string().substr(4, type->to_string().size());

  if (debug) {
    std::cout << CYAN << "[IR GEN] Array type: " << RESET
              << type->to_string().substr(4, type->to_string().size())
              << std::endl;
    std::cout << CYAN << "[IR GEN] Array name: " << RESET << identifier->content
              << std::endl;
  }

  function.code.emplace_back(op_array_decl, identifier->content);
  function.code.emplace_back(
      op_decl_array_type,
      type->to_string().substr(4, type->to_string().size()));

  auto dimensions_count_node = cst::cast_raw<cst_dimensions_count>(
      array_node->get_children().at(1).get());

  if (debug) {
    std::cout << CYAN << "[IR GEN] Dimensions: " << RESET
              << dimensions_count_node->content << std::endl;
  }

  std::vector<std::uint64_t> dimensions;
  for (const auto &c : dimensions_count_node->get_children()) {
    if (c->get_type() == cst_type::dimension) {
      if (debug) {
        std::cout << CYAN << "[IR GEN] D: " << RESET << c->content << std::endl;
      }
      dimensions.push_back(from_numerical_string<std::uint64_t>(c->content));
    }
  }

  function.code.emplace_back(op_array_dimensions,
                             static_cast<std::uint64_t>(dimensions.size()));

  for (const auto &dim : dimensions) {
    function.code.emplace_back(op_array_size, dim);
  }

  // 1d arrays
  if (dimensions.size() == 1) {
    auto first_array_node =
        cst::cast_raw<cst_arraybody>(array_node->get_children().back().get());

    if (first_array_node == nullptr) {
      return;
    }

    for (std::size_t i = 0; i < dimensions.front(); ++i) {
      function.code.emplace_back(op_declare_where_to_store,
                                 static_cast<std::uint64_t>(i));

      auto element = cst::cast_raw<cst_arrayelement>(
          first_array_node->get_children().at(i).get());

      for (const auto &c : element->get_children()) {
        switch (c->get_type()) {
        case cst_type::number_literal: {
          handle_push_types(function, c.get(),
                            local_array_map[function][identifier->content]);
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
        case cst_type::arrayaccess: {
          generate_array_access(function,
                                cst::cast_raw<cst_arrayaccess>(c.get()));
          break;
        }
        case cst_type::dereference: {
          function.code.emplace_back(
              op_dereference, from_numerical_string<std::int64_t>(c->content));
          break;
        }
        default: {
          generate_arith_and_bitwise_operators(
              function, c.get(),
              local_array_map[function][identifier->content]);

          break;
        }
        }
      }

      function.code.emplace_back(op_array_store_element, identifier->content);
    }
  }
  // multidimensional arrays
  else {
    auto array_body =
        cst::cast_raw<cst_arraybody>(array_node->get_children().back().get());

    if (array_body == nullptr) {
      return;
    }

    std::uint64_t linear_index = 0;

    std::function<void(cst *, std::size_t)> traverse_array;
    traverse_array = [&](cst *current_body, std::size_t depth) {
      if (depth >= dimensions.size() || current_body == nullptr) {
        return;
      }

      for (std::size_t i = 0; i < current_body->get_children().size(); ++i) {
        auto child = current_body->get_children().at(i).get();

        // if this is the last dimension, we have an element
        if (depth == dimensions.size() - 1) {
          auto element = cst::cast_raw<cst_arrayelement>(child);

          if (element) {
            function.code.emplace_back(op_declare_where_to_store, linear_index);

            for (const auto &c : element->get_children()) {
              switch (c->get_type()) {
              case cst_type::number_literal: {
                handle_push_types(
                    function, c.get(),
                    local_array_map[function][identifier->content]);
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
              case cst_type::arrayaccess: {
                generate_array_access(function,
                                      cst::cast_raw<cst_arrayaccess>(c.get()));
                break;
              }
              case cst_type::dereference: {
                function.code.emplace_back(
                    op_dereference,
                    from_numerical_string<std::int64_t>(c->content));
                break;
              }
              default: {
                generate_arith_and_bitwise_operators(
                    function, c.get(),
                    local_array_map[function][identifier->content]);

                break;
              }
              }
            }

            function.code.emplace_back(op_array_store_element,
                                       identifier->content);
            linear_index++;
          }
        }
        // recurse into the next dimension
        else {
          auto nested_body = cst::cast_raw<cst_arraybody>(child);
          if (nested_body) {
            traverse_array(nested_body, depth + 1);
          }
        }
      }
    };

    traverse_array(array_body, 0);
  }
}

void ir_gen::generate_array_access(ir_function &function,
                                   cst_arrayaccess *array_access_node) {
  if (array_access_node->get_children().front()->get_type() ==
      cst_type::identifier) {
    // 1-dimensional array access
    auto identifier = cst::cast_raw<cst_identifier>(
        array_access_node->get_children().front().get());

    auto has_assigment = array_access_node->get_children().back()->get_type() ==
                         cst_type::assignment;

    if (debug) {
      std::cout << CYAN << "[IR GEN] Access Array Type: "
                << local_array_map[function][identifier->content] << RESET
                << std::endl;
    }

    for (std::size_t i = 1; i < array_access_node->get_children().size() -
                                    ((has_assigment) ? 1 : 0);
         i++) {
      auto c = array_access_node->get_children().at(i).get();

      switch (c->get_type()) {
      case cst_type::number_literal: {
        handle_push_types(function, c,
                          local_array_map[function][identifier->content]);

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
      case cst_type::assignment: {
        generate_common(function, c,
                        local_array_map[function][identifier->content]);

        break;
      }
      case cst_type::arrayaccess: {
        generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c));

        break;
      }
      default: {
        generate_arith_and_bitwise_operators(
            function, c, local_array_map[function][identifier->content]);

        break;
      }
      }
    }

    function.code.emplace_back(op_mark_for_array_access);

    if (array_access_node->get_children().back()->get_type() ==
        cst_type::assignment) {
      auto assignment = cst::cast_raw<cst_assignment>(
          array_access_node->get_children().back().get());

      for (const auto &c : assignment->get_children()) {
        switch (c->get_type()) {
        case cst_type::number_literal: {
          handle_push_types(function, c.get(),
                            local_array_map[function][identifier->content]);
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
        case cst_type::arrayaccess: {
          generate_array_access(function,
                                cst::cast_raw<cst_arrayaccess>(c.get()));
          break;
        }
        case cst_type::dereference: {
          function.code.emplace_back(
              op_dereference, from_numerical_string<std::int64_t>(c->content));
          break;
        }
        default: {
          generate_arith_and_bitwise_operators(
              function, c.get(),
              local_array_map[function][identifier->content]);
          break;
        }
        }
      }

      function.code.emplace_back(op_array_store_element, identifier->content);
    } else {
      function.code.emplace_back(op_array_access_element, identifier->content);
    }
  } else if (array_access_node->get_children().front()->get_type() ==
             cst_type::arrayaccess) {
    // 2d+
    auto depth = 1;
    std::vector<cst *> indices;
    cst_assignment *assignment = nullptr;

    std::function<void(cst *)> traverse_access_nodes(
        [&, this](cst *node) -> void {
          for (auto &child : node->get_children()) {
            switch (child->get_type()) {
            case cst_type::arrayaccess: {
              depth++;

              traverse_access_nodes(child.get());

              break;
            }
            case cst_type::assignment: {
              assignment = cst::cast_raw<cst_assignment>(child.get());

              return;
            }
            default: {
              indices.emplace_back(child.get());
              traverse_access_nodes(child.get());

              break;
            }
            }
          }
        });

    traverse_access_nodes(array_access_node);

    cst_identifier *array_name =
        cst::cast_raw<cst_identifier>(indices.front()); // ALWAYS FRONT
    indices = std::vector(indices.begin() + 1, indices.end());

    for (auto &c : indices) {
      // push indices in order
      switch (c->get_type()) {
      case cst_type::number_literal: {
        handle_push_types(function, c,
                          local_array_map[function][array_name->content]);
        // function.code.emplace_back(op_mark_for_array_access);

        break;
      }
      case cst_type::identifier: {
        function.code.emplace_back(op_load, c->content);
        // function.code.emplace_back(op_mark_for_array_access);

        break;
      }
      case cst_type::functioncall: {
        generate_function_call(function, c);
        // function.code.emplace_back(op_mark_for_array_access);

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
        // function.code.emplace_back(op_mark_for_array_access);

        break;
      }
      default: {
        generate_arith_and_bitwise_operators(
            function, c, local_array_map[function][array_name->content]);
        // function.code.emplace_back(op_mark_for_array_access);

        break;
      }
      }
    }

    if (assignment) {
      if (debug) {
        std::cout << CYAN << "[IR GEN] Array assignment child count: "
                  << assignment->get_children().size() << RESET << std::endl;
        for (const auto &child : assignment->get_children()) {
          std::cout << CYAN << "  [IR GEN] Child type: "
                    << static_cast<int>(child->get_type()) << RESET
                    << std::endl;
        }
      }
      for (const auto &c : assignment->get_children()) {
        switch (c->get_type()) {
        /* ADD OTHER TYPES AND CASES TO THIS */
        case cst_type::number_literal: {
          handle_push_types(function, c.get(),
                            local_array_map[function][array_name->content]);

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
        case cst_type::arrayaccess: {
          generate_array_access(function,
                                cst::cast_raw<cst_arrayaccess>(c.get()));

          break;
        }
        case cst_type::dereference: {
          function.code.emplace_back(
              op_dereference, from_numerical_string<std::int64_t>(c->content));

          break;
        }
        default: {
          generate_arith_and_bitwise_operators(
              function, c.get(),
              local_array_map[function][array_name->content]);

          break;
        }
        }
      }

      function.code.emplace_back(op_array_store_element, array_name->content);
    } else {
      function.code.emplace_back(op_array_access_element, array_name->content);
    }
  }
}

void ir_gen::generate_struct_decl(ir_function &function,
                                  cst_struct *struct_node) {
  function.code.emplace_back(op_struct_decl, struct_node->content);

  if (struct_node->get_children().size() > 1) {
    // assignment
    const auto id = struct_node->get_children().front().get(); // identifier
    const auto assignment =
        struct_node->get_children().back().get(); // assignment expression

    generate_common_generic<std::int64_t>(function, assignment);

    function.code.emplace_back(op_struct_store, id->content);
  } else {
    const auto id = struct_node->get_children().front().get(); // identifier

    function.code.emplace_back(op_struct_store, id->content);
  }
}

void ir_gen::generate_member_access(ir_function &function,
                                    cst_memberaccess *member_access_node) {
  const auto &children = member_access_node->get_children();

  if (children.empty()) {
    throw std::runtime_error("Member access with no children");
  }

  std::string base_var = children[0]->content;
  std::vector<std::string> member_chain;
  bool is_assignment = false;
  cst *assignment_node = nullptr;

  for (size_t i = 1; i < children.size(); i++) {
    if (children[i]->get_type() == cst_type::assignment) {
      is_assignment = true;
      assignment_node = children[i].get();
    } else {
      member_chain.push_back(children[i]->content);
    }
  }

  if (debug) {
    std::cout << CYAN << "[IR GEN] Member "
              << (is_assignment ? "store: " : "access: ") << RESET << base_var;
    for (const auto &member : member_chain) {
      std::cout << "." << member;
    }
    std::cout << std::endl;
  }

  if (is_assignment) {
    function.code.emplace_back(op_load, base_var);

    for (size_t i = 0; i < member_chain.size() - 1; i++) {
      function.code.emplace_back(op_member_access, member_chain[i]);
    }

    generate_common_generic<std::int64_t>(function, assignment_node);

    if (!member_chain.empty()) {
      function.code.emplace_back(op_member_store, member_chain.back());
    }
  } else {
    function.code.emplace_back(op_load, base_var);

    for (const auto &member : member_chain) {
      function.code.emplace_back(op_member_access, member);
    }
  }
}

void ir_gen::generate_block(ir_function &function, cst_block *block_node,
                            std::string current_break_label,
                            std::string current_loop_start) {
  for (const auto &c : block_node->get_children()) {
    switch (c->get_type()) {
    case cst_type::int8_datatype:
    case cst_type::int16_datatype:
    case cst_type::int32_datatype:
    case cst_type::int64_datatype:
    case cst_type::uint8_datatype:
    case cst_type::uint16_datatype:
    case cst_type::uint32_datatype:
    case cst_type::uint64_datatype:
    case cst_type::float64_datatype:
    case cst_type::float32_datatype:
    case cst_type::string_datatype: {
      const auto node = c.get();

      std::string type_with_ptrs =
          c->to_string().substr(4, c->to_string().size());
      if (node->num_pointers > 0) {
        type_with_ptrs += "_ptr";
      }

      const auto identifier = cst::cast_raw<cst_identifier>(
          node->get_children().front().get()); // name

      // Only process assignment if it exists (variable might be declared
      // without initialization)
      if (node->get_children().size() > 1 &&
          node->get_children().back()->get_type() == cst_type::assignment) {
        const auto assignment =
            cst::cast_raw<cst_assignment>(node->get_children().back().get());
        generate_common(function, assignment,
                        type_with_ptrs); // expression stuff
        function.code.emplace_back(op_store, identifier->content,
                                   type_with_ptrs);
      }
      // If no assignment, just register the variable - space will be allocated
      // on first use

      local_variable_map[function][identifier->content] = type_with_ptrs;

      break;
    }
    case cst_type::bool_datatype: {
      const auto node = c.get();

      std::string type_with_ptrs =
          c->to_string().substr(4, c->to_string().size());
      if (node->num_pointers > 0) {
        type_with_ptrs += "_ptr";
      }

      const auto identifier = cst::cast_raw<cst_identifier>(
          node->get_children().front().get()); // name

      // Only process assignment if it exists (variable might be declared
      // without initialization)
      if (node->get_children().size() > 1 &&
          node->get_children().back()->get_type() == cst_type::assignment) {
        const auto assignment =
            cst::cast_raw<cst_assignment>(node->get_children().back().get());
        generate_common(function, assignment, type_with_ptrs,
                        type_of_push::single); // expression stuff
        function.code.emplace_back(op_store, identifier->content,
                                   type_with_ptrs);
      }
      // If no assignment, just register the variable - space will be allocated
      // on first use

      local_variable_map[function][identifier->content] = type_with_ptrs;

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
    case cst_type::structure: {
      generate_struct_decl(function, cst::cast_raw<cst_struct>(c.get()));

      break;
    }
    case cst_type::identifier: {
      if (debug) {
        std::cout << CYAN << "[IR GEN] Loading: "
                  << local_variable_map[function][c->content] << std::endl;
      }
      generate_common(function, c.get(),
                      local_variable_map[function][c->content]);

      function.code.emplace_back(op_store, c->content,
                                 local_variable_map[function][c->content]);

      break;
    }
    case cst_type::dereference: {
      // i think this shgould be good enough
      auto node = c.get();

      auto expr =
          cst::cast_raw<cst_generic_expr>(node->get_children().front().get());
      auto assignment =
          cst::cast_raw<cst_assignment>(node->get_children().back().get());

      auto deref_count = from_numerical_string<std::int64_t>(node->content);

      function.code.emplace_back(op_dereference_assign, deref_count);
      generate_common(function, expr, "int64");
      // function.code.emplace_back(op_load, identifier->content);

      generate_common_generic<std::int64_t>(function, assignment);

      function.code.emplace_back(op_store_at_addr);

      break;
    }
    case cst_type::functioncall: {
      generate_function_call(function, c.get());

      break;
    }
    case cst_type::ifstmt: {
      generate_if(function, cst::cast_raw<cst_ifstmt>(c.get()),
                  current_break_label, current_loop_start);

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
    case cst_type::forstmt: {
      // only supporting the normal for loop for now, not foreach
      generate_for(function, cst::cast_raw<cst_forstmt>(c.get()));

      break;
    }
    case cst_type::returnstmt: {
      generate_return(function, cst::cast_raw<cst_returnstmt>(c.get()));

      break;
    }
    case cst_type::memberaccess: {
      generate_member_access(function,
                             cst::cast_raw<cst_memberaccess>(c.get()));

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

void ir_gen::place_label(ir_function &function, std::string label_name) {
  function.code.emplace_back(label, label_name);
}

void ir_gen::visualize_stack_ir(const std::vector<ir_function> &funcs) {
  std::cout << CYAN << "Function(s): \n" << RESET;
  for (const auto &[code, args, name, type, uses_shellcode, is_external] :
       funcs) {
    std::cout << "Type: " << type << "\n";
    std::string sh = uses_shellcode ? "True" : "False";
    std::cout << "Shellcode: " << sh << "\n";
    std::cout << "Name: " << name << "\n";

    std::cout << "Args:\n";
    for (auto &arg : args) {
      std::cout << "  " << arg.type << "\n";
      std::cout << "  " << arg.name << "\n";
    }

    std::cout << "Code:\n";
    for (auto &i : code) {
      std::cout << "  " << occult::opcode_to_string(i.op) << " ";
      std::visit(visitor_stack(), i.operand);

      if (!i.type.empty()) {
        std::cout << "\t (type = " << i.type << ")\n";
      }
    }
    std::cout << "\n";
  }
}

void ir_gen::visualize_structs(const std::vector<ir_struct> &structs) {
  std::cout << CYAN << "Struct(s): \n" << RESET;
  for (auto &s : structs) {
    std::cout << "Type: " << s.datatype << "\n";

    std::cout << "Members: \n";
    for (auto &m : s.members) {
      std::cout << "  " << m.datatype << "\n";
      std::cout << "  " << m.name << "\n";
    }
    std::cout << "\n";
  }
}

std::vector<ir_function> ir_gen::lower_functions() {
  try {
    std::vector<ir_function> functions;

    for (const auto &c : root->get_children()) {
      if (const auto type = c->get_type(); type == cst_type::function) {
        functions.emplace_back(
            generate_function(cst::cast_raw<cst_function>(c.get())));
      }
    }

    for (auto &func : functions) {
      // fix unary ordering
      for (std::size_t i = 0; i < func.code.size(); i++) {
        switch (func.code.at(i).op) {
        case ir_opcode::op_bitwise_not:
        case ir_opcode::op_not:
        case ir_opcode::op_negate: {
          if (func.code.at(i + 1).op == ir_opcode::op_load ||
              func.code.at(i + 1).op == ir_opcode::op_push) {
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
  } catch (std::exception &e) {
    std::cout << RED << e.what() << RESET << std::endl;
    return std::vector<ir_function>();
  }
}

std::vector<ir_struct> ir_gen::lower_structs() {
  try {
    std::vector<ir_struct> structs;

    auto handle_struct = [](const std::string &name, cst *child) -> ir_struct {
      ir_struct ir_s;

      ir_s.datatype = name;

      for (const auto &c : child->get_children()) {
        switch (c->get_type()) {
        case cst_type::int8_datatype: {
          ir_s.members.emplace_back("int8", c->get_children().front()->content);

          break;
        }
        case cst_type::int16_datatype: {
          ir_s.members.emplace_back("int16",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::int32_datatype: {
          ir_s.members.emplace_back("int32",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::int64_datatype: {
          ir_s.members.emplace_back("int64",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::uint8_datatype: {
          ir_s.members.emplace_back("uint8",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::uint16_datatype: {
          ir_s.members.emplace_back("uint16",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::uint32_datatype: {
          ir_s.members.emplace_back("uint32",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::uint64_datatype: {
          ir_s.members.emplace_back("uint64",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::float32_datatype: {
          ir_s.members.emplace_back("float32",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::float64_datatype: {
          ir_s.members.emplace_back("float64",
                                    c->get_children().front()->content);

          break;
        }
        case cst_type::string_datatype: {
          ir_s.members.emplace_back("str", c->get_children().front()->content);

          break;
        }
        default: {
          if (c->get_children().size() == 1) {
            if (ir_s.datatype != c->content) {
              if (c->num_pointers > 0) {
                ir_s.members.emplace_back(c->content += "_stuct_ptr",
                                          c->get_children().front()->content);
              } else {
                ir_s.members.emplace_back(c->content,
                                          c->get_children().front()->content);
              }
            } else {
              if (c->num_pointers > 0) {
                ir_s.members.emplace_back(c->content += "_stuct_ptr",
                                          c->get_children().front()->content);
              } else {
                throw std::runtime_error("[IR ERR] Struct can't have nested "
                                         "type without ptr (sz = " +
                                         std::to_string(c->num_pointers) + ")");
              }
            }
          }

          break;
        }
        }
      }

      return ir_s;
    };

    for (auto &[name, child] : custom_type_map) {
      structs.emplace_back(handle_struct(name, child));
    }

    return structs;
  } catch (std::exception &e) {
    std::cout << RED << e.what() << RESET << std::endl;
    return std::vector<ir_struct>();
  }
}
} // namespace occult
