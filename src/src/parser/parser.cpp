#include "parser.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "../lexer/number_parser.hpp"
#include "error.hpp"
#include "parser_maps.hpp"

namespace occult {
    parser::parser(const std::vector<token_t>& stream, const std::string& source_file_path, const std::string& source) : root(cst::new_node<cst_root>()), stream(stream), source_file_path(source_file_path) {
        if (!source.empty()) {
            std::istringstream ss(source);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                source_lines.push_back(std::move(line));
            }
        }
    }

    token_t parser::peek(const std::uintptr_t _pos) {
        if (this->pos + _pos >= stream.size()) {
            return token_t(0, 0, "<EOF>", end_of_file_tt);
        }
        return stream[this->pos + _pos];
    }

    token_t parser::previous() {
        if ((pos - 1) != 0) {
            return stream[pos - 1];
        }
        else {
            throw std::runtime_error("Out of bounds parser::previous");
        }
    }

    void parser::consume(const std::uintptr_t amt) { pos += amt; }

    bool parser::match(const token_t& t, const token_type tt) {
        if (t.tt == tt) {
            return true;
        }
        else {
            return false;
        }
    }

    void parser::parse_function_call_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, const std::vector<token_t>& expr_ref, const token_t& curr_tok_ref, std::size_t& i_ref) {
        i_ref++;

        auto start_node = cst_map[function_call_parser_tt]("start_call"); // start call

        start_node->add_child(cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme)); // function name
        if (curr_tok_ref.lexeme == "error") {
            start_node->origin = source_file_path + ":" + std::to_string(curr_tok_ref.line);
        }

        auto paren_depth = 1;
        std::vector<token_t> current_args;

        while (i_ref + 1 < expr_ref.size() && 0 < paren_depth) {
            i_ref++;

            const auto& current_token = expr_ref.at(i_ref);

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

    std::unique_ptr<cst> parser::extend_access_chain_expr(std::unique_ptr<cst> base, const std::vector<token_t>& expr_ref, std::size_t& i_ref) {
        std::unique_ptr<cst> current = std::move(base);

        while (i_ref < expr_ref.size()
               && (expr_ref[i_ref].tt == left_bracket_tt
                   || expr_ref[i_ref].tt == period_tt
                   || expr_ref[i_ref].tt == left_paren_tt)) {
            if (expr_ref[i_ref].tt == left_paren_tt) {
                i_ref++; // consume '('
                int paren_depth = 1;
                std::vector<token_t> current_args;
                auto call = cst_map[function_call_parser_tt]("start_call");
                call->add_child(std::move(current));
                while (i_ref < expr_ref.size() && paren_depth > 0) {
                    const auto& tok = expr_ref.at(i_ref);
                    if (tok.tt == left_paren_tt) {
                        paren_depth++;
                        current_args.push_back(tok);
                    }
                    else if (tok.tt == right_paren_tt) {
                        paren_depth--;
                        if (paren_depth == 0) {
                            if (!current_args.empty()) {
                                auto arg_node = cst::new_node<cst_functionarg>();
                                auto parsed = parse_expression(current_args);
                                for (auto& c : parsed) {
                                    arg_node->add_child(std::move(c));
                                }
                                call->add_child(std::move(arg_node));
                                current_args.clear();
                            }
                            break;
                        }
                        current_args.push_back(tok);
                    }
                    else if (tok.tt == comma_tt && paren_depth == 1) {
                        auto arg_node = cst::new_node<cst_functionarg>();
                        auto parsed = parse_expression(current_args);
                        for (auto& c : parsed) {
                            arg_node->add_child(std::move(c));
                        }
                        call->add_child(std::move(arg_node));
                        current_args.clear();
                    }
                    else {
                        current_args.push_back(tok);
                    }
                    i_ref++;
                }
                call->add_child(cst_map[function_call_parser_tt]("end_call"));
                current = std::move(call);
                i_ref++; // past the matching ')'
                continue;
            }
            if (expr_ref[i_ref].tt == left_bracket_tt) {
                i_ref++; // consume '['

                int bracket_depth = 1;
                std::vector<token_t> index_tokens;

                while (i_ref < expr_ref.size() && bracket_depth > 0) {
                    if (auto& tok = expr_ref.at(i_ref); tok.tt == left_bracket_tt) {
                        bracket_depth++;
                        index_tokens.push_back(tok);
                    }
                    else if (tok.tt == right_bracket_tt) {
                        bracket_depth--;
                        if (bracket_depth == 0) {
                            break;
                        }
                        index_tokens.push_back(tok);
                    }
                    else {
                        index_tokens.push_back(tok);
                    }
                    i_ref++;
                }

                if (bracket_depth != 0) {
                    throw parsing_error("unmatched [ in array access", expr_ref.at(i_ref), pos, std::source_location::current().function_name());
                }

                auto arr = cst::new_node<cst_arrayaccess>();
                arr->add_child(std::move(current));
                if (!index_tokens.empty()) {
                    for (auto index_nodes = parse_expression(index_tokens); auto& n : index_nodes) {
                        arr->add_child(std::move(n));
                    }
                }
                current = std::move(arr);

                i_ref++; // past ']'
            }
            else { // period
                i_ref++; // consume '.'
                if (i_ref >= expr_ref.size() || expr_ref[i_ref].tt != identifier_tt) {
                    throw parsing_error("identifier after period", i_ref < expr_ref.size() ? expr_ref[i_ref] : expr_ref.back(), pos, std::source_location::current().function_name());
                }
                auto field = cst_map[identifier_tt](expr_ref[i_ref].lexeme);
                if (current->get_type() == cst_type::memberaccess
                    && current->get_children().front()->get_type() == cst_type::identifier) {
                    current->add_child(std::move(field));
                }
                else {
                    auto mem = cst::new_node<cst_memberaccess>();
                    mem->add_child(std::move(current));
                    mem->add_child(std::move(field));
                    current = std::move(mem);
                }
                i_ref++; // past the field identifier
            }
        }

        // dispatcher's for-loop does i++, so land on the last token of the chain.
        if (i_ref > 0) i_ref--;
        return current;
    }

    void parser::parse_access_chain_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, const std::vector<token_t>& expr_ref, const token_t& curr_tok_ref, std::size_t& i_ref) {
        std::unique_ptr<cst> current = cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme);
        i_ref++; // past the identifier
        expr_cst_ref.push_back(extend_access_chain_expr(std::move(current), expr_ref, i_ref));
    }

    void parser::shunting_yard(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref, const token_t& curr_tok_ref) const {
        switch (curr_tok_ref.tt) {
        case number_literal_tt:
        case float_literal_tt:
        case string_literal_tt:
        case char_literal_tt:
        case false_keyword_tt:
        case true_keyword_tt:
        case identifier_tt:
            {
                auto n = cst_map[curr_tok_ref.tt](curr_tok_ref.lexeme);

                expr_cst_ref.push_back(std::move(n));

                break;
            }
        case unary_bitwise_not_tt:
        case unary_minus_operator_tt:
        case unary_plus_operator_tt:
        case unary_not_operator_tt:
            {
                stack_ref.push(curr_tok_ref); // push unary ops to stack

                break;
            }
        case left_paren_tt:
            {
                stack_ref.push(curr_tok_ref);
                expr_cst_ref.push_back(cst::new_node<cst_expr_end>());

                break;
            }
        case right_paren_tt:
            {
                while (!stack_ref.empty() && stack_ref.top().tt != left_paren_tt) {
                    auto it = cst_map.find(stack_ref.top().tt);
                    if (it == cst_map.end() || !it->second) {
                        throw parsing_error("valid operator or operand (possibly a missing ';' "
                                            "before this token)",
                                            stack_ref.top(), pos, std::source_location::current().function_name());
                    }
                    auto n = it->second(stack_ref.top().lexeme);

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
                       (stack_ref.top().tt == unary_bitwise_not_tt || stack_ref.top().tt == unary_minus_operator_tt || stack_ref.top().tt == unary_plus_operator_tt || stack_ref.top().tt == unary_not_operator_tt ||
                        stack_ref.top().tt == dereference_operator_tt)) {
                    expr_cst_ref.push_back(cst_map[stack_ref.top().tt](stack_ref.top().lexeme));
                    stack_ref.pop();
                }

                expr_cst_ref.push_back(cst::new_node<cst_expr_start>());

                break;
            }
        default:
            { // binary ops
                while (!stack_ref.empty() && stack_ref.top().tt != left_paren_tt &&
                       (curr_tok_ref.tt == assignment_tt ? precedence_map[curr_tok_ref.tt] > precedence_map[stack_ref.top().tt] : precedence_map[curr_tok_ref.tt] >= precedence_map[stack_ref.top().tt])) {
                    auto it = cst_map.find(stack_ref.top().tt);
                    if (it == cst_map.end() || !it->second) {
                        throw parsing_error("valid operator or operand (possibly a missing ';' "
                                            "before this token)",
                                            stack_ref.top(), pos, std::source_location::current().function_name());
                    }
                    auto n = it->second(stack_ref.top().lexeme);

                    expr_cst_ref.push_back(std::move(n));
                    stack_ref.pop();
                }
                stack_ref.push(curr_tok_ref);

                break;
            }
        }
    }

    void parser::shunting_yard_stack_cleanup(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref) const {
        while (!stack_ref.empty()) {
            if (stack_ref.top().tt == left_paren_tt) {
                throw parsing_error("unmatched left parenthesis", stack_ref.top(), pos, std::source_location::current().function_name());
            }
            if (stack_ref.top().tt == right_curly_bracket_tt) {
                throw parsing_error("unmatched right curly bracket", stack_ref.top(), pos, std::source_location::current().function_name());
            }

            auto it = cst_map.find(stack_ref.top().tt);
            if (it == cst_map.end() || !it->second) {
                throw parsing_error("valid operator or operand (possibly a missing ';' "
                                    "before this token)",
                                    stack_ref.top(), pos, std::source_location::current().function_name());
            }

            expr_cst_ref.push_back(cst_map[stack_ref.top().tt](stack_ref.top().lexeme));
            stack_ref.pop();
        }
    }

    bool parser::is_castable(token_t t) {
        switch (t.tt) {
        case int64_keyword_tt:
        case int32_keyword_tt:
        case int16_keyword_tt:
        case int8_keyword_tt:
        case uint64_keyword_tt:
        case uint32_keyword_tt:
        case uint16_keyword_tt:
        case uint8_keyword_tt:
        case float32_keyword_tt:
        case float64_keyword_tt:
        case char_keyword_tt:
            {
                return true;
            }
        default:
            {
                return false;
            }
        }
    }

    std::vector<std::unique_ptr<cst>> parser::parse_expression(const std::vector<token_t>& raw_expr) {
        auto expr = preprocess_generic_calls(raw_expr);
        std::vector<std::unique_ptr<cst>> expr_cst;
        std::stack<token_t> operator_stack;

        for (std::size_t i = 0; i < expr.size(); i++) {
            const auto& t = expr.at(i);

            if (t.tt == identifier_tt && type_aliases.contains(t.lexeme)) {
                std::size_t look = i + 1;
                while (look < expr.size() && expr.at(look).tt == multiply_operator_tt) ++look;
                if (look < expr.size() && expr.at(look).tt == left_paren_tt) {
                    const auto tokens = type_aliases[t.lexeme];
                    expr.erase(expr.begin() + i);
                    expr.insert(expr.begin() + i, tokens.begin(), tokens.end());
                    --i;
                    continue;
                }
            }

            if (t.tt == semicolon_tt || t.tt == left_curly_bracket_tt || t.tt == comma_tt) {
                break; // end of expr
            }

            if (t.tt == left_paren_tt) {
                // skip empty parens -- those are a call, not a group
                int depth = 1;
                std::size_t close = i + 1;
                while (close < expr.size() && depth > 0) {
                    if (expr.at(close).tt == left_paren_tt) depth++;
                    else if (expr.at(close).tt == right_paren_tt) depth--;
                    if (depth == 0) break;
                    close++;
                }
                if (close < expr.size() && depth == 0
                    && close + 1 < expr.size()
                    && (expr.at(close + 1).tt == period_tt || expr.at(close + 1).tt == left_bracket_tt)
                    && close > i + 1 /* non-empty */) {
                    std::vector<token_t> inner(expr.begin() + i + 1, expr.begin() + close);
                    auto inner_nodes = parse_expression(inner);
                    if (inner_nodes.size() == 1) {
                        std::size_t ci = close + 1;
                        auto chained = extend_access_chain_expr(std::move(inner_nodes.front()), expr, ci);
                        expr_cst.push_back(std::move(chained));
                        i = ci; // land on the last token of the chain; loop i++ moves past it
                        continue;
                    }
                }
            }

            if (t.tt == reference_operator_tt) {
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

                // check if this is $var.member (dereference + member access)
                // if so, handle as a single member access node with deref flag
                if (i + 1 < expr.size() && expr.at(i + 1).tt == identifier_tt && i + 2 < expr.size() && expr.at(i + 2).tt == period_tt) {
                    i++; // advance to identifier
                    auto member_access_node = cst::new_node<cst_memberaccess>();
                    member_access_node->num_pointers = count; // signal dereference

                    // add base identifier with deref info
                    auto base_id = cst::new_node<cst_identifier>();
                    base_id->content = expr.at(i).lexeme;
                    base_id->num_pointers = count;
                    member_access_node->add_child(std::move(base_id));

                    i++; // advance past identifier

                    // parse member chain
                    while (i < expr.size() && expr.at(i).tt == period_tt) {
                        i++; // consume period
                        if (i >= expr.size() || expr.at(i).tt != identifier_tt) {
                            break;
                        }
                        member_access_node->add_child(cst_map[identifier_tt](expr.at(i).lexeme));
                        i++;
                    }

                    i--; // backtrack for outer loop increment

                    expr_cst.push_back(std::move(member_access_node));
                    continue;
                }

                operator_stack.push(token_t(t.line, t.column, std::to_string(count), dereference_operator_tt));

                continue;
            }
            /*else if (t.tt == reference_operator_tt || t.tt == dereference_operator_tt)
            { expr_cst.push_back(cst_map[t.tt](t.lexeme));
            }*/
            else if (t.tt == identifier_tt) {
                if (i + 1 < expr.size() && expr.at(i + 1).tt == scope_resolution_tt) {
                    std::string full_path = t.lexeme;
                    i += 2; // skip first identifier and '::'

                    while (i < expr.size() && expr.at(i).tt == identifier_tt) {
                        full_path += "::" + expr.at(i).lexeme;
                        if (i + 1 < expr.size() && expr.at(i + 1).tt == scope_resolution_tt) {
                            i += 2; // skip identifier and '::'
                        }
                        else {
                            break;
                        }
                    }

                    auto try_expand_import = [&](const std::string& path) -> std::string {
                        auto first_sep = path.find("::");
                        std::string first_seg = (first_sep != std::string::npos) ? path.substr(0, first_sep) : path;
                        for (const auto& imp : imported_modules) {
                            auto imp_last_sep = imp.rfind("::");
                            std::string imp_last = (imp_last_sep != std::string::npos) ? imp.substr(imp_last_sep + 2) : imp;
                            if (imp_last == first_seg) {
                                if (first_sep != std::string::npos) {
                                    return imp + path.substr(first_sep);
                                }
                                else {
                                    return imp;
                                }
                            }
                        }
                        return path;
                    };

                    std::string resolved_path = try_expand_import(full_path);
                    std::vector<std::string> scoped_candidates = {resolved_path};
                    if (!module_prefix_stack.empty()) {
                        std::string accum;
                        for (const auto& seg : module_prefix_stack) {
                            accum += seg + "::";
                            scoped_candidates.push_back(accum + resolved_path);
                        }
                    }

                    bool resolved_enum_member = false;
                    for (auto it = scoped_candidates.rbegin(); it != scoped_candidates.rend(); ++it) {
                        auto last_sep = it->rfind("::");
                        if (last_sep == std::string::npos) {
                            continue;
                        }
                        std::string potential_enum = it->substr(0, last_sep);
                        std::string potential_member = it->substr(last_sep + 2);
                        auto enum_it = enum_definitions.find(potential_enum);
                        if (enum_it != enum_definitions.end() && enum_it->second.contains(potential_member)) {
                            auto value = enum_it->second.at(potential_member);
                            expr_cst.push_back(cst::new_node<cst_numberliteral>(std::to_string(value)));
                            resolved_enum_member = true;
                            break;
                        }
                    }

                    if (!resolved_enum_member) {
                        std::string resolved_for_use = resolved_path;
                        if (!module_prefix_stack.empty()) {
                            for (auto it = scoped_candidates.rbegin(); it != scoped_candidates.rend(); ++it) {
                                if (generic_func_templates.contains(*it)) {
                                    resolved_for_use = *it;
                                    break;
                                }
                            }
                        }

                        if (i + 1 < expr.size() && expr.at(i + 1).tt == left_paren_tt) {
                            token_t qualified_tok = t;
                            qualified_tok.lexeme = resolved_for_use;
                            parse_function_call_expr(expr_cst, expr, qualified_tok, i);
                            if (i + 1 < expr.size()
                                && (expr.at(i + 1).tt == period_tt
                                    || expr.at(i + 1).tt == left_bracket_tt
                                    || expr.at(i + 1).tt == left_paren_tt)) {
                                std::unique_ptr<cst> call_node = std::move(expr_cst.back());
                                expr_cst.pop_back();
                                i++;
                                expr_cst.push_back(extend_access_chain_expr(std::move(call_node), expr, i));
                            }
                        }
                        else {
                            expr_cst.push_back(cst::new_node<cst_identifier>(resolved_for_use));
                        }
                    }
                }
                else if (i + 1 < expr.size() && expr.at(i + 1).tt == left_paren_tt) {
                    parse_function_call_expr(expr_cst, expr, t, i);
                    if (i + 1 < expr.size()
                        && (expr.at(i + 1).tt == period_tt
                            || expr.at(i + 1).tt == left_bracket_tt
                            || expr.at(i + 1).tt == left_paren_tt)) {
                        std::unique_ptr<cst> call_node = std::move(expr_cst.back());
                        expr_cst.pop_back();
                        i++;
                        expr_cst.push_back(extend_access_chain_expr(std::move(call_node), expr, i));
                    }
                }
                else if (i + 3 < expr.size() && expr.at(i + 1).tt == period_tt && expr.at(i + 2).tt == identifier_tt && expr.at(i + 3).tt == left_paren_tt) {
                    auto start_node = cst_map[function_call_parser_tt]("start_call");

                    std::string rewritten_method_name = "." + expr.at(i + 2).lexeme;
                    start_node->add_child(cst_map[identifier_tt](rewritten_method_name));

                    auto self_arg = cst::new_node<cst_functionarg>();
                    self_arg->add_child(cst_map[identifier_tt](t.lexeme));
                    start_node->add_child(std::move(self_arg));

                    i += 3; // at '('
                    auto paren_depth = 1;
                    std::vector<token_t> current_args;

                    while (i + 1 < expr.size() && 0 < paren_depth) {
                        i++;

                        const auto& current_token = expr.at(i);

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

                    start_node->add_child(cst_map[function_call_parser_tt]("end_call"));
                    expr_cst.push_back(std::move(start_node));
                    if (i + 1 < expr.size()
                        && (expr.at(i + 1).tt == period_tt
                            || expr.at(i + 1).tt == left_bracket_tt
                            || expr.at(i + 1).tt == left_paren_tt)) {
                        std::unique_ptr<cst> call_node = std::move(expr_cst.back());
                        expr_cst.pop_back();
                        i++;
                        expr_cst.push_back(extend_access_chain_expr(std::move(call_node), expr, i));
                    }
                }
                else if (i + 1 < expr.size() && (expr.at(i + 1).tt == left_bracket_tt || expr.at(i + 1).tt == period_tt)) {
                    parse_access_chain_expr(expr_cst, expr, t, i);
                }
                else {
                    expr_cst.push_back(cst_map[t.tt](t.lexeme));
                }
            }
            else if (t.tt == sizeof_keyword_tt) {
                if (i + 1 >= expr.size() || expr.at(i + 1).tt != left_paren_tt) {
                    throw parsing_error("'(' after sizeof", t, pos, std::source_location::current().function_name());
                }

                static const std::unordered_map<token_type, std::string> prim = {
                    {int8_keyword_tt, "int8"},   {int16_keyword_tt, "int16"},   {int32_keyword_tt, "int32"},     {int64_keyword_tt, "int64"}, {uint8_keyword_tt, "uint8"},
                    {uint16_keyword_tt, "uint16"}, {uint32_keyword_tt, "uint32"}, {uint64_keyword_tt, "uint64"}, {float32_keyword_tt, "float32"}, {float64_keyword_tt, "float64"},
                    {boolean_keyword_tt, "bool"}, {char_keyword_tt, "int8"},    {string_keyword_tt, "string"}};

                std::string type_name;
                i += 2; // skip 'sizeof' and '('
                for (; i < expr.size() && expr.at(i).tt != right_paren_tt; ++i) {
                    const auto& tok = expr.at(i);
                    if (auto p_it = prim.find(tok.tt); p_it != prim.end()) {
                        type_name += p_it->second;
                    }
                    else if (tok.tt == identifier_tt || tok.tt == scope_resolution_tt) {
                        type_name += tok.lexeme;
                    }
                    else if (tok.tt == multiply_operator_tt) {
                        type_name += '*';
                    }
                    else {
                        throw parsing_error("<datatype> inside sizeof", tok, pos, std::source_location::current().function_name());
                    }
                }
                if (i >= expr.size() || type_name.empty()) {
                    throw parsing_error("sizeof(<datatype>)", t, pos, std::source_location::current().function_name());
                }

                expr_cst.push_back(cst::new_node<cst_sizeof_expr>(type_name));
            }
            else if (is_castable(t)) {
                std::size_t type_i = i; // remember where the type keyword is

                // a pointer cast: i64*(x), i64**(x), and so on
                std::size_t stars = 0;
                std::size_t after_type = i + 1;
                while (after_type < expr.size() && expr.at(after_type).tt == multiply_operator_tt) {
                    stars++;
                    after_type++;
                }

                if (after_type < expr.size() && expr.at(after_type).tt == left_paren_tt) {
                    i = after_type + 1; // skip past the type, any '*' and '('

                    std::vector<token_t> arg_tokens;
                    int paren_depth = 1;

                    while (i < expr.size() && paren_depth > 0) {
                        const auto& tok = expr.at(i);

                        if (tok.tt == left_paren_tt) {
                            paren_depth++;
                        }
                        else if (tok.tt == right_paren_tt) {
                            paren_depth--;
                            if (paren_depth == 0) {
                                break; // don't include the closing ')'
                            }
                        }

                        if (paren_depth > 0) {
                            arg_tokens.push_back(tok);
                        }

                        i++;
                    }

                    if (paren_depth != 0) {
                        throw parsing_error("unmatched '(' in cast expression", expr.at(type_i), pos, std::source_location::current().function_name());
                    }

                    // parse as a normal expression
                    auto inner_expr = parse_expression(arg_tokens);

                    if (inner_expr.empty()) {
                        throw parsing_error("cast expects an expression in parentheses", expr.at(type_i), pos, std::source_location::current().function_name());
                    }

                    auto cast_node = cst::new_node<cst_cast_to_datatype>(expr.at(type_i).lexeme);
                    cast_node->num_pointers = stars;
                    for (auto& child : inner_expr) {
                        cast_node->add_child(std::move(child));
                    }

                    expr_cst.push_back(std::move(cast_node));
                }
                else {
                    throw parsing_error("a cast needs parentheses, as in i64(x) or i64*(x)", t, pos, std::source_location::current().function_name());
                }
            }
            else {
                shunting_yard(operator_stack, expr_cst, t);
            }
        }

        shunting_yard_stack_cleanup(operator_stack, expr_cst);

        return expr_cst;
    }

    // canonical type name of a parsed type node, as used in fn-pointer signatures
    static std::string fnptr_type_name(cst* n) {
        if (!n->fnptr_sig.empty()) {
            return n->fnptr_sig;
        }
        std::string t;
        if (n->get_type() == cst_type::structure || n->get_type() == cst_type::generic_type) {
            t = n->content;
        }
        else {
            t = n->to_string().substr(4);
        }
        for (std::size_t i = 0; i < n->num_pointers; i++) {
            t += "_ptr";
        }
        return t;
    }

    std::unique_ptr<cst> parser::parse_fnptr_type() {
        consume(); // fn
        if (!match(peek(), left_paren_tt)) {
            throw parsing_error("( in function pointer type", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        std::string sig = "fn(";
        bool first = true;
        while (!match(peek(), right_paren_tt)) {
            auto param = parse_datatype(); // an optional parameter name is allowed and ignored
            if (!param) {
                throw parsing_error("parameter type in function pointer type", peek(), pos, std::source_location::current().function_name());
            }
            if (!first) {
                sig += ",";
            }
            first = false;
            sig += fnptr_type_name(param.get());

            if (match(peek(), comma_tt)) {
                consume();
            }
            else if (!match(peek(), right_paren_tt)) {
                throw parsing_error(", or ) in function pointer type", peek(), pos, std::source_location::current().function_name());
            }
        }
        consume(); // )
        sig += ")";

        auto node = cst::new_node<cst_int64>(); // an fn pointer is an 8-byte code address
        node->content = "fnptr";

        // optional return type; parse_datatype absorbs a following name, which
        // then belongs to this declaration rather than to the return type
        if (auto ret = parse_datatype(); ret) {
            std::unique_ptr<cst> absorbed;
            if (!ret->get_children().empty() && ret->get_children().front()->get_type() == cst_type::identifier) {
                absorbed = std::move(ret->get_children().front());
                ret->get_children().erase(ret->get_children().begin());
            }
            sig += fnptr_type_name(ret.get());
            if (absorbed) {
                node->add_child(std::move(absorbed));
            }
        }
        else {
            sig += "int64"; // functions without a return type return i64
        }

        node->fnptr_sig = sig;

        if (node->get_children().empty() && match(peek(), identifier_tt)) {
            node->add_child(parse_identifier());
        }

        return node;
    }

    std::unique_ptr<cst> parser::parse_fnptr_decl() {
        auto node = parse_fnptr_type();

        if (node->get_children().empty()) {
            throw parsing_error("<identifier> after function pointer type", peek(), pos, std::source_location::current().function_name());
        }

        if (match(peek(), assignment_tt)) {
            node->add_child(parse_assignment());

            if (match(peek(), semicolon_tt)) {
                throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
            }

            parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
        }

        if (match(peek(), semicolon_tt)) {
            consume();
        }
        else {
            throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return node;
    }

    std::unique_ptr<cst> parser::parse_datatype() {
        // `const` as a prefix on a parameter / extern param / struct field is
        // accepted and ignored. The keyword's local-variable enforcement
        // check still applies on assignments to the named slot.
        while (match(peek(), const_keyword_tt)) {
            consume();
        }

        if (match(peek(), identifier_tt)) {
            if (auto it = type_aliases.find(peek().lexeme); it != type_aliases.end()) {
                if (type_alias_depth > 32) {
                    throw parsing_error("non-cyclic type alias", peek(), pos, std::source_location::current().function_name());
                }
                const auto tokens = it->second;
                stream.erase(stream.begin() + pos);
                stream.insert(stream.begin() + pos, tokens.begin(), tokens.end());
                ++type_alias_depth;
                auto r = parse_datatype();
                --type_alias_depth;
                return r;
            }
        }

        if (match(peek(), function_keyword_tt) && match(peek(1), left_paren_tt)) {
            return parse_fnptr_type();
        }

        if (const auto it = datatype_map.find(peek().tt); it != datatype_map.end()) {
            consume();
            auto node = it->second();

            if (match(peek(), reference_operator_tt)) {
                consume();
                node->content = "reference";
            }

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }

            return node;
        }

        if (match(peek(), identifier_tt) && pos + 2 < stream.size() && match(peek(1), scope_resolution_tt) && match(peek(2), identifier_tt)) {
            std::string module_name = peek().lexeme;
            std::string qualified_name = module_name + "::" + peek(2).lexeme;
            std::string type_name = peek(2).lexeme;

            if (pos + 3 < stream.size() && match(peek(3), less_than_operator_tt) && (generic_struct_templates.contains(qualified_name) || generic_struct_templates.contains(type_name))) {
                std::string template_name = generic_struct_templates.contains(qualified_name) ? qualified_name : type_name;
                consume(); // consume module name
                consume(); // consume '::'
                consume(); // consume type name
                consume(); // consume '<'

                auto type_args = parse_type_argument_tokens();
                const bool has_generic_param = type_args_have_generic_params(type_args);

                std::string struct_name = template_name;
                if (!has_generic_param) {
                    struct_name = instantiate_generic_struct(template_name, type_args);
                }

                auto node = cst::new_node<cst_struct>();
                node->content = struct_name;

                if (match(peek(), reference_operator_tt)) {
                    consume();
                    node->is_reference = true;
                }

                if (match(peek(), multiply_operator_tt)) {
                    while (match(peek(), multiply_operator_tt)) {
                        consume();
                        node->num_pointers++;
                    }
                }

                if (match(peek(), identifier_tt)) {
                    node->add_child(parse_identifier());
                }

                return node;
            }

            if (custom_type_map.contains(qualified_name)) {
                consume(); // consume module name
                consume(); // consume '::'
                consume(); // consume type name

                auto node = cst::new_node<cst_struct>();
                node->content = qualified_name;

                if (match(peek(), reference_operator_tt)) {
                    consume();
                    node->is_reference = true;
                }

                if (match(peek(), multiply_operator_tt)) {
                    while (match(peek(), multiply_operator_tt)) {
                        consume();
                        node->num_pointers++;
                    }
                }

                if (match(peek(), identifier_tt)) {
                    node->add_child(parse_identifier());
                }

                return node;
            }
        }

        if (match(peek(), identifier_tt) && generic_struct_templates.contains(peek().lexeme) && pos + 1 < stream.size() && match(peek(1), less_than_operator_tt)) {
            std::string template_name = peek().lexeme;
            consume(); // consume template name
            consume(); // consume '<'

            auto type_args = parse_type_argument_tokens();
            const bool has_generic_param = type_args_have_generic_params(type_args);

            std::string struct_name = template_name;
            if (!has_generic_param) {
                struct_name = instantiate_generic_struct(template_name, type_args);
            }

            auto node = cst::new_node<cst_struct>();
            node->content = struct_name;

            if (match(peek(), reference_operator_tt)) {
                consume();
                node->is_reference = true;
            }

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }

            return node;
        }

        {
            std::unique_ptr<cst> fwd_node;
            if (try_parse_forward_generic_struct_type(fwd_node)) {
                if (match(peek(), identifier_tt)) {
                    fwd_node->add_child(parse_identifier());
                }
                return fwd_node;
            }
        }

        if (match(peek(), identifier_tt) && custom_type_map.contains(peek().lexeme)) {
            const auto type_name = peek().lexeme;
            consume();

            auto node = cst::new_node<cst_struct>();
            node->content = type_name;

            if (match(peek(), reference_operator_tt)) {
                consume();
                node->is_reference = true;
            }

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }

            return node;
        }

        if (match(peek(), identifier_tt) && cst_generic_type_cache.contains(peek().lexeme)) {
            auto node = cst::new_node<cst_generic_type>(peek().lexeme);
            consume();

            if (match(peek(), reference_operator_tt)) {
                consume();
                node->is_reference = true;
            }

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }

            return node;
        }

        if (match(peek(), array_keyword_tt)) {
            throw parsing_error("scalar return type (array types cannot be used as a "
                                "function return type)",
                                peek(), pos, std::source_location::current().function_name());
        }

        return nullptr;
    }

    std::unique_ptr<cst_identifier> parser::parse_identifier() {
        consume();

        auto node = cst::new_node<cst_identifier>();

        node->content = previous().lexeme;

        return node;
    }

    std::unique_ptr<cst_function> parser::parse_function(bool is_external) {
        auto func_node = cst::new_node<cst_function>();

        consume(); // consume function keyword

        auto name = parse_identifier();
        // `fn Type::method(...)` -- a method declaration qualified by its
        // owning struct name. parse_function_call_expr already builds the
        // same "Type::method" lookup key for obj.method(obj) calls, so a
        // qualified decl here puts the function in func_map under the key
        // the call site asks for. multi-segment paths are allowed too:
        // `fn Mod::Type::method(...)` builds "Mod::Type::method".
        while (match(peek(), scope_resolution_tt)) {
            consume();
            if (!match(peek(), identifier_tt)) {
                throw parsing_error("identifier after '::' in function name", peek(), pos, std::source_location::current().function_name());
            }
            auto next = parse_identifier();
            name->content += "::" + next->content;
        }

        func_node->add_child(std::move(name));

        if (match(peek(), left_paren_tt)) {
            consume();

            auto func_args_node = cst::new_node<cst_functionargs>();

            auto arg_count = 0;
            while (!match(peek(), right_paren_tt)) {
                if (match(peek(), variadic_tt)) {
                    consume();

                    int existing_args = static_cast<int>(func_args_node->get_children().size());
                    int max_total_args = existing_args + 16; // support up to 16 variadic args
                    int synthetic_count = max_total_args - existing_args;

                    for (int va_idx = 0; va_idx < synthetic_count; va_idx++) {
                        auto arg = cst::new_node<cst_int64>();
                        arg->add_child(cst::new_node<cst_identifier>("__va" + std::to_string(va_idx)));
                        func_args_node->add_child(std::move(arg));
                    }

                    func_args_node->add_child(cst::new_node<cst_variadic>());

                    break;
                }

                auto arg = parse_datatype(); // now handles generics

                if (!arg) {
                    throw parsing_error("valid type in function argument", peek(), pos, std::source_location::current().function_name());
                }

                if (arg->get_children().size() < 1) { // for shellcode
                    arg->add_child(cst::new_node<cst_identifier>("arg" + std::to_string(++arg_count)));
                }

                func_args_node->add_child(std::move(arg));

                if (match(peek(), comma_tt)) {
                    consume();
                }
            }

            if (!match(peek(), right_paren_tt)) {
                throw parsing_error(")", peek(), pos, std::source_location::current().function_name());
            }

            consume();

            func_node->add_child(std::move(func_args_node));
        }
        else {
            throw parsing_error("(", peek(), pos, std::source_location::current().function_name());
        }

        bool uses_shellcode = false;
        bool uses_assembly = false;

        std::string body_arch = "x86_64";

        if (match(peek(), asm_keyword_tt)) {
            consume();
            body_arch = parse_asm_arch();

            func_node->add_child(cst::new_node<cst_func_uses_asm>());
            uses_assembly = true;
        }

        if (match(peek(), shellcode_denoter_tt)) {
            consume();
            body_arch = parse_asm_arch();

            func_node->add_child(cst::new_node<cst_func_uses_shellcode>());
            uses_shellcode = true;
        }

        if (auto return_type = parse_datatype(); return_type) {
            func_node->add_child(std::move(return_type));
        }
        else {
            func_node->add_child(cst::new_node<cst_int64>());
        }

        if (is_external) {
            if (uses_assembly || uses_shellcode) {
                throw parsing_error("no body on an extern declaration", peek(), pos, std::source_location::current().function_name());
            }
            func_node->add_child(cst::new_node<cst_func_is_external>());

            if (!match(peek(), semicolon_tt)) {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }
            consume();

            return func_node;
        }

        if (uses_shellcode) {
            func_node->add_child(parse_shellcode_bytes(body_arch));

            return func_node;
        }

        if (uses_assembly) {
            auto asm_node = cst::new_node<cst_asm_code>(body_arch);

            if (!match(peek(), left_curly_bracket_tt)) {
                throw parsing_error("{", peek(), pos, std::source_location::current().function_name());
            }
            consume();

            if (!match(peek(), asm_body_tt)) {
                throw parsing_error("assembly code", peek(), pos, std::source_location::current().function_name());
            }
            asm_node->add_child(cst::new_node<cst_stringliteral>(peek().lexeme)); // src
            consume();

            if (!match(peek(), right_curly_bracket_tt)) {
                throw parsing_error("}", peek(), pos, std::source_location::current().function_name());
            }
            consume();

            func_node->add_child(std::move(asm_node));

            return func_node;
        }

        auto body = parse_block();

        bool has_return = false;
        for (const auto& child : body->get_children()) {
            if (child->get_type() == cst_type::returnstmt)
                has_return = true;
        }

        if (!has_return) { // implicit return 0
            auto return_node = cst::new_node<cst_returnstmt>();
            return_node->add_child(cst::new_node<cst_numberliteral>("0"));

            body->add_child(std::move(return_node));
        }

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

            consume();

            return block_node;
        }
        throw parsing_error("{", peek(), pos, std::source_location::current().function_name());
    }

    std::unique_ptr<cst_assignment> parser::parse_assignment() {
        consume();

        auto node = cst::new_node<cst_assignment>();

        return node;
    }

    template <typename ParentNode>
    void parser::parse_expression_until(ParentNode* parent, const token_type t) {
        const auto first_pos = find_first_token(stream.begin() + pos, stream.end(), t);
        if (first_pos == -1) {
            return;
        }
        const std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_pos + 1};
        pos += first_pos;

        for (auto converted_rpn = parse_expression(sub_stream); auto& c : converted_rpn) {
            parent->add_child(std::move(c));
        }
    }

    template <typename IntegerCstType>
    std::unique_ptr<IntegerCstType> parser::parse_integer_type() {
        consume(); // consume keyword

        auto node = cst::new_node<IntegerCstType>();

        if (match(peek(), multiply_operator_tt)) {
            while (match(peek(), multiply_operator_tt)) {
                consume();
                ++node->num_pointers;
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
                throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
            }

            parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
            // parse the expression until the semicolon
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

        parse_expression_until(return_node.get(),
                               semicolon_tt); // parse the expression until the semicolon

        if (match(peek(), semicolon_tt)) {
            consume();
        }
        else {
            throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return return_node;
    }

    std::unique_ptr<cst_struct> parser::parse_custom_type() {
        const auto type_name = peek().lexeme;
        consume(); // consume identifier

        auto node = cst::new_node<cst_struct>();
        node->content = type_name;

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
                throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
            }

            parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
            // parse the expression until the semicolon
        }

        if (match(peek(), semicolon_tt)) { // end of declaration
            consume();
        }
        else {
            throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return node;
    }

    std::unique_ptr<cst_ifstmt> parser::parse_if() {
        consume(); // consume if

        auto if_node = cst::new_node<cst_ifstmt>();

        parse_expression_until(if_node.get(), left_curly_bracket_tt);
        // parse the expression until the left curly bracket

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

        parse_expression_until(elseif_node.get(), left_curly_bracket_tt);
        // parse the expression until the left curly bracket

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

    std::unique_ptr<cst_gotostmt> parser::parse_goto() {
        consume();

        auto goto_node = cst::new_node<cst_gotostmt>();

        if (match(peek(), identifier_tt)) {
            goto_node->content = peek().lexeme;
            consume();
        }
        else {
            throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
        }

        if (match(peek(), semicolon_tt)) {
            consume();
        }
        else {
            throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }

        return goto_node;
    }

    // somewhere: a label. the colon tells it from an assignment or a call.
    std::unique_ptr<cst_labelstmt> parser::parse_label() {
        auto label_node = cst::new_node<cst_labelstmt>();
        label_node->content = peek().lexeme;
        consume(); // the name
        consume(); // the colon

        return label_node;
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

        parse_expression_until(while_node.get(), left_curly_bracket_tt);
        // parse the expression until the left curly bracket

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
                throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
            }

            parse_expression_until(assignment.get(),
                                   semicolon_tt); // parse the expression until the semicolon

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

    std::unique_ptr<cst_forstmt> parser::parse_regular_for(std::unique_ptr<cst_forstmt> existing_for_node) {
        // for expr when condition do expr {}
        const auto when_pos = find_first_token(stream.begin() + pos, stream.end(), when_keyword_tt);
        stream.insert(stream.begin() + pos + when_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));

        existing_for_node->add_child(parse_keyword());

        if (match(peek(), when_keyword_tt)) {
            consume(); // consume when

            const auto do_pos = find_first_token(stream.begin() + pos, stream.end(), do_keyword_tt);
            stream.insert(stream.begin() + pos + do_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));
            std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + do_pos + 2};
            pos += do_pos + 1;
            auto converted_rpn = parse_expression(sub_stream);

            auto forcond_node = cst::new_node<cst_forcondition>();

            for (auto& c : converted_rpn) {
                forcond_node->add_child(std::move(c));
            }

            existing_for_node->add_child(std::move(forcond_node));

            if (match(peek(), do_keyword_tt)) {
                consume(); // consume do

                const auto left_curly_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
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

        throw parsing_error("when", peek(), pos, std::source_location::current().function_name());
    }

    std::unique_ptr<cst_forstmt> parser::parse_for() { // for expr; in expr; { }
        consume();

        auto for_node = cst::new_node<cst_forstmt>();

        if (find_first_token(stream.begin() + pos, stream.end(), when_keyword_tt) != -1) {
            return parse_regular_for(std::move(for_node));
        }

        const auto in_pos = find_first_token(stream.begin() + pos, stream.end(), in_keyword_tt);

        // we're going to insert a semicolon
        stream.insert(stream.begin() + pos + in_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));

        for_node->add_child(parse_keyword()); // first expr

        if (match(peek(), in_keyword_tt)) {
            consume();

            if (match(peek(), identifier_tt) && peek(1).tt != left_paren_tt) {
                for_node->add_child(parse_identifier());
            }
            else {
                const auto left_curly_bracket_pos = find_first_token(stream.begin() + pos, stream.end(), left_curly_bracket_tt);
                stream.insert(stream.begin() + pos + left_curly_bracket_pos, token_t(stream.at(pos).line, stream.at(pos).column + 1, ";", semicolon_tt));

                for_node->add_child(parse_keyword()); // 2nd expr
            }

            auto body = parse_block();
            for_node->add_child(std::move(body));

            return for_node;
        }

        throw parsing_error("in", peek(), pos, std::source_location::current().function_name());
    }

    std::unique_ptr<cst_array> parser::parse_array() { // array <dimensions> <datatype> <identifier> = { ... };
        consume();                                     // consume array

        auto node = cst::new_node<cst_array>();
        std::vector<std::size_t> dimensions;

        while (match(peek(), left_bracket_tt)) {
            consume(); // consume [

            if (!match(peek(), number_literal_tt)) {
                throw parsing_error("number literal in array dimension", peek(), pos, std::source_location::current().function_name());
            }

            const auto dimension_size = from_numerical_string<std::size_t>(peek().lexeme);
            // get dimension and store for later
            dimensions.push_back(dimension_size);
            consume(); // consume the number literal

            match(peek(), right_bracket_tt); // expect ]
            consume();                       // consume ]
        }

        const bool is_primitive =
            match(peek(), int8_keyword_tt) || match(peek(), int16_keyword_tt) ||
            match(peek(), int32_keyword_tt) || match(peek(), int64_keyword_tt) ||
            match(peek(), uint8_keyword_tt) || match(peek(), uint16_keyword_tt) ||
            match(peek(), uint32_keyword_tt) || match(peek(), uint64_keyword_tt) ||
            match(peek(), float32_keyword_tt) || match(peek(), float64_keyword_tt) ||
            match(peek(), string_keyword_tt) || match(peek(), boolean_keyword_tt) ||
            match(peek(), char_keyword_tt);
        const bool is_custom = match(peek(), identifier_tt) && custom_type_map.contains(peek().lexeme);
        if (!is_primitive && !is_custom) {
            if (match(peek(), identifier_tt)) {
                throw parsing_error("primitive datatype or a declared custom type (saw unknown identifier '" + peek().lexeme + "')", peek(), pos, std::source_location::current().function_name());
            }
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
                        while (!(match(peek(), comma_tt) && paren_depth == 0) && !(match(peek(), right_curly_bracket_tt) && paren_depth == 0)) {
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

    std::unique_ptr<cst_struct> parser::parse_struct(bool is_extern) {
        consume(); // consume struct

        auto struct_node = cst::new_node<cst_struct>();

        auto identifier_node = parse_identifier();
        struct_node->content = identifier_node->content;
        custom_type_map.insert({identifier_node->content, struct_node.get()});

        struct_node->add_child(std::move(identifier_node));

        if (is_extern && match(peek(), semicolon_tt)) {
            consume();
            struct_node->add_child(cst::new_node<cst_struct_is_opaque>());

            return struct_node;
        }

        if (match(peek(), left_curly_bracket_tt)) {
            consume();

            while (!match(peek(), right_curly_bracket_tt)) {
                if (match(peek(), union_keyword_tt)) {
                    // union { fields }: every field shares the same offset.
                    consume();
                    if (!match(peek(), left_curly_bracket_tt)) {
                        throw parsing_error("{ after union", peek(), pos, std::source_location::current().function_name());
                    }
                    consume();
                    auto group = cst::new_node<cst_union_group>();
                    while (!match(peek(), right_curly_bracket_tt)) {
                        if (match(peek(), array_keyword_tt)) {
                            // parse_array consumes its own trailing semicolon.
                            group->add_child(parse_array());
                            continue;
                        }
                        group->add_child(parse_datatype());
                        if (!match(peek(), semicolon_tt)) {
                            throw parsing_error("; after union field", peek(), pos, std::source_location::current().function_name());
                        }
                        consume();
                    }
                    consume(); // }
                    if (match(peek(), semicolon_tt)) consume();
                    struct_node->add_child(std::move(group));
                    continue;
                }
                if (match(peek(), array_keyword_tt)) {
                    // parse_array consumes its own trailing semicolon.
                    struct_node->add_child(parse_array());
                    continue;
                }
                struct_node->add_child(parse_datatype());

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error("; in datatype expression", peek(), pos, std::source_location::current().function_name());
                }
            }

            if (match(peek(), right_curly_bracket_tt)) {
                consume();
            }
            else {
                throw parsing_error("} in struct declaration", peek(), pos, std::source_location::current().function_name());
            }

            return struct_node;
        }

        throw parsing_error("{ in struct declaration", peek(), pos, std::source_location::current().function_name());
    }

    // Parse `type Name = <datatype>;`. The right-hand side is stored as a
    // raw token sequence. parse_datatype splices it in whenever Name is
    // used as a type later on.
    std::unique_ptr<cst> parser::parse_type_alias() {
        consume(); // 'type'
        auto name = parse_identifier();
        if (!match(peek(), assignment_tt)) {
            throw parsing_error("= in type alias", peek(), pos, std::source_location::current().function_name());
        }
        consume();
        const std::size_t rhs_start = pos;
        std::size_t depth_angle = 0;
        std::size_t depth_paren = 0;
        std::size_t end = pos;
        while (end < stream.size()) {
            const auto& tok = stream[end];
            if (tok.tt == less_than_operator_tt) ++depth_angle;
            else if (tok.tt == greater_than_operator_tt && depth_angle > 0) --depth_angle;
            else if (tok.tt == left_paren_tt) ++depth_paren;
            else if (tok.tt == right_paren_tt && depth_paren > 0) --depth_paren;
            else if (tok.tt == semicolon_tt && depth_angle == 0 && depth_paren == 0) break;
            ++end;
        }
        if (end == stream.size()) {
            throw parsing_error("; after type alias", peek(), pos, std::source_location::current().function_name());
        }
        if (end == rhs_start) {
            throw parsing_error("type after = in type alias", peek(), pos, std::source_location::current().function_name());
        }
        type_aliases[name->content] = std::vector<token_t>(stream.begin() + rhs_start, stream.begin() + end);
        pos = end;
        consume(); // ';'
        auto node = cst::new_node<cst_typealias>();
        node->content = name->content;
        return node;
    }

    std::unique_ptr<cst_struct> parser::parse_union_decl() {
        consume(); // consume 'union'

        auto struct_node = cst::new_node<cst_struct>();

        auto identifier_node = parse_identifier();
        struct_node->content = identifier_node->content;
        custom_type_map.insert({identifier_node->content, struct_node.get()});

        struct_node->add_child(std::move(identifier_node));

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ in union declaration", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        auto group = cst::new_node<cst_union_group>();
        while (!match(peek(), right_curly_bracket_tt)) {
            if (match(peek(), array_keyword_tt)) {
                group->add_child(parse_array());
                continue;
            }
            group->add_child(parse_datatype());

            if (!match(peek(), semicolon_tt)) {
                throw parsing_error("; after union field", peek(), pos, std::source_location::current().function_name());
            }
            consume();
        }

        consume(); // }

        if (match(peek(), semicolon_tt)) {
            consume();
        }

        struct_node->add_child(std::move(group));
        return struct_node;
    }

    std::unique_ptr<cst_enum> parser::parse_enum() {
        consume(); // consume 'enum'

        auto enum_node = cst::new_node<cst_enum>();

        if (!match(peek(), identifier_tt)) {
            throw parsing_error("enum name", peek(), pos, std::source_location::current().function_name());
        }

        auto name_node = parse_identifier();
        std::string enum_name = name_node->content;
        enum_node->content = enum_name;
        enum_node->add_child(std::move(name_node));

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ in enum declaration", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '{'

        std::unordered_map<std::string, std::int64_t> members;
        std::int64_t next_value = 0;

        while (!match(peek(), right_curly_bracket_tt)) {
            if (!match(peek(), identifier_tt)) {
                throw parsing_error("enum member name", peek(), pos, std::source_location::current().function_name());
            }

            auto member_node = parse_identifier();
            std::string member_name = member_node->content;

            if (match(peek(), assignment_tt)) {
                consume(); // consume '='
                if (!match(peek(), number_literal_tt)) {
                    throw parsing_error("integer value for enum member", peek(), pos, std::source_location::current().function_name());
                }
                next_value = std::stoll(peek().lexeme);
                consume();
            }

            member_node->content = member_name;
            members[member_name] = next_value;

            auto value_node = cst::new_node<cst_numberliteral>(std::to_string(next_value));
            member_node->add_child(std::move(value_node));
            enum_node->add_child(std::move(member_node));

            next_value++;

            if (match(peek(), comma_tt)) {
                consume();
            }
        }

        if (!match(peek(), right_curly_bracket_tt)) {
            throw parsing_error("} in enum declaration", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '}'

        enum_definitions[enum_name] = members;
        custom_type_map.insert({enum_name, enum_node.get()});

        return enum_node;
    }

    std::unique_ptr<cst_struct> parser::parse_error_decl() {
        consume(); // error
        if (!match(peek(), identifier_tt)) {
            throw parsing_error("name after error", peek(), pos, std::source_location::current().function_name());
        }
        auto node = cst::new_node<cst_struct>();
        auto id = parse_identifier();
        node->content = id->content;
        custom_type_map.insert({id->content, node.get()});
        node->add_child(std::move(id));
        node->add_child(cst::new_node<cst_struct_is_error>());

        if (match(peek(), semicolon_tt)) {
            consume();
            return node;
        }
        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ or ; after error type name", peek(), pos, std::source_location::current().function_name());
        }
        consume();
        while (!match(peek(), right_curly_bracket_tt)) {
            auto field = parse_datatype();
            if (field->get_type() != cst_type::int64_datatype &&
                field->get_type() != cst_type::string_datatype) {
                throw parsing_error("error type fields are i64 or string in this release", peek(), pos, std::source_location::current().function_name());
            }
            if (!match(peek(), semicolon_tt)) {
                throw parsing_error("; after error type field", peek(), pos, std::source_location::current().function_name());
            }
            consume();
            node->add_child(std::move(field));
        }
        consume(); // }
        if (match(peek(), semicolon_tt)) consume();
        return node;
    }

    std::unique_ptr<cst_deferstmt> parser::parse_defer() {
        auto node = cst::new_node<cst_deferstmt>();
        const auto tok = peek();
        node->origin = source_file_path + ":" + std::to_string(tok.line);
        consume(); // defer

        if (match(peek(), left_curly_bracket_tt)) {
            node->add_child(parse_block());
        }
        else {
            // one statement; wrap it in a block so lowering has a single shape
            auto block = cst::new_node<cst_block>();
            block->add_child(parse_keyword(true));
            node->add_child(std::move(block));
        }
        return node;
    }

    std::unique_ptr<cst> parser::parse_try() {
        auto try_node = cst::new_node<cst_trystmt>();
        const auto try_tok = peek();
        try_node->origin = source_file_path + ":" + std::to_string(try_tok.line);
        consume(); // try

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ after try", peek(), pos, std::source_location::current().function_name());
        }
        try_node->add_child(parse_block());

        bool saw_untyped = false;
        while (match(peek(), catch_keyword_tt)) {
            consume();
            auto clause = cst::new_node<cst_catchclause>();
            if (!match(peek(), identifier_tt)) {
                throw parsing_error("name or type for the caught error", peek(), pos, std::source_location::current().function_name());
            }
            // catch TypeName e { ... }  OR  catch e { ... }
            if (peek(1).tt == identifier_tt) {
                if (saw_untyped) {
                    throw parsing_error("a catch after the untyped `catch e` is unreachable", peek(), pos, std::source_location::current().function_name());
                }
                clause->library = peek().lexeme;   // reuse library to carry the type name
                consume();
                clause->content = peek().lexeme;
                consume();
            }
            else {
                clause->content = peek().lexeme;
                saw_untyped = true;
                consume();
            }
            if (!match(peek(), left_curly_bracket_tt)) {
                throw parsing_error("{ after catch", peek(), pos, std::source_location::current().function_name());
            }
            clause->add_child(parse_block());
            try_node->add_child(std::move(clause));
        }
        if (try_node->get_children().size() == 1 && !match(peek(), finally_keyword_tt)) {
            throw parsing_error("catch or finally after a try block", peek(), pos, std::source_location::current().function_name());
        }

        if (match(peek(), finally_keyword_tt)) {
            consume();
            auto finally_node = cst::new_node<cst_finallyblock>();
            if (!match(peek(), left_curly_bracket_tt)) {
                throw parsing_error("{ after finally", peek(), pos, std::source_location::current().function_name());
            }
            finally_node->add_child(parse_block());
            try_node->add_child(std::move(finally_node));
        }

        return try_node;
    }

    std::unique_ptr<cst_errorstmt> parser::parse_error_stmt() {
        auto node = cst::new_node<cst_errorstmt>();
        const auto tok = peek();
        node->origin = source_file_path + ":" + std::to_string(tok.line);
        consume(); // error

        const bool typed_form = match(peek(), identifier_tt) &&
                                (peek(1).tt == semicolon_tt || peek(1).tt == left_curly_bracket_tt);
        if (typed_form) {
            // the type name is the first child, as a bare identifier
            node->content = peek().lexeme;
            auto name = parse_identifier();
            node->add_child(std::move(name));
            if (match(peek(), left_curly_bracket_tt)) {
                consume();
                // each field: ident = <expr> ; or , between fields
                while (!match(peek(), right_curly_bracket_tt)) {
                    if (!match(peek(), identifier_tt)) {
                        throw parsing_error("field name in error initializer", peek(), pos, std::source_location::current().function_name());
                    }
                    auto farg = cst::new_node<cst_functionarg>();
                    auto field = parse_identifier();
                    farg->add_child(std::move(field));
                    if (!match(peek(), assignment_tt)) {
                        throw parsing_error("= after field name", peek(), pos, std::source_location::current().function_name());
                    }
                    consume();
                    // parse expression until , or }
                    const auto start = pos;
                    int depth = 0;
                    while (pos < stream.size()) {
                        const auto t = peek().tt;
                        if (depth == 0 && (t == comma_tt || t == right_curly_bracket_tt)) break;
                        if (t == left_paren_tt || t == left_curly_bracket_tt || t == left_bracket_tt) depth++;
                        else if (t == right_paren_tt || t == right_bracket_tt ||
                                 (t == right_curly_bracket_tt && depth > 0)) depth--;
                        consume();
                    }
                    std::vector<token_t> sub(stream.begin() + start, stream.begin() + pos);
                    sub.push_back(token_t(0, 0, ";", semicolon_tt));
                    for (auto& c : parse_expression(sub)) {
                        farg->add_child(std::move(c));
                    }
                    node->add_child(std::move(farg));
                    if (match(peek(), comma_tt)) consume();
                }
                consume(); // }
            }
            if (!match(peek(), semicolon_tt)) {
                throw parsing_error("; after error raise", peek(), pos, std::source_location::current().function_name());
            }
            consume();
            return node;
        }

        parse_expression_until(node.get(), semicolon_tt);
        if (!match(peek(), semicolon_tt)) {
            throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
        }
        consume();
        if (node->get_children().empty()) {
            throw parsing_error("the message after error", tok, pos, std::source_location::current().function_name());
        }
        return node;
    }

    std::unique_ptr<cst_switchstmt> parser::parse_switch() {
        consume(); // consume 'switch'

        auto switch_node = cst::new_node<cst_switchstmt>();

        if (!match(peek(), left_paren_tt)) {
            throw parsing_error("( after switch", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '('

        std::vector<token_t> expr_tokens;
        int paren_depth = 1;
        while (pos < stream.size() && paren_depth > 0) {
            if (match(peek(), left_paren_tt))
                paren_depth++;
            else if (match(peek(), right_paren_tt)) {
                paren_depth--;
                if (paren_depth == 0)
                    break;
            }
            expr_tokens.push_back(peek());
            consume();
        }

        if (!match(peek(), right_paren_tt)) {
            throw parsing_error(") after switch expression", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume ')'

        auto expr_cst = parse_expression(expr_tokens);
        for (auto& e : expr_cst) {
            switch_node->add_child(std::move(e));
        }

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ after switch expression", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '{'

        while (!match(peek(), right_curly_bracket_tt) && !match(peek(), end_of_file_tt)) {
            if (match(peek(), case_keyword_tt)) {
                consume(); // consume 'case'

                auto case_node = cst::new_node<cst_casestmt>();

                std::vector<token_t> case_expr_tokens;
                while (!match(peek(), colon_tt) && !match(peek(), end_of_file_tt)) {
                    case_expr_tokens.push_back(peek());
                    consume();
                }

                if (!match(peek(), colon_tt)) {
                    throw parsing_error(": after case value", peek(), pos, std::source_location::current().function_name());
                }
                consume(); // consume ':'

                auto case_expr = parse_expression(case_expr_tokens);
                for (auto& e : case_expr) {
                    case_node->add_child(std::move(e));
                }

                if (match(peek(), left_curly_bracket_tt)) {
                    auto body = parse_block();
                    case_node->add_child(std::move(body));
                }

                switch_node->add_child(std::move(case_node));
            }
            else if (match(peek(), default_keyword_tt)) {
                consume(); // consume 'default'

                if (!match(peek(), colon_tt)) {
                    throw parsing_error(": after default", peek(), pos, std::source_location::current().function_name());
                }
                consume(); // consume ':'

                auto default_node = cst::new_node<cst_defaultstmt>();

                auto body = parse_block();
                default_node->add_child(std::move(body));

                switch_node->add_child(std::move(default_node));
            }
            else {
                throw parsing_error("case or default in switch body", peek(), pos, std::source_location::current().function_name());
            }
        }

        if (!match(peek(), right_curly_bracket_tt)) {
            throw parsing_error("} to close switch", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '}'

        return switch_node;
    }

    std::string parser::current_module_prefix() const {
        std::string prefix;
        for (const auto& m : module_prefix_stack) {
            prefix += m + "::";
        }
        return prefix;
    }

    static void prefix_module_references(cst* node, const std::string& prefix, const std::unordered_set<std::string>& module_names) {
        if (!node)
            return;

        if (node->get_type() == cst_type::functioncall && node->content == "start_call") {
            if (!node->get_children().empty()) {
                auto* name_child = node->get_children().front().get();
                if (name_child->get_type() == cst_type::identifier) {
                    if (module_names.count(name_child->content) && name_child->content.find("::") == std::string::npos) {
                        name_child->content = prefix + name_child->content;
                    }
                }
            }
        }

        for (auto& child : node->get_children()) {
            prefix_module_references(child.get(), prefix, module_names);
        }
    }

    std::unique_ptr<cst> parser::parse_module() {
        consume(); // consume 'module'

        if (!match(peek(), identifier_tt)) {
            throw parsing_error("module name", peek(), pos, std::source_location::current().function_name());
        }

        std::string module_name = peek().lexeme;
        consume(); // consume module name

        std::string library;
        if (match(peek(), identifier_tt) && peek().lexeme == "link") {
            consume();
            if (!match(peek(), string_literal_tt)) {
                throw parsing_error("library name in quotes after link", peek(), pos, std::source_location::current().function_name());
            }
            library = peek().lexeme;
            if (library.empty()) {
                throw parsing_error("a library name, not an empty string", peek(), pos, std::source_location::current().function_name());
            }
            consume();
        }

        // module graphics export { ... }: mark every fn in the block exported.
        bool module_exports = false;
        if (match(peek(), export_keyword_tt)) {
            consume();
            module_exports = true;
        }

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{ after module name", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '{'

        module_prefix_stack.push_back(module_name);
        std::string prefix = current_module_prefix();

        auto container = cst::new_node<cst_root>();

        while (!match(peek(), right_curly_bracket_tt) && !match(peek(), end_of_file_tt)) {
            auto node = parse_keyword(true);

            if (node->get_type() == cst_type::root) {
                for (auto& child : node->get_children()) {
                    container->add_child(std::move(child));
                }
            }
            else {
                auto ntype = node->get_type();
                if (ntype == cst_type::function) {
                    if (!node->get_children().empty()) {
                        auto* name_node = node->get_children().front().get();
                        if (name_node->get_type() == cst_type::identifier) {
                            name_node->content = prefix + name_node->content;
                        }
                    }
                    if (module_exports) {
                        node->add_child(cst::new_node<cst_func_is_exported>());
                    }
                }
                else if (ntype == cst_type::structure) {
                    if (!node->get_children().empty()) {
                        auto* first_child = node->get_children().front().get();
                        if (first_child->get_type() == cst_type::identifier) {
                            std::string old_name = first_child->content;
                            std::string new_name = prefix + old_name;
                            node->content = new_name;
                            first_child->content = new_name;
                            if (custom_type_map.contains(old_name)) {
                                custom_type_map[new_name] = custom_type_map[old_name];
                            }

                            for (std::size_t ci = 1; ci < node->get_children().size(); ++ci) {
                                auto* child = node->get_children()[ci].get();
                                if (child->get_type() != cst_type::function || child->get_children().empty()) {
                                    continue;
                                }
                                auto* fn_name = child->get_children().front().get();
                                if (fn_name->get_type() != cst_type::identifier) {
                                    continue;
                                }
                                const std::string old_method_prefix = old_name + "::";
                                if (fn_name->content.rfind(old_method_prefix, 0) == 0) {
                                    fn_name->content = new_name + fn_name->content.substr(old_name.size());
                                }
                            }
                        }
                    }
                }
                else if (ntype == cst_type::enumeration) {
                    std::string old_name = node->content;
                    std::string new_name = prefix + old_name;
                    node->content = new_name;
                    if (!node->get_children().empty()) {
                        auto* name_node = node->get_children().front().get();
                        if (name_node->get_type() == cst_type::identifier) {
                            name_node->content = new_name;
                        }
                    }
                    if (enum_definitions.contains(old_name)) {
                        enum_definitions[new_name] = enum_definitions[old_name];
                    }
                    if (custom_type_map.contains(old_name)) {
                        custom_type_map[new_name] = custom_type_map[old_name];
                    }
                }

                container->add_child(std::move(node));
            }
        }

        if (!match(peek(), right_curly_bracket_tt)) {
            throw parsing_error("} to close module", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume '}' of module

        std::unordered_set<std::string> module_func_names;
        for (const auto& child : container->get_children()) {
            if (child->get_type() == cst_type::function && !child->get_children().empty()) {
                auto* name_node = child->get_children().front().get();
                if (name_node->get_type() == cst_type::identifier) {
                    const std::string& full_name = name_node->content;
                    auto sep = full_name.rfind("::");
                    if (sep != std::string::npos) {
                        module_func_names.insert(full_name.substr(sep + 2));
                    }
                    else {
                        module_func_names.insert(full_name);
                    }
                }
            }
        }

        if (!module_func_names.empty()) {
            for (auto& child : container->get_children()) {
                if (child->get_type() == cst_type::function) {
                    prefix_module_references(child.get(), prefix, module_func_names);
                }
            }
        }

        if (!library.empty()) {
            for (auto& child : container->get_children()) {
                if (child->get_type() != cst_type::function || !child->library.empty()) {
                    continue;
                }
                for (auto& part : child->get_children()) {
                    if (part->get_type() == cst_type::func_is_external) {
                        child->library = library;
                        break;
                    }
                }
            }
        }

        module_prefix_stack.pop_back();

        return container;
    }

    std::unique_ptr<cst> parser::parse_import() {
        consume(); // consume 'import'

        std::string import_path;
        std::vector<std::string> path_segments;

        if (!match(peek(), identifier_tt)) {
            throw parsing_error("module path after import", peek(), pos, std::source_location::current().function_name());
        }

        import_path = peek().lexeme;
        path_segments.push_back(peek().lexeme);
        consume();

        while (match(peek(), scope_resolution_tt)) {
            consume(); // consume '::'
            if (!match(peek(), identifier_tt)) {
                throw parsing_error("identifier after ::", peek(), pos, std::source_location::current().function_name());
            }
            import_path += "::" + peek().lexeme;
            path_segments.push_back(peek().lexeme);
            consume();
        }

        std::string from_source;
        if (match(peek(), identifier_tt) && peek().lexeme == "from") {
            consume(); // consume 'from'
            if (!match(peek(), identifier_tt)) {
                throw parsing_error("source path after 'from'", peek(), pos, std::source_location::current().function_name());
            }
            from_source = peek().lexeme;
            consume();

            while (match(peek(), division_operator_tt)) {
                consume(); // consume '/'
                if (!match(peek(), identifier_tt)) {
                    throw parsing_error("identifier after '/' in from path", peek(), pos, std::source_location::current().function_name());
                }
                from_source += "/";
                from_source += peek().lexeme;
                consume();
            }
        }

        if (!match(peek(), semicolon_tt)) {
            throw parsing_error("; after import", peek(), pos, std::source_location::current().function_name());
        }
        consume(); // consume ';'

        auto try_load_module_file = [&](const std::filesystem::path& base_dir) -> std::unique_ptr<cst_root> {
            std::filesystem::path module_file = base_dir;
            for (const auto& seg : path_segments) {
                module_file /= seg;
            }
            module_file += ".occ";

            if (!std::filesystem::exists(module_file)) {
                return nullptr;
            }

            std::ifstream file(module_file);
            if (!file.is_open()) {
                throw parsing_error("readable module file '" + module_file.string() + "'", peek(), pos, std::source_location::current().function_name());
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string src = buffer.str();

            lexer l(src);
            auto included_stream = l.analyze();

            parser p(included_stream, module_file.string());
            p.import_generic_templates(generic_struct_templates, generic_func_templates, instantiated_generics, custom_type_map, enum_definitions);
            auto included_cst = p.parse();

            import_generic_templates(p.get_generic_struct_templates(), p.get_generic_func_templates(), p.get_instantiated_generics(), p.get_custom_type_map(), p.get_enum_definitions());

            return included_cst;
        };

        std::unique_ptr<cst_root> loaded_cst = nullptr;

        if (!source_file_path.empty()) {
            auto source_dir = std::filesystem::path(source_file_path).parent_path();
            if (source_dir.empty())
                source_dir = ".";

            if (!from_source.empty()) {
                loaded_cst = try_load_module_file(source_dir / from_source);

                if (!loaded_cst) {
                    loaded_cst = try_load_module_file(source_dir / ".." / from_source);
                }
            }
            else {
                loaded_cst = try_load_module_file(source_dir);

                if (!loaded_cst) {
                    loaded_cst = try_load_module_file(source_dir / "lib");
                }

                if (!loaded_cst) {
                    loaded_cst = try_load_module_file(source_dir / ".." / "lib");
                }
            }
        }

        imported_modules.push_back(import_path);

        if (loaded_cst) {
            auto container = cst::new_node<cst_root>();

            for (auto& child : loaded_cst->get_children()) {
                container->add_child(std::move(child));
            }

            auto import_node = cst::new_node<cst_importdecl>();
            import_node->content = import_path;
            container->add_child(std::move(import_node));

            return container;
        }

        auto node = cst::new_node<cst_importdecl>();
        node->content = import_path;
        return node;
    }

    std::unique_ptr<cst> parser::parse_compound_assignment_identifier(std::unique_ptr<cst_identifier> to_assign) {

        std::function<void()> handle_compound_assignment([&]() -> void {
            auto assignment_node = cst::new_node<cst_assignment>();
            to_assign->add_child(std::move(assignment_node));

            if (match(peek(), semicolon_tt)) {
                throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
            }

            parse_expression_until(to_assign.get(), semicolon_tt);

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }
        });

        switch (peek().tt) { // special assignments
        case add_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_add>("+"));

                break;
            }
        case sub_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_subtract>("-"));

                break;
            }
        case mul_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_multiply>("*"));

                break;
            }
        case div_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_divide>("/"));

                break;
            }
        case mod_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_modulo>("%"));

                break;
            }
        case and_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_bitwise_and>("&"));

                break;
            }
        case or_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_bitwise_or>("|"));

                break;
            }
        case xor_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_xor>("^"));

                break;
            }
        case lshift_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_bitwise_lshift>("<<"));

                break;
            }
        case rshift_assign_tt:
            {
                consume();

                to_assign->add_child(cst::new_node<cst_identifier>(to_assign->content));

                handle_compound_assignment();

                to_assign->add_child(cst::new_node<cst_bitwise_rshift>(">>"));

                break;
            }
        default:
            {
                break;
            }
        }

        return to_assign;
    }

    std::string parser::parse_asm_arch() {
        if (!match(peek(), left_paren_tt)) {
            throw parsing_error("'(' and an architecture, e.g. asm(x86_64)", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        if (!match(peek(), identifier_tt)) {
            throw parsing_error("architecture name, e.g. x86_64", peek(), pos, std::source_location::current().function_name());
        }
        std::string arch = peek().lexeme;
        const auto arch_token = peek();
        consume();

        if (!match(peek(), right_paren_tt)) {
            throw parsing_error(")", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        for (auto& ch : arch) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (arch == "x86_64" || arch == "amd64" || arch == "x64") {
            return "x86_64";
        }

        static const std::unordered_set<std::string> known_unsupported = {"x86", "i386", "i686", "arm", "arm64", "aarch64", "riscv32", "riscv64", "wasm32", "wasm64", "ppc64", "mips", "mips64"};
        if (known_unsupported.contains(arch)) {
            throw parsing_error("a supported architecture (only x86_64 is implemented; '" + arch + "' is not yet)", arch_token, pos, std::source_location::current().function_name());
        }

        throw parsing_error("a known architecture (x86_64)", arch_token, pos, std::source_location::current().function_name());
    }

    std::unique_ptr<cst_shellcode> parser::parse_shellcode_bytes(const std::string& arch) {
        auto shellcode_node = cst::new_node<cst_shellcode>(arch);

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        while (!match(peek(), right_curly_bracket_tt)) {
            if (match(peek(), comma_tt)) { // bytes may be comma separated
                consume();
                continue;
            }
            if (!match(peek(), number_literal_tt)) {
                throw parsing_error("<number literal>", peek(), pos, std::source_location::current().function_name());
            }

            auto number_token = peek();
            consume();
            shellcode_node->add_child(cst::new_node<cst_numberliteral>(number_token.lexeme));
        }

        consume(); // }

        return shellcode_node;
    }

    std::unique_ptr<cst> parser::parse_asm_statement() {
        if (match(peek(), shellcode_denoter_tt)) {
            consume();
            const std::string arch = parse_asm_arch();
            return parse_shellcode_bytes(arch);
        }

        consume(); // asm
        const std::string arch = parse_asm_arch();
        auto asm_node = cst::new_node<cst_asm_code>(arch);

        if (!match(peek(), left_curly_bracket_tt)) {
            throw parsing_error("{", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        if (!match(peek(), asm_body_tt)) {
            throw parsing_error("assembly code", peek(), pos, std::source_location::current().function_name());
        }
        asm_node->add_child(cst::new_node<cst_stringliteral>(peek().lexeme));
        consume();

        if (!match(peek(), right_curly_bracket_tt)) {
            throw parsing_error("}", peek(), pos, std::source_location::current().function_name());
        }
        consume();

        return asm_node;
    }

    std::unique_ptr<cst> parser::parse_keyword(bool nested_function) {
        ++recursion_depth;

        // RAII guard to decrement depth on all exit paths (including throws)
        struct depth_guard {
            std::size_t& depth;
            ~depth_guard() { --depth; }
        } guard{recursion_depth};

        if (recursion_depth > max_recursion_depth) {
            throw parsing_error("expression (recursion depth exceeded)", peek(), pos, std::source_location::current().function_name());
        }

        if (match(peek(), asm_keyword_tt) || match(peek(), shellcode_denoter_tt)) {
            return parse_asm_statement();
        }

        if (match(peek(), generic_keyword_tt)) {
            cst_generic_type_cache.clear();

            /*
                Just have to clear these above ^^^
                Prevents reuse of generics in structs and functions
            */

            consume();

            if (match(peek(), less_than_operator_tt)) {
                consume(); // '<'

                if (match(peek(), greater_than_operator_tt)) {
                    throw parsing_error("typename in generic definition", peek(), pos, std::source_location::current().function_name());
                }

                std::vector<std::string> type_params;

                while (!match(peek(), greater_than_operator_tt)) {
                    auto generic_typename = parse_identifier()->content; // typename
                    cst_generic_type_cache[generic_typename] = cst::new_node<cst_generic_type>(generic_typename);
                    type_params.push_back(generic_typename);

                    if (match(peek(), comma_tt) && match(peek(1), greater_than_operator_tt)) {
                        throw parsing_error("typename after comma", peek(), pos, std::source_location::current().function_name());
                    }

                    if (match(peek(), comma_tt)) {
                        consume();
                    }
                }

                if (!match(peek(), greater_than_operator_tt)) {
                    throw parsing_error("greater than operator (for generic type)", peek(), pos, std::source_location::current().function_name());
                }

                consume(); // '>'

                auto template_start = pos;

                if (match(peek(), struct_keyword_tt)) {
                    auto struct_node = parse_struct();
                    auto template_end = pos;

                    auto struct_name = struct_node->get_children().front()->content;

                    generic_struct_templates[struct_name] = {type_params, std::vector<token_t>(stream.begin() + template_start, stream.begin() + template_end), struct_name};

                    custom_type_map.erase(struct_name);
                    cst_generic_type_cache.clear();

                    return cst::new_node<cst_root>();
                }
                else if (match(peek(), function_keyword_tt)) {
                    auto func_node = parse_function();
                    auto template_end = pos;

                    auto func_name = func_node->get_children().front()->content;

                    generic_func_templates[func_name] = {type_params, std::vector<token_t>(stream.begin() + template_start, stream.begin() + template_end), func_name};

                    cst_generic_type_cache.clear();

                    return cst::new_node<cst_root>();
                }
                else {
                    throw parsing_error("function or struct definition", peek(), pos, std::source_location::current().function_name());
                }
            }
            else {
                throw parsing_error("'<' with type parameters after 'generic' (e.g., generic<T>)", peek(), pos, std::source_location::current().function_name());
            }
        }

        if (match(peek(), function_keyword_tt) && match(peek(1), left_paren_tt)) {
            return parse_fnptr_decl(); // fn(<types>) [ret] name [= expr];
        }

        if (match(peek(), extern_keyword_tt)) {
            consume();

            if (match(peek(), struct_keyword_tt)) {
                return parse_struct(true);
            }

            if (!match(peek(), function_keyword_tt)) {
                throw parsing_error("fn or struct", peek(), pos, std::source_location::current().function_name());
            }

            return parse_function(true);
        }

        if (nested_function) {
            if (match(peek(), function_keyword_tt)) {
                return parse_function();
            }
        }

        if (match(peek(), include_keyword_tt)) {
            consume();

            if (match(peek(), string_literal_tt)) {
                consume();
                auto string_token = previous();

                std::filesystem::path include_path = string_token.lexeme;
                if (!include_path.is_absolute() && !source_file_path.empty()) {

                    std::filesystem::path source_dir = std::filesystem::path(source_file_path).parent_path();

                    include_path = (source_dir / include_path).lexically_normal();
                }

                std::ifstream file(include_path);
                if (!file.is_open()) {
                    throw parsing_error("existing include file", string_token, pos, std::source_location::current().function_name());
                }

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string src = buffer.str();

                lexer l(src);
                auto included_stream = l.analyze();

                parser p(included_stream, include_path.string());
                p.import_generic_templates(generic_struct_templates, generic_func_templates, instantiated_generics, custom_type_map, enum_definitions);
                auto included_cst = p.parse();

                import_generic_templates(p.get_generic_struct_templates(), p.get_generic_func_templates(), p.get_instantiated_generics(), p.get_custom_type_map(), p.get_enum_definitions());

                return included_cst;
            }

            throw parsing_error("string literal", peek(), pos, std::source_location::current().function_name());
        }
        if (match(peek(), const_keyword_tt) || match(peek(), global_keyword_tt)) {
            // any order of 'const' / 'global' before a declaration
            bool is_const = false;
            while (match(peek(), const_keyword_tt) || match(peek(), global_keyword_tt)) {
                if (match(peek(), global_keyword_tt) && !nested_function) {
                    throw parsing_error("declaration at top level ('global' is not allowed inside a function)", peek(), pos, std::source_location::current().function_name());
                }
                is_const |= match(peek(), const_keyword_tt);
                consume();
            }

            auto node = parse_keyword(false);
            if (node && is_const) {
                node->is_const = true;
            }
            return node;
        }
        if (match(peek(), int8_keyword_tt)) {
            return parse_integer_type<cst_int8>();
        }
        if (match(peek(), int16_keyword_tt)) {
            return parse_integer_type<cst_int16>();
        }
        if (match(peek(), int32_keyword_tt)) {
            return parse_integer_type<cst_int32>();
        }
        if (match(peek(), int64_keyword_tt)) {
            return parse_integer_type<cst_int64>();
        }
        if (match(peek(), uint8_keyword_tt)) {
            return parse_integer_type<cst_uint8>();
        }
        if (match(peek(), uint16_keyword_tt)) {
            return parse_integer_type<cst_uint16>();
        }
        if (match(peek(), uint32_keyword_tt)) {
            return parse_integer_type<cst_uint32>();
        }
        if (match(peek(), uint64_keyword_tt)) {
            return parse_integer_type<cst_uint64>();
        }
        if (match(peek(), float32_keyword_tt)) {
            return parse_integer_type<cst_float32>();
        }
        if (match(peek(), float64_keyword_tt)) {
            return parse_integer_type<cst_float64>();
        }
        if (match(peek(), string_keyword_tt)) {
            return parse_string();
        }
        if (match(peek(), char_keyword_tt)) {
            auto node = parse_integer_type<cst_int8>();
            node->is_char = true;
            return node;
        }
        if (match(peek(), boolean_keyword_tt)) {
            return parse_integer_type<cst_bool>();
        }
        if (match(peek(), identifier_tt) && pos + 2 < stream.size() && match(peek(1), scope_resolution_tt) && match(peek(2), identifier_tt)) {
            std::string mod_name = peek().lexeme;
            std::string qualified = mod_name + "::" + peek(2).lexeme;
            std::string bare_name = peek(2).lexeme;

            if (pos + 3 < stream.size() && peek(3).tt == less_than_operator_tt && (generic_struct_templates.contains(qualified) || generic_struct_templates.contains(bare_name))) {
                std::string template_name = generic_struct_templates.contains(qualified) ? qualified : bare_name;
                consume(); // consume module name
                consume(); // consume '::'
                consume(); // consume type name
                consume(); // consume '<'

                auto type_args = parse_type_argument_tokens();
                const bool has_generic_param = type_args_have_generic_params(type_args);

                std::string struct_name = template_name;
                if (!has_generic_param) {
                    struct_name = instantiate_generic_struct(template_name, type_args);
                }

                auto node = cst::new_node<cst_struct>();
                node->content = struct_name;

                if (match(peek(), multiply_operator_tt)) {
                    while (match(peek(), multiply_operator_tt)) {
                        consume();
                        node->num_pointers++;
                    }
                }

                if (match(peek(), identifier_tt)) {
                    node->add_child(parse_identifier());
                }
                else {
                    throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
                }

                if (match(peek(), assignment_tt)) {
                    node->add_child(parse_assignment());

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return node;
            }

            if (custom_type_map.contains(qualified)) {
                consume(); // consume module name
                consume(); // consume '::'
                consume(); // consume type name

                auto node = cst::new_node<cst_struct>();
                node->content = qualified;

                if (match(peek(), reference_operator_tt)) {
                    consume();
                    node->is_reference = true;
                }

                if (match(peek(), multiply_operator_tt)) {
                    while (match(peek(), multiply_operator_tt)) {
                        consume();
                        node->num_pointers++;
                    }
                }

                if (match(peek(), identifier_tt)) {
                    node->add_child(parse_identifier());
                }
                else {
                    throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
                }

                if (match(peek(), assignment_tt)) {
                    node->add_child(parse_assignment());

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return node;
            }
        }
        if (match(peek(), identifier_tt) && generic_struct_templates.contains(peek().lexeme) && peek(1).tt == less_than_operator_tt) {
            auto template_name = peek().lexeme;
            consume(); // consume template name
            consume(); // consume '<'

            auto type_args = parse_type_argument_tokens();
            const bool has_generic_param = type_args_have_generic_params(type_args);

            std::string struct_name = template_name;
            if (!has_generic_param) {
                struct_name = instantiate_generic_struct(template_name, type_args);
            }

            auto node = cst::new_node<cst_struct>();
            node->content = struct_name;

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }
            else {
                throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
            }

            if (match(peek(), assignment_tt)) {
                node->add_child(parse_assignment());

                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }

                parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
            }

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            return node;
        }
        {
            std::unique_ptr<cst> fwd_node;
            if (try_parse_forward_generic_struct_type(fwd_node)) {
                if (match(peek(), identifier_tt)) {
                    fwd_node->add_child(parse_identifier());
                }
                else {
                    throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
                }

                if (match(peek(), assignment_tt)) {
                    fwd_node->add_child(parse_assignment());

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(fwd_node->get_children().at(1).get(), semicolon_tt);
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return fwd_node;
            }
        }
        if (match(peek(), identifier_tt)) {
            if (auto it = type_aliases.find(peek().lexeme); it != type_aliases.end()) {
                if (type_alias_depth > 32) {
                    throw parsing_error("non-cyclic type alias", peek(), pos, std::source_location::current().function_name());
                }
                const auto tokens = it->second;
                stream.erase(stream.begin() + pos);
                stream.insert(stream.begin() + pos, tokens.begin(), tokens.end());
                ++type_alias_depth;
                auto r = parse_keyword(nested_function);
                --type_alias_depth;
                return r;
            }
        }
        if (match(peek(), identifier_tt) && custom_type_map.contains(peek().lexeme)) {
            return parse_custom_type();
        }
        if (match(peek(), identifier_tt) && cst_generic_type_cache.contains(peek().lexeme)) { // T* var = expr; or T var = expr;
            const auto type_name = peek().lexeme;
            consume(); // consume generic type identifier

            auto node = cst::new_node<cst_generic_type>(type_name);

            if (match(peek(), multiply_operator_tt)) {
                while (match(peek(), multiply_operator_tt)) {
                    consume();
                    node->num_pointers++;
                }
            }

            if (match(peek(), identifier_tt)) {
                node->add_child(parse_identifier());
            }
            else {
                throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
            }

            if (match(peek(), assignment_tt)) {
                node->add_child(parse_assignment());

                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }

                parse_expression_until(node->get_children().at(1).get(), semicolon_tt);
            }

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            return node;
        }
        /*if (match(peek(), dereference_operator_tt)) {
            auto deref_count = 0;
            while (!match(peek(), left_paren_tt) && !match(peek(), identifier_tt)) {
                deref_count++;
                consume();
            }

            if (match(peek(), identifier_tt)) {
                auto deref_node =
        cst::new_node<cst_dereference>(std::to_string(deref_count));

                auto deref_expr = cst::new_node<cst_generic_expr>();

                deref_expr->add_child(parse_identifier());

                deref_node->add_child(std::move(deref_expr));

                if (match(peek(), assignment_tt)) {
                    auto assignment = parse_assignment();

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)",
        peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(assignment.get(),
                                           semicolon_tt); // parse the expression
        until the semicolon

                    deref_node->add_child(std::move(assignment));
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                } else {
                    throw parsing_error(";", peek(), pos,
        std::source_location::current().function_name());
                }

                return deref_node;
            }

            if (match(peek(), left_paren_tt)) {
                consume(); // consume the '('

                auto deref_node =
        cst::new_node<cst_dereference>(std::to_string(deref_count)); auto deref_expr =
        cst::new_node<cst_generic_expr>();

                // Use the new function to find matching paren
                const auto matching_paren_pos = find_matching_paren(stream.begin() +
        pos, stream.end()); if (matching_paren_pos == -1) { throw
        parsing_error("matching )", peek(), pos,
        std::source_location::current().function_name());
                }

                std::vector<token_t> sub_stream = {stream.begin() + pos,
        stream.begin() + pos + matching_paren_pos}; auto converted_rpn =
        parse_expression(sub_stream);

                for (auto& c : converted_rpn) {
                    deref_expr->add_child(std::move(c));
                }

                pos += matching_paren_pos; // move past the expression
                deref_node->add_child(std::move(deref_expr));

                if (match(peek(), right_paren_tt)) {
                    consume();
                } else {
                    throw parsing_error(")", peek(), pos,
        std::source_location::current().function_name());
                }

                if (match(peek(), assignment_tt)) {
                    auto assignment = parse_assignment();

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)",
        peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(assignment.get(),
                                           semicolon_tt); // parse the expression
        until the semicolon

                    deref_node->add_child(std::move(assignment));
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                } else {
                    throw parsing_error(";", peek(), pos,
        std::source_location::current().function_name());
                }

                return deref_node;
            }
            // throw parsing_error("<identifier>", peek(), pos,
            // std::source_location::current().function_name());
        }*/
        if (match(peek(), dereference_operator_tt)) {
            auto deref_count = 0;
            while (match(peek(), dereference_operator_tt)) {
                deref_count++;
                consume();
            }

            auto deref_node = cst::new_node<cst_dereference>(std::to_string(deref_count));
            auto deref_expr = cst::new_node<cst_generic_expr>();

            if (match(peek(), left_paren_tt)) {
                // find matching paren with depth tracking
                int depth = 0;
                std::size_t paren_pos = 0;
                for (std::size_t i = pos; i < stream.size(); i++) {
                    if (stream[i].tt == left_paren_tt)
                        depth++;
                    else if (stream[i].tt == right_paren_tt) {
                        depth--;
                        if (depth == 0) {
                            paren_pos = i;
                            break;
                        }
                    }
                }

                consume(); // consume '('
                std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + paren_pos};
                auto converted_rpn = parse_expression(sub_stream);

                for (auto& c : converted_rpn) {
                    deref_expr->add_child(std::move(c));
                }

                pos = paren_pos; // jump to ')'

                if (match(peek(), right_paren_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(")", peek(), pos, std::source_location::current().function_name());
                }
            }
            else if (match(peek(), identifier_tt)) {
                deref_expr->add_child(parse_identifier());
            }
            else {
                throw parsing_error("identifier or (", peek(), pos, std::source_location::current().function_name());
            }

            // Handle $p.x or $p.x.y (dereference + member access)
            if (match(peek(), period_tt)) {
                // Handle $var.member[.member...] [= value]; (dereference + member access)
                auto member_access_node = cst::new_node<cst_memberaccess>();

                auto base_ident = cst::new_node<cst_identifier>();
                base_ident->content = deref_expr->get_children().front()->content;
                base_ident->num_pointers = deref_count; // store deref count for codegen
                member_access_node->add_child(std::move(base_ident));

                // Parse member chain: .x, .y, .z, etc.
                while (match(peek(), period_tt)) {
                    consume(); // consume '.'
                    if (match(peek(), identifier_tt)) {
                        member_access_node->add_child(parse_identifier());
                    }
                    else {
                        throw parsing_error("member name after '.'", peek(), pos, std::source_location::current().function_name());
                    }
                }

                // Check for assignment
                if (match(peek(), assignment_tt)) {
                    auto assignment = parse_assignment();

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(assignment.get(), semicolon_tt);
                    member_access_node->add_child(std::move(assignment));
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                // Mark the member access as a dereference operation
                member_access_node->num_pointers = deref_count;
                return member_access_node;
            }

            deref_node->add_child(std::move(deref_expr));

            auto parse_deref_compound_assignment = [&](token_type tt) -> bool {
                if (!match(peek(), tt)) {
                    return false;
                }

                consume(); // consume compound assignment token

                auto assignment = cst::new_node<cst_assignment>();
                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }

                parse_expression_until(assignment.get(), semicolon_tt);

                std::vector<std::unique_ptr<cst>> rhs_nodes;
                for (auto& child : assignment->get_children()) {
                    rhs_nodes.push_back(std::move(child));
                }
                assignment->get_children().clear();

                auto* lhs_source = deref_node->get_children().front().get();
                for (const auto& child : lhs_source->get_children()) {
                    assignment->add_child(child->clone());
                }
                assignment->add_child(cst::new_node<cst_dereference>(std::to_string(deref_count)));

                for (auto& node : rhs_nodes) {
                    assignment->add_child(std::move(node));
                }

                switch (tt) {
                case add_assign_tt:
                    assignment->add_child(cst::new_node<cst_add>("+"));
                    break;
                case sub_assign_tt:
                    assignment->add_child(cst::new_node<cst_subtract>("-"));
                    break;
                case mul_assign_tt:
                    assignment->add_child(cst::new_node<cst_multiply>("*"));
                    break;
                case div_assign_tt:
                    assignment->add_child(cst::new_node<cst_divide>("/"));
                    break;
                case mod_assign_tt:
                    assignment->add_child(cst::new_node<cst_modulo>("%"));
                    break;
                case and_assign_tt:
                    assignment->add_child(cst::new_node<cst_bitwise_and>("&"));
                    break;
                case or_assign_tt:
                    assignment->add_child(cst::new_node<cst_bitwise_or>("|"));
                    break;
                case xor_assign_tt:
                    assignment->add_child(cst::new_node<cst_xor>("^"));
                    break;
                case lshift_assign_tt:
                    assignment->add_child(cst::new_node<cst_bitwise_lshift>("<<"));
                    break;
                case rshift_assign_tt:
                    assignment->add_child(cst::new_node<cst_bitwise_rshift>(">>"));
                    break;
                default:
                    break;
                }

                deref_node->add_child(std::move(assignment));
                return true;
            };

            bool handled_compound_assignment = parse_deref_compound_assignment(add_assign_tt) || parse_deref_compound_assignment(sub_assign_tt) || parse_deref_compound_assignment(mul_assign_tt) ||
                                               parse_deref_compound_assignment(div_assign_tt) || parse_deref_compound_assignment(mod_assign_tt) || parse_deref_compound_assignment(and_assign_tt) ||
                                               parse_deref_compound_assignment(or_assign_tt) || parse_deref_compound_assignment(xor_assign_tt) || parse_deref_compound_assignment(lshift_assign_tt) ||
                                               parse_deref_compound_assignment(rshift_assign_tt);

            if (match(peek(), assignment_tt)) {
                auto assignment = parse_assignment();

                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }

                parse_expression_until(assignment.get(), semicolon_tt);
                deref_node->add_child(std::move(assignment));
            }
            else if (!handled_compound_assignment) {
                throw parsing_error("assignment operator after dereference", peek(), pos, std::source_location::current().function_name());
            }

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            return deref_node;
        }
        if (match(peek(), reference_operator_tt)) {
            consume();

            if (match(peek(), identifier_tt)) {
                auto identifier = parse_identifier();

                auto ref_node = cst::new_node<cst_reference>();

                ref_node->add_child(std::move(identifier));

                if (match(peek(), assignment_tt)) {
                    ref_node->add_child(parse_assignment());

                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }

                    parse_expression_until(ref_node.get(),
                                           semicolon_tt); // parse the expression until the semicolon
                }

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return ref_node;
            }

            throw parsing_error("<identifier>", peek(), pos, std::source_location::current().function_name());
        }
        if (match(peek(), if_keyword_tt)) {
            return parse_if();
        }
        if (match(peek(), try_keyword_tt)) {
            return parse_try();
        }
        if (match(peek(), defer_keyword_tt)) {
            return parse_defer();
        }
        if (match(peek(), error_keyword_tt)) {
            if (nested_function && peek(1).tt == identifier_tt &&
                (peek(2).tt == semicolon_tt || peek(2).tt == left_curly_bracket_tt)) {
                return parse_error_decl();
            }
            return parse_error_stmt();
        }
        if (match(peek(), loop_keyword_tt)) {
            return parse_loop();
        }
        if (match(peek(), break_keyword_tt)) {
            return parse_break();
        }
        if (match(peek(), continue_keyword_tt)) {
            return parse_continue();
        }
        if (match(peek(), goto_keyword_tt)) {
            return parse_goto();
        }
        // a label. name :: is scope resolution, so rule it out first.
        if (match(peek(), identifier_tt) && peek(1).tt == colon_tt) {
            return parse_label();
        }
        if (match(peek(), while_keyword_tt)) {
            return parse_while();
        }
        if (match(peek(), for_keyword_tt)) {
            return parse_for();
        }
        if (match(peek(), return_keyword_tt)) {
            return parse_return();
        }
        if (match(peek(), array_keyword_tt)) {
            return parse_array();
        }
        if (match(peek(), struct_keyword_tt)) {
            return parse_struct();
        }
        if (match(peek(), union_keyword_tt) && match(peek(1), identifier_tt)) {
            return parse_union_decl();
        }
        if (match(peek(), type_keyword_tt) && match(peek(1), identifier_tt)) {
            return parse_type_alias();
        }
        if (match(peek(), export_keyword_tt)) {
            consume();
            if (match(peek(), function_keyword_tt)) {
                auto f = parse_function();
                f->add_child(cst::new_node<cst_func_is_exported>());
                return f;
            }
            if (match(peek(), module_keyword_tt)) {
                auto m = parse_module();
                // mark every top-level fn inside the module as exported
                for (auto& c : m->get_children()) {
                    if (c->get_type() == cst_type::function) {
                        c->add_child(cst::new_node<cst_func_is_exported>());
                    }
                }
                return m;
            }
            auto node = parse_keyword(nested_function);
            if (node) {
                if (node->get_type() == cst_type::function) {
                    node->add_child(cst::new_node<cst_func_is_exported>());
                } else {
                    node->add_child(cst::new_node<cst_global_is_exported>());
                }
            }
            return node;
        }
        if (match(peek(), enum_keyword_tt)) {
            return parse_enum();
        }
        if (match(peek(), switch_keyword_tt)) {
            return parse_switch();
        }
        if (match(peek(), module_keyword_tt)) {
            return parse_module();
        }
        if (match(peek(), import_keyword_tt)) {
            return parse_import();
        }
        if (match(peek(), identifier_tt) && peek(1).tt == less_than_operator_tt) { // potential generic fn call at
                                                                                   // statement level
            // Scan ahead to check for pattern: identifier < type_args > (
            std::size_t scan = pos + 2; // skip identifier and '<'
            bool found_generic_call = false;
            while (scan < stream.size() && stream[scan].tt != semicolon_tt && stream[scan].tt != end_of_file_tt) {
                if (stream[scan].tt == greater_than_operator_tt && scan + 1 < stream.size() && stream[scan + 1].tt == left_paren_tt) {
                    found_generic_call = true;
                    break;
                }
                scan++;
            }

            if (found_generic_call) {
                auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);

                if (first_semicolon_pos == -1) {
                    std::size_t err_scan = pos;
                    while (err_scan < stream.size() && stream[err_scan].tt != right_paren_tt && stream[err_scan].tt != end_of_file_tt) {
                        ++err_scan;
                    }
                    const auto& err_tok = (err_scan + 1 < stream.size()) ? stream[err_scan + 1] : stream[err_scan];
                    throw parsing_error(";", err_tok, pos, std::source_location::current().function_name());
                }
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

                throw parsing_error("a valid generic function call statement", peek(), pos, std::source_location::current().function_name());
            }
        }
        if (match(peek(), identifier_tt) && peek(1).tt == scope_resolution_tt) {
            auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
            if (first_semicolon_pos == -1) {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }
            std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
            pos += first_semicolon_pos;
            auto converted_rpn = parse_expression(sub_stream);

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            if (converted_rpn.size() == 1) {
                return std::move(converted_rpn.at(0));
            }

            auto wrapper = cst::new_node<cst_root>();
            for (auto& n : converted_rpn) {
                wrapper->add_child(std::move(n));
            }
            return wrapper;
        }
        if (match(peek(), left_paren_tt)) {
            auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
            if (first_semicolon_pos == -1) {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }
            std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
            auto find_assign = [](const std::vector<token_t>& toks) -> std::ptrdiff_t {
                int depth = 0;
                for (std::size_t k = 0; k < toks.size(); k++) {
                    switch (toks[k].tt) {
                    case left_paren_tt:
                    case left_bracket_tt:
                    case left_curly_bracket_tt: depth++; break;
                    case right_paren_tt:
                    case right_bracket_tt:
                    case right_curly_bracket_tt: depth--; break;
                    case assignment_tt:
                        if (depth == 0) return static_cast<std::ptrdiff_t>(k);
                        break;
                    default: break;
                    }
                }
                return -1;
            };
            const std::ptrdiff_t ap = find_assign(sub_stream);
            std::vector<std::unique_ptr<cst>> converted_rpn;
            std::vector<std::unique_ptr<cst>> rhs_rpn;
            if (ap < 0) {
                converted_rpn = parse_expression(sub_stream);
            }
            else {
                std::vector<token_t> lhs_tokens(sub_stream.begin(), sub_stream.begin() + ap);
                std::vector<token_t> rhs_tokens(sub_stream.begin() + ap + 1, sub_stream.end());
                converted_rpn = parse_expression(lhs_tokens);
                rhs_rpn = parse_expression(rhs_tokens);
            }
            pos += first_semicolon_pos;
            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }
            auto is_access_root3 = [](cst* n) {
                return n && (n->get_type() == cst_type::memberaccess
                             || n->get_type() == cst_type::arrayaccess);
            };
            if (converted_rpn.size() == 1 && is_access_root3(converted_rpn.front().get())) {
                if (rhs_rpn.empty()) {
                    return std::move(converted_rpn.front());
                }
                auto lhs = std::move(converted_rpn.front());
                auto assign = cst::new_node<cst_assignment>();
                for (auto& n : rhs_rpn) {
                    assign->add_child(std::move(n));
                }
                lhs->add_child(std::move(assign));
                return lhs;
            }
            throw parsing_error("a valid access or call statement", peek(), pos, std::source_location::current().function_name());
        }
        if (match(peek(), identifier_tt) && peek(1).tt == left_paren_tt) { // fn call
            auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);

            if (first_semicolon_pos == -1) {

                std::size_t scan = pos;
                while (scan < stream.size() && stream[scan].tt != right_paren_tt && stream[scan].tt != end_of_file_tt) {
                    ++scan;
                }

                const auto& err_tok = (scan + 1 < stream.size()) ? stream[scan + 1] : stream[scan];
                throw parsing_error(";", err_tok, pos, std::source_location::current().function_name());
            }
            std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};
            auto find_top_level_assign = [](const std::vector<token_t>& toks) -> std::ptrdiff_t {
                int depth = 0;
                for (std::size_t k = 0; k < toks.size(); k++) {
                    switch (toks[k].tt) {
                    case left_paren_tt:
                    case left_bracket_tt:
                    case left_curly_bracket_tt: depth++; break;
                    case right_paren_tt:
                    case right_bracket_tt:
                    case right_curly_bracket_tt: depth--; break;
                    case assignment_tt:
                        if (depth == 0) return static_cast<std::ptrdiff_t>(k);
                        break;
                    default: break;
                    }
                }
                return -1;
            };

            const std::ptrdiff_t assign_pos = find_top_level_assign(sub_stream);
            std::vector<std::unique_ptr<cst>> converted_rpn;
            std::vector<std::unique_ptr<cst>> rhs_rpn;
            if (assign_pos < 0) {
                converted_rpn = parse_expression(sub_stream);
            }
            else {
                std::vector<token_t> lhs_tokens(sub_stream.begin(), sub_stream.begin() + assign_pos);
                std::vector<token_t> rhs_tokens(sub_stream.begin() + assign_pos + 1, sub_stream.end());
                converted_rpn = parse_expression(lhs_tokens);
                rhs_rpn = parse_expression(rhs_tokens);
            }
            pos += first_semicolon_pos;

            if (converted_rpn.size() == 1 && converted_rpn.at(0)->content == "start_call") {
                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return std::move(converted_rpn.at(0));
            }

            auto is_access_root = [](cst* n) {
                return n && (n->get_type() == cst_type::memberaccess
                             || n->get_type() == cst_type::arrayaccess);
            };
            if (converted_rpn.size() == 1 && is_access_root(converted_rpn.front().get())) {
                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                if (rhs_rpn.empty()) {
                    return std::move(converted_rpn.front());
                }

                auto lhs = std::move(converted_rpn.front());
                auto assign = cst::new_node<cst_assignment>();
                for (auto& n : rhs_rpn) {
                    assign->add_child(std::move(n));
                }
                lhs->add_child(std::move(assign));
                return lhs;
            }

            throw parsing_error("a valid function call statement", peek(), pos, std::source_location::current().function_name());
        }
        if (match(peek(), identifier_tt) && peek(1).tt == period_tt && peek(2).tt == identifier_tt && peek(3).tt == left_paren_tt) {
            auto first_semicolon_pos = find_first_token(stream.begin() + pos, stream.end(), semicolon_tt);
            if (first_semicolon_pos == -1) {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            std::vector<token_t> sub_stream = {stream.begin() + pos, stream.begin() + pos + first_semicolon_pos + 1};

            // `obj.method().field = v;`: same split-at-`=` dance as the
            // bare-function-call case above. assignment stays out of
            // shunting_yard, so parse the two halves separately.
            auto find_top_level_assign2 = [](const std::vector<token_t>& toks) -> std::ptrdiff_t {
                int depth = 0;
                for (std::size_t k = 0; k < toks.size(); k++) {
                    switch (toks[k].tt) {
                    case left_paren_tt:
                    case left_bracket_tt:
                    case left_curly_bracket_tt: depth++; break;
                    case right_paren_tt:
                    case right_bracket_tt:
                    case right_curly_bracket_tt: depth--; break;
                    case assignment_tt:
                        if (depth == 0) return static_cast<std::ptrdiff_t>(k);
                        break;
                    default: break;
                    }
                }
                return -1;
            };

            const std::ptrdiff_t assign_pos = find_top_level_assign2(sub_stream);
            std::vector<std::unique_ptr<cst>> converted_rpn;
            std::vector<std::unique_ptr<cst>> rhs_rpn;
            if (assign_pos < 0) {
                converted_rpn = parse_expression(sub_stream);
            }
            else {
                std::vector<token_t> lhs_tokens(sub_stream.begin(), sub_stream.begin() + assign_pos);
                std::vector<token_t> rhs_tokens(sub_stream.begin() + assign_pos + 1, sub_stream.end());
                converted_rpn = parse_expression(lhs_tokens);
                rhs_rpn = parse_expression(rhs_tokens);
            }
            pos += first_semicolon_pos;

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            if (converted_rpn.size() == 1 && converted_rpn.at(0)->content == "start_call") {
                return std::move(converted_rpn.at(0));
            }
            auto is_access_root2 = [](cst* n) {
                return n && (n->get_type() == cst_type::memberaccess
                             || n->get_type() == cst_type::arrayaccess);
            };
            if (converted_rpn.size() == 1 && is_access_root2(converted_rpn.front().get())) {
                if (rhs_rpn.empty()) {
                    return std::move(converted_rpn.front());
                }
                auto lhs = std::move(converted_rpn.front());
                auto assign = cst::new_node<cst_assignment>();
                for (auto& n : rhs_rpn) {
                    assign->add_child(std::move(n));
                }
                lhs->add_child(std::move(assign));
                return lhs;
            }
            throw parsing_error("a valid method call statement", peek(), pos, std::source_location::current().function_name());
        }
        if (match(peek(), identifier_tt) && (peek(1).tt == left_bracket_tt || peek(1).tt == period_tt)) {
            auto id = parse_identifier();
            std::unique_ptr<cst> current = std::move(id);

            auto parse_indices_into = [this](cst* arr_node) {
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
                                for (auto index_nodes = parse_expression(index_tokens); auto& n : index_nodes) {
                                    arr_node->add_child(std::move(n));
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
                            for (auto index_nodes = parse_expression(index_tokens); auto& n : index_nodes) {
                                arr_node->add_child(std::move(n));
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
            };

            while (match(peek(), left_bracket_tt) || match(peek(), period_tt)) {
                if (match(peek(), left_bracket_tt)) {
                    consume();
                    auto arr = cst::new_node<cst_arrayaccess>();
                    arr->add_child(std::move(current));
                    parse_indices_into(arr.get());
                    current = std::move(arr);
                }
                else { // period
                    consume();
                    if (!match(peek(), identifier_tt)) {
                        throw parsing_error("identifier after period operator", peek(), pos, std::source_location::current().function_name());
                    }
                    auto field = parse_identifier();
                    if (current->get_type() == cst_type::memberaccess
                        && current->get_children().front()->get_type() == cst_type::identifier) {
                        current->add_child(std::move(field));
                    }
                    else {
                        auto mem = cst::new_node<cst_memberaccess>();
                        mem->add_child(std::move(current));
                        mem->add_child(std::move(field));
                        current = std::move(mem);
                    }
                }
            }

            const bool is_member_root = current->get_type() == cst_type::memberaccess;
            const bool is_array_root  = current->get_type() == cst_type::arrayaccess;

            if (match(peek(), assignment_tt)) {
                auto assignment = parse_assignment();
                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }
                parse_expression_until(assignment.get(), semicolon_tt);
                current->add_child(std::move(assignment));
            }
            else if (is_member_root) {
                auto parse_member_compound_assignment = [&](token_type tt) -> bool {
                    if (!match(peek(), tt)) {
                        return false;
                    }
                    consume();
                    auto assignment = cst::new_node<cst_assignment>();
                    if (match(peek(), semicolon_tt)) {
                        throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                    }
                    parse_expression_until(assignment.get(), semicolon_tt);

                    auto rhs_nodes = std::vector<std::unique_ptr<cst>>();
                    for (auto& child : assignment->get_children()) {
                        rhs_nodes.push_back(std::move(child));
                    }
                    assignment->get_children().clear();

                    auto lhs_clone = current->clone();
                    assignment->add_child(std::move(lhs_clone));
                    for (auto& rhs : rhs_nodes) {
                        assignment->add_child(std::move(rhs));
                    }

                    switch (tt) {
                    case add_assign_tt:    assignment->add_child(cst::new_node<cst_add>("+")); break;
                    case sub_assign_tt:    assignment->add_child(cst::new_node<cst_subtract>("-")); break;
                    case mul_assign_tt:    assignment->add_child(cst::new_node<cst_multiply>("*")); break;
                    case div_assign_tt:    assignment->add_child(cst::new_node<cst_divide>("/")); break;
                    case mod_assign_tt:    assignment->add_child(cst::new_node<cst_modulo>("%")); break;
                    case and_assign_tt:    assignment->add_child(cst::new_node<cst_bitwise_and>("&")); break;
                    case or_assign_tt:     assignment->add_child(cst::new_node<cst_bitwise_or>("|")); break;
                    case xor_assign_tt:    assignment->add_child(cst::new_node<cst_xor>("^")); break;
                    case lshift_assign_tt: assignment->add_child(cst::new_node<cst_bitwise_lshift>("<<")); break;
                    case rshift_assign_tt: assignment->add_child(cst::new_node<cst_bitwise_rshift>(">>")); break;
                    default: break;
                    }

                    current->add_child(std::move(assignment));
                    return true;
                };

                const bool handled_member_compound = parse_member_compound_assignment(add_assign_tt) || parse_member_compound_assignment(sub_assign_tt) || parse_member_compound_assignment(mul_assign_tt) ||
                                                     parse_member_compound_assignment(div_assign_tt) || parse_member_compound_assignment(mod_assign_tt) || parse_member_compound_assignment(and_assign_tt) ||
                                                     parse_member_compound_assignment(or_assign_tt) || parse_member_compound_assignment(xor_assign_tt) || parse_member_compound_assignment(lshift_assign_tt) ||
                                                     parse_member_compound_assignment(rshift_assign_tt);
                (void)handled_member_compound;
            }

            if (match(peek(), semicolon_tt)) {
                consume();
            }
            else {
                throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
            }

            (void)is_array_root;
            return current;
        }
        if (match(peek(), identifier_tt)) {
            auto id = parse_identifier();

            if (match(peek(), assignment_tt)) {
                id->add_child(parse_assignment());

                if (match(peek(), semicolon_tt)) {
                    throw parsing_error("expression (found empty assignment)", peek(), pos, std::source_location::current().function_name());
                }

                parse_expression_until(id.get(),
                                       semicolon_tt); // parse the expression until the semicolon

                if (match(peek(), semicolon_tt)) {
                    consume();
                }
                else {
                    throw parsing_error(";", peek(), pos, std::source_location::current().function_name());
                }

                return id;
            }
            else {
                return parse_compound_assignment_identifier(std::move(id));
            }
        }

        throw parsing_error("a valid statement or keyword", peek(), pos, std::source_location::current().function_name());
    }

    /*
        Returns true if the current position looks like a forward-referenced generic
       struct type in a template context (e.g., child_entry<T>* where child_entry
       isn't defined yet). If true, advances pos past the type reference.
    */

    bool parser::try_parse_forward_generic_struct_type(std::unique_ptr<cst>& out_node) {
        if (!match(peek(), identifier_tt) || cst_generic_type_cache.empty() || pos + 1 >= stream.size() || !match(peek(1), less_than_operator_tt)) {
            return false;
        }

        // check that <...> contains at least one generic type param

        std::size_t scan = pos + 2;
        bool found_generic_param = false;
        bool found_close = false;
        while (scan < stream.size()) {
            if (stream[scan].tt == greater_than_operator_tt || stream[scan].tt == bitwise_rshift_tt) {
                found_close = true;
                break;
            }
            if (stream[scan].tt == identifier_tt && cst_generic_type_cache.contains(stream[scan].lexeme)) {
                found_generic_param = true;
            }
            if (stream[scan].tt == semicolon_tt || stream[scan].tt == left_curly_bracket_tt) {
                break;
            }
            scan++;
        }

        if (!found_generic_param || !found_close) {
            return false;
        }

        if (scan + 1 >= stream.size()) {
            return false;
        }
        auto next_tt = stream[scan + 1].tt;
        if (next_tt != multiply_operator_tt && next_tt != identifier_tt && next_tt != right_paren_tt && next_tt != semicolon_tt) {
            return false;
        }

        // consume identifier < type_args > and build struct node
        std::string template_name = peek().lexeme;
        consume(); // consume template name
        consume(); // <

        parse_type_argument_tokens();

        auto node = cst::new_node<cst_struct>();
        node->content = template_name;

        if (match(peek(), multiply_operator_tt)) {
            while (match(peek(), multiply_operator_tt)) {
                consume();
                node->num_pointers++;
            }
        }

        out_node = std::move(node);
        return true;
    }

    void parser::synchronize(const parsing_error& e) {
        const auto& tk = e.get_token();

        std::cerr << RED << "[PARSE ERROR] " << RESET << e.what() << '\n';

        if (!source_lines.empty() && tk.line > 0 && static_cast<std::size_t>(tk.line) <= source_lines.size()) {
            const std::string& src_line = source_lines[static_cast<std::size_t>(tk.line) - 1];

            const std::string gutter = "  " + std::to_string(tk.line) + " | ";
            std::cout << gutter << src_line << '\n';

            const std::size_t col_offset = (tk.column > 0 ? tk.column - 1 : 0);
            const std::size_t spaces = gutter.size() + col_offset;
            const std::size_t tildes = (tk.lexeme.size() > 1 ? tk.lexeme.size() - 1 : 0);
            std::cerr << YELLOW << std::string(spaces, ' ') << '^' << std::string(tildes, '~') << RESET << '\n';
        }

        while (!match(peek(), end_of_file_tt)) {
            if (match(peek(), semicolon_tt)) {
                consume();
                return;
            }

            if (match(peek(), left_curly_bracket_tt)) {
                int depth = 0;
                while (!match(peek(), end_of_file_tt)) {
                    if (match(peek(), left_curly_bracket_tt)) {
                        ++depth;
                    }
                    else if (match(peek(), right_curly_bracket_tt)) {
                        --depth;
                        if (depth == 0) {
                            consume(); // consume closing }
                            return;
                        }
                    }
                    consume();
                }
                return;
            }

            if (match(peek(), right_curly_bracket_tt)) {
                consume();
                return;
            }

            consume();
        }
    }

    std::vector<std::vector<token_t>> parser::parse_type_argument_tokens() {
        std::vector<std::vector<token_t>> type_args;
        std::vector<token_t> current_arg;
        int depth = 1;

        while (!match(peek(), end_of_file_tt)) {
            token_t tok = peek();

            if (tok.tt == less_than_operator_tt) {
                depth++;
                current_arg.push_back(tok);
                consume();
                continue;
            }

            if (tok.tt == bitwise_rshift_tt) {
                token_t gt_tok = tok;
                gt_tok.tt = greater_than_operator_tt;
                gt_tok.lexeme = ">";

                for (int close = 0; close < 2; close++) {
                    depth -= 1;
                    if (depth >= 1) {
                        current_arg.push_back(gt_tok);
                    }
                    else {
                        type_args.push_back(current_arg);
                        current_arg.clear();
                        consume(); // consume >> token once
                        return type_args;
                    }
                }

                consume(); // consume >> token once
                continue;
            }

            if (tok.tt == greater_than_operator_tt) {
                depth -= 1;
                if (depth >= 1) {
                    current_arg.push_back(tok);
                    consume();
                    continue;
                }

                type_args.push_back(current_arg);
                current_arg.clear();
                consume();
                return type_args;
            }

            if (tok.tt == comma_tt && depth == 1) {
                type_args.push_back(current_arg);
                current_arg.clear();
                consume();
                continue;
            }

            current_arg.push_back(tok);
            consume();
        }

        return type_args;
    }

    bool parser::type_args_have_generic_params(const std::vector<std::vector<token_t>>& type_args) const {
        for (const auto& arg_tokens : type_args) {
            for (const auto& tok : arg_tokens) {
                if (tok.tt == identifier_tt && cst_generic_type_cache.contains(tok.lexeme)) {
                    return true;
                }
            }
        }
        return false;
    }

    std::string parser::mangle_type_token_sequence(const std::vector<token_t>& tokens) const {
        std::string mangled_piece;

        for (const auto& tok : tokens) {
            if (tok.tt == comma_tt || tok.tt == less_than_operator_tt || tok.tt == greater_than_operator_tt) {
                continue;
            }

            std::string piece = tok.lexeme;
            if (tok.tt == scope_resolution_tt) {
                piece = "ns";
            }
            else if (tok.tt == multiply_operator_tt) {
                piece = "ptr";
            }
            else if (tok.tt == reference_operator_tt) {
                piece = "ref";
            }

            if (piece.empty()) {
                continue;
            }

            std::string sanitized;
            sanitized.reserve(piece.size());
            for (char ch : piece) {
                sanitized.push_back(std::isalnum(static_cast<unsigned char>(ch)) ? ch : '_');
            }

            if (!sanitized.empty()) {
                if (!mangled_piece.empty()) {
                    mangled_piece.push_back('_');
                }
                mangled_piece += sanitized;
            }
        }

        return mangled_piece;
    }

    std::string parser::mangle_generic_struct_name(const std::string& template_name, const std::vector<std::vector<token_t>>& concrete_type_args) const {
        std::string mangled = template_name;

        for (const auto& arg_tokens : concrete_type_args) {
            auto piece = mangle_type_token_sequence(arg_tokens);
            if (!piece.empty()) {
                mangled.push_back('_');
                mangled += piece;
            }
        }

        return mangled;
    }

    std::string parser::instantiate_generic_struct(const std::string& template_name, const std::vector<std::vector<token_t>>& concrete_type_args) {
        const std::string mangled_name = mangle_generic_struct_name(template_name, concrete_type_args);
        std::string base_template_name = template_name;
        if (auto scope_pos = base_template_name.find_last_of(':'); scope_pos != std::string::npos) {
            base_template_name = base_template_name.substr(scope_pos + 1);
        }
        if (custom_type_map.contains(mangled_name))
            return mangled_name;

        auto& tmpl = generic_struct_templates[template_name];

        std::unordered_map<std::string, std::vector<token_t>> type_map;
        for (std::size_t i = 0; i < tmpl.type_params.size() && i < concrete_type_args.size(); i++) {
            type_map[tmpl.type_params[i]] = concrete_type_args[i];
        }

        std::vector<token_t> substituted;
        for (auto tok : tmpl.tokens) {
            if (tok.tt == identifier_tt && type_map.contains(tok.lexeme)) {
                const auto& mapped = type_map[tok.lexeme];
                substituted.insert(substituted.end(), mapped.begin(), mapped.end());
            }
            else if (tok.tt == identifier_tt && (tok.lexeme == template_name || tok.lexeme == base_template_name)) {
                tok.lexeme = mangled_name;
                substituted.push_back(tok);
            }
            else {
                substituted.push_back(tok);
            }
        }
        substituted.emplace_back(0, 0, "", end_of_file_tt);

        auto saved_stream = std::move(stream);
        auto saved_pos = pos;
        auto saved_cache = std::move(cst_generic_type_cache);

        stream = std::move(substituted);
        pos = 0;
        cst_generic_type_cache.clear();

        std::unique_ptr<cst_struct> struct_node = parse_struct();

        stream = std::move(saved_stream);
        pos = saved_pos;
        cst_generic_type_cache = std::move(saved_cache);

        root->add_child(std::move(struct_node));

        instantiate_methods_for_struct(template_name, mangled_name, concrete_type_args);

        return mangled_name;
    }

    // When a generic struct is instantiated, instantiate every `fn Name::method(...)`
    // template whose receiver is that struct, substituting the struct's concrete
    // type args into the method body. The method lands in func_map under the
    // mangled struct's name (e.g. `Adder_float64::get`) so a later `a.get()` call
    // on an `Adder<f64>` resolves through the ordinary method-dispatch path.
    void parser::instantiate_methods_for_struct(const std::string& struct_template_name,
                                                 const std::string& new_struct_name,
                                                 const std::vector<std::vector<token_t>>& concrete_type_args) {
        const std::string prefix = struct_template_name + "::";

        std::vector<std::string> matching_templates;
        for (const auto& [fn_name, _] : generic_func_templates) {
            if (fn_name.size() > prefix.size() && fn_name.compare(0, prefix.size(), prefix) == 0) {
                matching_templates.push_back(fn_name);
            }
        }

        for (const auto& template_name : matching_templates) {
            const std::string leaf = template_name.substr(prefix.size());
            const std::string new_fn_name = new_struct_name + "::" + leaf;

            if (instantiated_generics.contains(new_fn_name))
                continue;
            instantiated_generics.insert(new_fn_name);

            auto& tmpl = generic_func_templates[template_name];

            std::unordered_map<std::string, std::vector<token_t>> type_map;
            for (std::size_t i = 0; i < tmpl.type_params.size() && i < concrete_type_args.size(); i++) {
                type_map[tmpl.type_params[i]] = concrete_type_args[i];
            }

            std::vector<token_t> substituted;
            for (std::size_t i = 0; i < tmpl.tokens.size(); i++) {
                const auto& tok = tmpl.tokens[i];

                if (tok.tt == identifier_tt && type_map.contains(tok.lexeme)) {
                    const auto& mapped = type_map[tok.lexeme];
                    substituted.insert(substituted.end(), mapped.begin(), mapped.end());
                    continue;
                }

                if (tok.tt == identifier_tt && tok.lexeme == struct_template_name) {
                    auto renamed = tok;
                    renamed.lexeme = new_struct_name;
                    substituted.push_back(renamed);

                    // Skip a following `<...>` so `Adder<T>` collapses to the
                    // concrete `Adder_float64` instead of being left as a
                    // generic instance that parse_datatype would retry.
                    if (i + 1 < tmpl.tokens.size() && tmpl.tokens[i + 1].tt == less_than_operator_tt) {
                        int depth = 1;
                        i += 2;
                        while (i < tmpl.tokens.size() && depth > 0) {
                            const auto& inner = tmpl.tokens[i];
                            if (inner.tt == less_than_operator_tt) depth++;
                            else if (inner.tt == greater_than_operator_tt) depth--;
                            else if (inner.tt == bitwise_rshift_tt) depth -= 2;
                            if (depth > 0) i++;
                        }
                    }
                    continue;
                }

                substituted.push_back(tok);
            }
            substituted.emplace_back(0, 0, "", end_of_file_tt);

            auto saved_stream = std::move(stream);
            auto saved_pos = pos;
            auto saved_cache = std::move(cst_generic_type_cache);

            stream = std::move(substituted);
            pos = 0;
            cst_generic_type_cache.clear();

            auto func_node = parse_function();

            stream = std::move(saved_stream);
            pos = saved_pos;
            cst_generic_type_cache = std::move(saved_cache);

            root->add_child(std::move(func_node));
        }
    }

    void parser::instantiate_generic_function(const std::string& template_name, const std::vector<std::vector<token_t>>& concrete_type_args) {
        std::string mangled_name = template_name;
        for (const auto& arg_tokens : concrete_type_args) {
            auto piece = mangle_type_token_sequence(arg_tokens);
            if (!piece.empty()) {
                mangled_name += "_" + piece;
            }
        }

        if (instantiated_generics.contains(mangled_name))
            return;
        instantiated_generics.insert(mangled_name);

        auto& tmpl = generic_func_templates[template_name];

        std::unordered_map<std::string, std::vector<token_t>> type_map;
        for (std::size_t i = 0; i < tmpl.type_params.size() && i < concrete_type_args.size(); i++) {
            type_map[tmpl.type_params[i]] = concrete_type_args[i];
        }

        std::vector<token_t> substituted;
        bool name_mangled = false;
        for (const auto& tok : tmpl.tokens) {
            if (tok.tt == identifier_tt && type_map.contains(tok.lexeme)) {
                const auto& mapped = type_map[tok.lexeme];
                substituted.insert(substituted.end(), mapped.begin(), mapped.end());
            }
            else if (!name_mangled && tok.tt == identifier_tt && tok.lexeme == template_name) {
                auto mangled_tok = tok;
                mangled_tok.lexeme = mangled_name;
                substituted.push_back(mangled_tok);
                name_mangled = true;
            }
            else {
                substituted.push_back(tok);
            }
        }
        substituted.emplace_back(0, 0, "", end_of_file_tt);

        auto saved_stream = std::move(stream);
        auto saved_pos = pos;
        auto saved_cache = std::move(cst_generic_type_cache);

        stream = std::move(substituted);
        pos = 0;
        cst_generic_type_cache.clear();

        auto func_node = parse_function();

        stream = std::move(saved_stream);
        pos = saved_pos;
        cst_generic_type_cache = std::move(saved_cache);

        root->add_child(std::move(func_node));
    }

    std::vector<token_t> parser::preprocess_generic_calls(const std::vector<token_t>& expr) {
        std::vector<token_t> result;

        for (std::size_t j = 0; j < expr.size(); j++) {
            // `Type<T>::method(...)` — a factory-style call that names the
            // instantiated struct up front. Rewritten to
            // `MangledType::method(...)` so the normal call path takes over.
            if (expr[j].tt == identifier_tt && generic_struct_templates.contains(expr[j].lexeme)
                && j + 1 < expr.size() && expr[j + 1].tt == less_than_operator_tt) {
                std::size_t s = j + 2;
                int depth = 1;
                std::vector<std::vector<token_t>> type_args;
                std::vector<token_t> current_arg;
                bool ended = false;
                while (s < expr.size() && !ended) {
                    const auto& tok = expr[s];
                    if (tok.tt == less_than_operator_tt) {
                        depth++;
                        current_arg.push_back(tok);
                        s++;
                        continue;
                    }
                    if (tok.tt == greater_than_operator_tt) {
                        depth--;
                        if (depth == 0) {
                            type_args.push_back(current_arg);
                            ended = true;
                            continue;
                        }
                        current_arg.push_back(tok);
                        s++;
                        continue;
                    }
                    if (tok.tt == bitwise_rshift_tt) {
                        depth -= 2;
                        if (depth <= 0) {
                            type_args.push_back(current_arg);
                            ended = true;
                            continue;
                        }
                        current_arg.push_back(tok);
                        s++;
                        continue;
                    }
                    if (tok.tt == comma_tt && depth == 1) {
                        type_args.push_back(current_arg);
                        current_arg.clear();
                        s++;
                        continue;
                    }
                    current_arg.push_back(tok);
                    s++;
                }

                if (ended && s + 2 < expr.size()
                    && expr[s + 1].tt == scope_resolution_tt
                    && expr[s + 2].tt == identifier_tt) {

                    const std::string mangled = instantiate_generic_struct(expr[j].lexeme, type_args);

                    token_t type_tok = expr[j];
                    type_tok.lexeme = mangled;
                    result.push_back(type_tok);
                    result.push_back(expr[s + 1]); // `::`
                    result.push_back(expr[s + 2]); // method name

                    j = s + 2; // next iter resumes after method name
                    continue;
                }
            }

            bool is_module_qualified = false;
            std::string module_prefix_str;
            std::size_t func_idx = j;

            if (expr[j].tt == identifier_tt && j + 2 < expr.size() && expr[j + 1].tt == scope_resolution_tt && expr[j + 2].tt == identifier_tt && j + 3 < expr.size() && expr[j + 3].tt == less_than_operator_tt) {
                module_prefix_str = expr[j].lexeme + "::";
                func_idx = j + 2;
                is_module_qualified = true;
            }

            std::size_t check_idx = is_module_qualified ? func_idx : j;
            bool is_generic_pattern = (expr[check_idx].tt == identifier_tt && check_idx + 1 < expr.size() && expr[check_idx + 1].tt == less_than_operator_tt);

            if (is_generic_pattern) {
                // `@name<T>` takes the address of an instantiation (no call parens)
                const bool address_of_generic = j > 0 && expr[j - 1].tt == reference_operator_tt && generic_func_templates.contains(expr[check_idx].lexeme);

                // scan ahead to find > followed by (
                std::size_t scan = check_idx + 2;
                bool looks_like_generic_call = false;
                while (scan < expr.size()) {
                    if ((expr[scan].tt == greater_than_operator_tt || expr[scan].tt == bitwise_rshift_tt) &&
                        (address_of_generic || (scan + 1 < expr.size() && expr[scan + 1].tt == left_paren_tt))) {
                        looks_like_generic_call = true;
                        break;
                    }
                    if (expr[scan].tt == semicolon_tt || expr[scan].tt == left_curly_bracket_tt) {
                        break;
                    }
                    scan++;
                }

                if (looks_like_generic_call) {
                    auto func_name_tok = expr[check_idx];
                    std::string bare_name = func_name_tok.lexeme;

                    std::size_t scan = check_idx + 2; // position after '<'
                    int depth = 1;
                    std::vector<std::vector<token_t>> type_args;
                    std::vector<token_t> current_arg;
                    bool finished = false;

                    while (scan < expr.size() && !finished) {
                        auto tok = expr[scan];

                        if (tok.tt == less_than_operator_tt) {
                            depth++;
                            current_arg.push_back(tok);
                            scan++;
                            continue;
                        }

                        if (tok.tt == bitwise_rshift_tt) {
                            token_t gt_tok = tok;
                            gt_tok.tt = greater_than_operator_tt;
                            gt_tok.lexeme = ">";

                            for (int close = 0; close < 2; close++) {
                                depth -= 1;
                                if (depth >= 1) {
                                    current_arg.push_back(gt_tok);
                                }
                                else {
                                    type_args.push_back(current_arg);
                                    current_arg.clear();
                                    finished = true;
                                    break;
                                }
                            }

                            if (!finished) {
                                scan++;
                            }
                            continue;
                        }

                        if (tok.tt == greater_than_operator_tt) {
                            depth -= 1;
                            if (depth >= 1) {
                                current_arg.push_back(tok);
                                scan++;
                                continue;
                            }

                            type_args.push_back(current_arg);
                            current_arg.clear();
                            finished = true;
                            continue;
                        }

                        if (tok.tt == comma_tt && depth == 1) {
                            type_args.push_back(current_arg);
                            current_arg.clear();
                            scan++;
                            continue;
                        }

                        current_arg.push_back(tok);
                        scan++;
                    }

                    j = scan; // position at final closing token

                    // check if any type arg is still an unresolved generic type parameter
                    bool has_generic_param = type_args_have_generic_params(type_args);

                    if (has_generic_param) {
                        if (is_module_qualified) {
                            result.push_back(expr[func_idx - 2]); // module name
                            result.push_back(expr[func_idx - 1]); // ::
                        }
                        result.push_back(func_name_tok);
                    }
                    else if (generic_func_templates.contains(bare_name)) {
                        instantiate_generic_function(bare_name, type_args);

                        std::string mangled_name = bare_name;
                        for (const auto& arg_tokens : type_args) {
                            auto piece = mangle_type_token_sequence(arg_tokens);
                            if (!piece.empty()) {
                                mangled_name += "_" + piece;
                            }
                        }

                        if (is_module_qualified) {
                            for (auto& child : root->get_children()) {
                                if (child->get_type() == cst_type::function && !child->get_children().empty()) {
                                    auto* name_node = child->get_children().front().get();
                                    if (name_node->get_type() == cst_type::identifier && name_node->content == mangled_name) {
                                        name_node->content = module_prefix_str + mangled_name;
                                        break;
                                    }
                                }
                            }
                            func_name_tok.lexeme = module_prefix_str + mangled_name;
                        }
                        else {
                            func_name_tok.lexeme = mangled_name;
                        }
                        result.push_back(func_name_tok);
                    }
                    else {
                        if (is_module_qualified) {
                            result.push_back(expr[func_idx - 2]);
                            result.push_back(expr[func_idx - 1]);
                        }
                        result.push_back(func_name_tok);
                    }
                    continue;
                }
            }

            result.push_back(expr[j]);
        }

        return result;
    }

    std::unordered_map<std::string, cst*> parser::get_custom_type_map() const { return custom_type_map; }

    void parser::import_generic_templates(const std::unordered_map<std::string, generic_template>& struct_templates, const std::unordered_map<std::string, generic_template>& func_templates,
                                          const std::unordered_set<std::string>& instantiated, const std::unordered_map<std::string, cst*>& types,
                                          const std::unordered_map<std::string, std::unordered_map<std::string, std::int64_t>>& enums) {
        for (const auto& [k, v] : struct_templates) {
            if (!generic_struct_templates.contains(k)) {
                generic_struct_templates[k] = v;
            }
        }
        for (const auto& [k, v] : func_templates) {
            if (!generic_func_templates.contains(k)) {
                generic_func_templates[k] = v;
            }
        }
        for (const auto& s : instantiated) {
            instantiated_generics.insert(s);
        }
        for (const auto& [k, v] : types) {
            if (!custom_type_map.contains(k)) {
                custom_type_map[k] = v;
            }
        }
        for (const auto& [k, v] : enums) {
            if (!enum_definitions.contains(k)) {
                enum_definitions[k] = v;
            }
        }
    }

    std::unique_ptr<cst_root> parser::parse() {
        while (!match(peek(), end_of_file_tt)) {
            try {
                if (auto node = parse_keyword(true); node->get_type() == cst_type::root) {
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
                ++error_count;

                e.set_context(source_file_path);
                synchronize(e);
            }
            catch (const std::exception& e) {
                parser_state = state::failed;
                ++error_count;
                const auto& tk = peek();
                std::cerr << RED << "[PARSE ERROR] " << RESET;
                if (!source_file_path.empty()) {
                    std::cout << source_file_path << ':';
                }
                std::cout << tk.line << ':' << tk.column << ": internal parser error: " << e.what() << '\n';
                if (!match(peek(), end_of_file_tt)) {
                    consume();
                }
            }
        }

        return std::move(root);
    }
} // namespace occult
