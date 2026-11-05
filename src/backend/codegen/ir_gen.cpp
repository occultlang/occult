#include "ir_gen.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <unordered_set>
#include "../../lexer/number_parser.hpp"

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
    ir_function ir_gen::generate_function(cst_function* func_node) {
        ir_function function;
        function.library = func_node->library;
        propagates = false;
        try_dispatch_stack.clear();
        try_finally_stack.clear();
        defer_bodies.clear();

        for (const auto& c : func_node->get_children()) {
            // a function returning a fn-pointer alias (BinOp = fn(..)ret)
            // carries the pointer signature on its return-type cst node.
            // record it so a chained call `fn()(args)` can find the
            // outer call's parameter types without re-parsing the alias.
            if (!c->fnptr_sig.empty()
                && c->get_type() != cst_type::functionarguments
                && c->get_type() != cst_type::identifier) {
                function.return_fnptr_sig = c->fnptr_sig;
            }
            switch (c->get_type()) {
            case cst_type::int8_datatype:
            case cst_type::int16_datatype:
            case cst_type::int32_datatype:
            case cst_type::int64_datatype:
            case cst_type::uint8_datatype:
            case cst_type::uint16_datatype:
            case cst_type::uint32_datatype:
            case cst_type::uint64_datatype:
            case cst_type::string_datatype:
                {
                    function.type = c->to_string().substr(4, c->to_string().size());

                    break;
                }
            case cst_type::bool_datatype:
                {
                    function.type = "bool";

                    break;
                }
            case cst_type::float32_datatype:
                {
                    function.type = "float32";

                    break;
                }
            case cst_type::float64_datatype:
                {
                    function.type = "float64";

                    break;
                }
            case cst_type::structure:
                {
                    function.type = c->content;
                    if (c->num_pointers > 0) {
                        function.type += kStructPtrSuffix;
                    }

                    break;
                }
            case cst_type::functionarguments:
                {
                    generate_function_args(function, cst::cast_raw<cst_functionargs>(c.get()));

                    break;
                }
            case cst_type::identifier:
                {
                    function.name = c->content;

                    break;
                }
            case cst_type::func_uses_shellcode:
                {
                    function.uses_shellcode = true;

                    break;
                }
            case cst_type::func_uses_asm:
                {
                    function.uses_assembly = true;

                    break;
                }
            case cst_type::func_is_external:
                {
                    function.is_external = true;

                    break;
                }
            case cst_type::asm_code:
                {
                    for (auto& str_literal : c->get_children()) {
                        if (str_literal->get_type() == cst_type::stringliteral) {
                            function.code.emplace_back(op_asm_code, str_literal->content, c->content); // type = architecture
                        }
                        else {
                            std::cerr << RED << "[IR ERR] Expected string literal in assembly block.\n" << RESET;

                            break;
                        }
                    }

                    break;
                }
            case cst_type::shellcode:
                {
                    for (auto& num_literal : c->get_children()) {
                        if (num_literal->get_type() == cst_type::number_literal) {
                            function.code.emplace_back(op_push_shellcode, from_numerical_string<std::uint8_t>(num_literal->content));
                        }
                        else {
                            std::cerr << RED << "[IR ERR] Expected number literal in shellcode.\n" << RESET;

                            break;
                        }
                    }

                    break;
                }
            case cst_type::block:
                {
                    generate_block(function, cst::cast_raw<cst_block>(c.get()));

                    break;
                }
            default:
                {
                    break;
                }
            }
        }

        emit_defers(function);

        if (propagates) {
            place_label(function, "__oc_prop");
            emit_defers(function);
            if (function.name == "main") {
                function.code.emplace_back(op_call, std::string("__occult_rt::uncaught"), std::string(""), false);
            }
            function.code.emplace_back(op_propagate);
            propagates = false;
        }

        defer_bodies.clear();

        func_map.emplace(function.name, function);

        return function;
    }

    void ir_gen::generate_try(ir_function& function, cst* try_node, std::string current_break_label, std::string current_loop_start) {
        if (try_node->get_children().empty()) {
            throw std::runtime_error("try with no body, at " + try_node->origin);
        }
        auto* body = cst::cast_raw<cst_block>(try_node->get_children()[0].get());
        std::vector<cst*> clauses;
        cst* finally_body = nullptr;
        for (std::size_t i = 1; i < try_node->get_children().size(); ++i) {
            auto* ch = try_node->get_children()[i].get();
            if (ch->get_type() == cst_type::catchclause) {
                clauses.push_back(ch);
            }
            else if (ch->get_type() == cst_type::finallyblock && !ch->get_children().empty()) {
                finally_body = ch->get_children().front().get();
            }
        }
        if (clauses.empty() && finally_body == nullptr) {
            throw std::runtime_error("try needs catch or finally, at " + try_node->origin);
        }
        const bool has_finally = finally_body != nullptr;

        const std::string base = try_node->content.empty() ? ("__oc_try_" + std::to_string(function.code.size())) : try_node->content;
        const std::string dispatch_label = base + "_dispatch";
        const std::string finally_label = base + "_finally";
        const std::string end_label = base + "_end";

        std::string exit_var, ret_var;
        std::unordered_map<std::string, std::int64_t> goto_codes;
        if (has_finally) {
            // tracks which exit the covered code took, so the finally
            // dispatcher can take the real exit afterwards:
            // Exit kinds: 0 fall-through, 1 return, 2 break, 3 continue, 4+ goto.
            exit_var = base + "_exit";
            ret_var = base + "_ret";
            local_variable_map[function][exit_var] = "int64";
            local_variable_map[function][ret_var] = "int64";
            function.code.emplace_back(op_push, std::int64_t(0));
            function.code.emplace_back(op_store, exit_var);
            function.code.emplace_back(op_push, std::int64_t(0));
            function.code.emplace_back(op_store, ret_var);
            try_finally_stack.push_back({exit_var, ret_var, finally_label, current_break_label, current_loop_start, &goto_codes});
        }

        try_dispatch_stack.push_back(dispatch_label);
        generate_block(function, body, current_break_label, current_loop_start);
        function.code.emplace_back(op_jmp, finally_label);

        place_label(function, dispatch_label);
        std::vector<std::string> body_labels;
        body_labels.reserve(clauses.size());
        for (std::size_t ci = 0; ci < clauses.size(); ++ci) {
            body_labels.push_back(base + "_catch_" + std::to_string(ci));
        }
        bool has_untyped = false;
        for (std::size_t ci = 0; ci < clauses.size(); ++ci) {
            auto* clause = clauses[ci];
            if (clause->type_id == 0) {
                function.code.emplace_back(op_jmp, body_labels[ci]);
                has_untyped = true;
                break;
            }
            function.code.emplace_back(op_load, std::string("__occult_err_type_id"));
            function.code.emplace_back(op_push, static_cast<std::int64_t>(clause->type_id));
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_jz, body_labels[ci]);
        }
        function.code.emplace_back(op_jmp, finally_label);

        for (std::size_t ci = 0; ci < clauses.size(); ++ci) {
            auto* clause = clauses[ci];
            place_label(function, body_labels[ci]);
            if (clause->type_id == 0 && !clause->content.empty()) {
                local_variable_map[function][clause->content] = "int64";
                function.code.emplace_back(op_load, std::string("__occult_err_message"));
                function.code.emplace_back(op_store, clause->content);
            }
            function.code.emplace_back(op_call, std::string("__occult_rt::clear"), std::string(""), false);
            for (auto& k : clause->get_children()) {
                if (k->get_type() == cst_type::block) {
                    generate_block(function, cst::cast_raw<cst_block>(k.get()), current_break_label, current_loop_start);
                    break;
                }
            }
            function.code.emplace_back(op_jmp, finally_label);
        }
        (void)has_untyped;
        if (has_finally) {
            try_finally_stack.pop_back();
        }

        place_label(function, finally_label);
        if (finally_body != nullptr) {
            generate_block(function, cst::cast_raw<cst_block>(finally_body), current_break_label, current_loop_start);
        }
        function.code.emplace_back(op_load, std::string("__occult_err_active"));
        function.code.emplace_back(op_push, std::int64_t(0));
        function.code.emplace_back(op_cmp);
        try_dispatch_stack.pop_back();
        const std::string outer_dispatch = try_dispatch_stack.empty() ? std::string("__oc_prop") : try_dispatch_stack.back();
        function.code.emplace_back(op_error_jump, outer_dispatch);
        if (outer_dispatch == "__oc_prop") {
            propagates = true;
        }

        if (!has_finally) {
            function.code.emplace_back(op_jmp, end_label);
            place_label(function, end_label);
            return;
        }

        const auto* outer = try_finally_stack.empty() ? nullptr : &try_finally_stack.back();

        auto forward_or = [&](std::int64_t kind, std::function<void()> local_action) {
            const std::string skip = create_label();
            function.code.emplace_back(op_load, exit_var);
            function.code.emplace_back(op_push, kind);
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_jnz, skip);
            if (outer != nullptr) {
                if (kind == 1) {
                    function.code.emplace_back(op_load, ret_var);
                    function.code.emplace_back(op_store, outer->ret_var);
                }
                function.code.emplace_back(op_push, kind);
                function.code.emplace_back(op_store, outer->exit_var);
                function.code.emplace_back(op_jmp, outer->finally_label);
            }
            else {
                local_action();
            }
            place_label(function, skip);
        };

        forward_or(1, [&]() {
            function.code.emplace_back(op_load, ret_var);
            function.code.emplace_back(op_ret);
        });
        if (!current_break_label.empty() || outer != nullptr) {
            forward_or(2, [&]() {
                function.code.emplace_back(op_jmp, current_break_label);
            });
        }
        if (!current_loop_start.empty() || outer != nullptr) {
            forward_or(3, [&]() {
                function.code.emplace_back(op_jmp, current_loop_start);
            });
        }
        for (const auto& [name, code] : goto_codes) {
            const std::string skip = create_label();
            function.code.emplace_back(op_load, exit_var);
            function.code.emplace_back(op_push, code);
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_jnz, skip);
            if (outer != nullptr) {
                auto& ocodes = *outer->goto_codes;
                auto it = ocodes.find(name);
                if (it == ocodes.end()) {
                    it = ocodes.emplace(name, static_cast<std::int64_t>(4 + ocodes.size())).first;
                }
                function.code.emplace_back(op_push, it->second);
                function.code.emplace_back(op_store, outer->exit_var);
                function.code.emplace_back(op_jmp, outer->finally_label);
            }
            else {
                function.code.emplace_back(op_jmp, user_label(name));
            }
            place_label(function, skip);
        }

        function.code.emplace_back(op_jmp, end_label);
        place_label(function, end_label);
    }

    void ir_gen::generate_function_args(ir_function& function, cst_functionargs* func_args_node) {
        for (const auto& arg : func_args_node->get_children()) {
            if (arg->get_type() == cst_type::variadic) {
                function.is_variadic = true;
                if (debug) {
                    std::cout << CYAN << "[IR GEN] Function is variadic" << RESET << std::endl;
                }
                continue;
            }

            bool is_ref = false;
            if (arg->content == "reference" || arg->is_reference) {
                is_ref = true;
            }

            for (const auto& c : arg->get_children()) {
                auto type = arg->to_string().substr(4, arg->to_string().size()) + ((is_ref) ? "_reference" : "");

                // for struct types, use the actual struct name from content
                if (arg->get_type() == cst_type::structure) {
                    type = arg->content;
                    if (arg->num_pointers > 0) {
                        type += kStructPtrSuffix;
                    }
                    if (is_ref) {
                        type += "_reference";
                    }
                }

                auto variable_name = c->content;

                if (debug) {
                    std::cout << CYAN << "[IR GEN] Arg (type, name): " << RESET << "(" << type << ", " << variable_name << ")" << std::endl;
                }

                function.args.emplace_back(variable_name, type);
                // register function arguments in local_variable_map for type tracking
                local_variable_map[function][variable_name] = type;
                if (!arg->fnptr_sig.empty()) {
                    fnptr_sigs[function.name][variable_name] = arg->fnptr_sig;
                }
            }
        }
    }

    void ir_gen::generate_arith_and_bitwise_operators(ir_function& function, cst* c, std::optional<std::string> type) {
        switch (c->get_type()) {
        case cst_type::add_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_addf32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_addf64);
                }
                else {
                    function.code.emplace_back(op_add);
                }

                break;
            }
        case cst_type::subtract_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_subf32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_subf64);
                }
                else {
                    function.code.emplace_back(op_sub);
                }

                break;
            }
        case cst_type::multiply_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_mulf32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_mulf64);
                }
                else {
                    ir_opcode to_push = (type.has_value() && is_signed[type.value()]) ? op_imul : op_mul;
                    function.code.emplace_back(to_push);
                }

                break;
            }
        case cst_type::division_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_divf32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_divf64);
                }
                else {
                    ir_opcode to_push = (type.has_value() && is_signed[type.value()]) ? op_idiv : op_div;
                    function.code.emplace_back(to_push);
                }

                break;
            }
        case cst_type::modulo_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_modf32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_modf64);
                }
                else {
                    ir_opcode to_push = (type.has_value() && is_signed[type.value()]) ? op_imod : op_mod;
                    function.code.emplace_back(to_push);
                }

                break;
            }
        case cst_type::bitwise_and:
            {
                function.code.emplace_back(op_bitwise_and);

                break;
            }
        case cst_type::bitwise_or:
            {
                function.code.emplace_back(op_bitwise_or);

                break;
            }
        case cst_type::xor_operator:
            {
                function.code.emplace_back(op_bitwise_xor);

                break;
            }
        case cst_type::bitwise_lshift:
            {
                function.code.emplace_back(op_bitwise_lshift);

                break;
            }
        case cst_type::bitwise_rshift:
            {
                ir_opcode to_push = (type.has_value() && is_signed.count(type.value()) && is_signed[type.value()]) ? op_ibitwise_rshift : op_bitwise_rshift;
                function.code.emplace_back(to_push);

                break;
            }
        case cst_type::unary_plus_operator:
            {
                // just purely syntax
                break;
            }
        case cst_type::unary_minus_operator:
            {
                if (type.has_value() && type.value() == "float32") {
                    function.code.emplace_back(op_negatef32);
                }
                else if (type.has_value() && type.value() == "float64") {
                    function.code.emplace_back(op_negatef64);
                }
                else {
                    function.code.emplace_back(op_negate);
                }

                break;
            }
        case cst_type::unary_bitwise_not:
            {
                function.code.emplace_back(op_bitwise_not);

                break;
            }
        case cst_type::unary_not_operator:
            {
                // !x is x == 0. Postfix, so x is already on the stack; nothing
                // emitted anything here before, and i64(!true) came out as 1
                function.code.emplace_back(op_push, std::int64_t(0));
                function.code.emplace_back(op_cmp);
                function.code.emplace_back(op_setz);

                break;
            }
        default:
            {
                break;
            }
        }
    }

    void ir_gen::emit_comparison(ir_function& function, std::optional<std::string> type) {
        if (type.has_value()) {
            if (type.value() == "float32") {
                function.code.emplace_back(op_cmpf32);
            }
            else if (type.value() == "float64") {
                function.code.emplace_back(op_cmpf64);
            }
            else {
                function.code.emplace_back(op_cmp);
            }
        }
        else {
            function.code.emplace_back(op_cmp);
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

    static std::optional<std::string> float_deref_type(cst* node) {
        bool has_deref = false;
        std::optional<std::string> t;
        for (const auto& c : node->get_children()) {
            if (c->get_type() == cst_type::dereference) {
                has_deref = true;
            }
            if (c->get_type() == cst_type::cast_to_datatype && c->num_pointers > 0) {
                if (c->content == "f64") t = "float64";
                else if (c->content == "f32") t = "float32";
                else return std::nullopt;
            }
        }
        return has_deref ? t : std::nullopt;
    }

    std::optional<std::string> ir_gen::member_access_type(ir_function& function, cst* node) {
        const auto& kids = node->get_children();
        if (kids.size() < 2) {
            return std::nullopt;
        }
        auto fn_it = local_variable_map.find(function);
        if (fn_it == local_variable_map.end()) {
            return std::nullopt;
        }
        auto var_it = fn_it->second.find(kids[0]->content);
        if (var_it == fn_it->second.end()) {
            return std::nullopt;
        }
        std::string cur = var_it->second;
        if (cur.ends_with(kStructPtrSuffix)) {
            cur = cur.substr(0, cur.size() - std::char_traits<char>::length(kStructPtrSuffix));
        }
        if (cur.ends_with("_reference")) {
            cur = cur.substr(0, cur.size() - 10);
        }

        std::optional<std::string> last;
        for (std::size_t ci = 1; ci < kids.size(); ci++) {
            auto st = custom_type_map.find(cur);
            if (st == custom_type_map.end()) {
                return std::nullopt;
            }
            last = std::nullopt;
            for (const auto& mc : st->second->get_children()) {
                if (mc->get_children().empty() || mc->get_children().front()->content != kids[ci]->content) {
                    continue;
                }
                switch (mc->get_type()) {
                case cst_type::float32_datatype: last = "float32"; break;
                case cst_type::float64_datatype: last = "float64"; break;
                case cst_type::structure:        cur = mc->content; break;
                default:                         last = std::nullopt; break;
                }
                break;
            }
        }
        return last;
    }

    void ir_gen::generate_boolean_value(ir_function& function, cst* node, type_of_push type_push) {
        bool has_logical = false;
        for (const auto& c : node->get_children()) {
            // only && and || need branching for short-circuit evaluation;
            // simple comparisons (==, !=, >, <, >=, <=) use cmp+setcc which
            // avoids register pool corruption from label-based branching
            if (c->get_type() == cst_type::and_operator || c->get_type() == cst_type::or_operator) {
                has_logical = true;

                if (debug) {
                    std::cout << CYAN << "[IR GEN] Generating boolean value for logical operator.\n" << RESET;
                }

                break;
            }
        }

        std::optional<std::string> expr_type = float_deref_type(node);
        for (const auto& c : node->get_children()) {
            if (expr_type.has_value()) {
                break;
            }
            if (c->get_type() == cst_type::float_literal) {
                expr_type = "float64"; // default
                break;
            }
            else if (c->get_type() == cst_type::memberaccess) {
                if (auto t = member_access_type(function, c.get()); t.has_value()) {
                    expr_type = t;
                    break;
                }
            }
            else if (c->get_type() == cst_type::identifier) {
                if (local_variable_map.find(function) != local_variable_map.end()) {
                    auto& var_map = local_variable_map[function];
                    if (var_map.find(c->content) != var_map.end()) {
                        expr_type = var_map[c->content];
                        break;
                    }
                }
            }
        }

        if (!has_logical) {
            for (const auto& c : node->get_children()) {
                switch (c->get_type()) {
                case cst_type::number_literal:
                    handle_push_types(function, c.get(), expr_type);
                    break;
                case cst_type::float_literal:
                    handle_push_types(function, c.get(), expr_type);
                    break;
                case cst_type::charliteral:
                    {
                        handle_push_types(function, c.get(), expr_type);
                        break;
                    }
                case cst_type::identifier:
                    function.code.emplace_back(op_load, c->content);
                    break;
                case cst_type::functioncall:
                    generate_function_call(function, c.get());
                    break;
                case cst_type::arrayaccess:
                    generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
                    break;
                case cst_type::memberaccess:
                    generate_member_access(function, cst::cast_raw<cst_memberaccess>(c.get()));
                    break;
                case cst_type::stringliteral:
                    function.code.emplace_back(op_push, c->content);
                    break;
                case cst_type::cast_to_datatype:
                    generate_cast<std::int64_t>(function, c.get());
                    break;
                case cst_type::dereference:
                    function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));
                    break;
                case cst_type::reference:
                    function.code.emplace_back(op_reference);
                    break;
                case cst_type::sizeof_expr:
                    function.code.emplace_back(op_sizeof, c->content);
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
        case type_of_push::ret:
            {
                if (debug) {
                    std::cout << BLUE << "[IR GEN] Push type: ret\n" << RESET;
                }
                function.code.emplace_back(op_push_for_ret, std::int64_t(1));
                break;
            }
        case type_of_push::single:
            {
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
        case type_of_push::ret:
            {
                if (debug) {
                    std::cout << BLUE << "[IR GEN] Push type: ret\n" << RESET;
                }
                function.code.emplace_back(op_push_for_ret, std::int64_t(0));
                break;
            }
        case type_of_push::single:
            {
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
    void ir_gen::generate_cast(ir_function& function, cst* c) {
        std::string src_type;

        auto is_comparison = [](cst_type t) {
            switch (t) {
            case cst_type::equals_operator:
            case cst_type::not_equals_operator:
            case cst_type::greater_than_operator:
            case cst_type::less_than_operator:
            case cst_type::greater_than_or_equal:
            case cst_type::less_than_or_equal:
            case cst_type::greater_than_or_equal_operator:
            case cst_type::less_than_or_equal_operator:
            case cst_type::and_operator:
            case cst_type::or_operator:
            case cst_type::unary_not_operator:
                return true;
            default:
                return false;
            }
        };
        for (const auto& ch : c->get_children()) {
            if (is_comparison(ch->get_type())) {
                generate_boolean_value(function, c);
                function.code.emplace_back(op_cast, c->content, std::string("int64"));
                return;
            }
        }
        cst* first_value = nullptr;
        for (const auto& ch : c->get_children()) {
            if (ch->get_type() == cst_type::expr_end || ch->get_type() == cst_type::expr_start) {
                continue;
            }
            first_value = ch.get();
            break;
        }
        if (first_value) {
            cst* first_child = first_value;
            if (first_child->get_type() == cst_type::identifier) {
                auto var_it = local_variable_map[function].find(first_child->content);
                if (var_it != local_variable_map[function].end()) {
                    src_type = var_it->second;
                }
            }
            else if (first_child->get_type() == cst_type::memberaccess) {
                // a float behind a member access is still a float
                if (auto t = member_access_type(function, first_child); t.has_value()) {
                    src_type = t.value();
                }
            }
            else if (first_child->get_type() == cst_type::float_literal) {
                src_type = "float64";
            }
            else if (first_child->get_type() == cst_type::number_literal) {
                src_type = "int64";
            }
            else if (first_child->get_type() == cst_type::functioncall && !first_child->get_children().empty()) {
                // source type is the callee's return type
                const std::string& callee = first_child->get_children().front()->content;
                // the bitcast intrinsics are built in, so not in the function map
                if (callee == "__bitcast_f64") src_type = "float64";
                else if (callee == "__bitcast_f32") src_type = "float32";
                else if (callee == "__bitcast_i64") src_type = "int64";
                else if (callee == "__bitcast_i32") src_type = "int32";
                if (!src_type.empty()) {
                    // already known
                }
                else if (auto f_it = func_map.find(callee); f_it != func_map.end()) {
                    src_type = f_it->second.type.empty() ? "int64" : f_it->second.type;
                }
                else {
                    // call through a function pointer: return type is the signature's tail
                    std::string fsig;
                    if (auto f = fnptr_sigs.find(function.name); f != fnptr_sigs.end() && f->second.contains(callee)) {
                        fsig = f->second.at(callee);
                    }
                    else if (global_fnptr_sigs.contains(callee)) {
                        fsig = global_fnptr_sigs.at(callee);
                    }
                    if (!fsig.empty()) {
                        int depth = 0;
                        for (std::size_t i = 3; i < fsig.size(); i++) {
                            if (fsig[i] == '(') depth++;
                            if (fsig[i] == ')') {
                                if (depth == 0) {
                                    src_type = fsig.substr(i + 1);
                                    break;
                                }
                                depth--;
                            }
                        }
                    }
                }
            }
            else if (first_child->get_type() == cst_type::cast_to_datatype) {
                // nested cast: source type is the target type of the inner cast
                src_type = first_child->content;
            }
        }

        if (src_type == "f64") src_type = "float64";
        else if (src_type == "f32") src_type = "float32";
        // a dereference of a float pointer in the operand says the operand is one
        if (src_type.empty()) {
            if (auto fd = float_deref_type(c)) {
                src_type = *fd;
            }
        }

        if (src_type == "float32") {
            generate_common_generic<float>(function, c, "float32");
        }
        else if (src_type == "float64") {
            generate_common_generic<double>(function, c, "float64");
        }
        else if (!src_type.empty()) {
            generate_common_generic<std::int64_t>(function, c, "int64");
        }
        else {
            generate_common_generic<IntType>(function, c);
        }

        function.code.emplace_back(op_cast, c->num_pointers > 0 ? std::string("u64") : c->content, src_type);
        if (c->num_pointers > 0) {
            function.code.emplace_back(op_pointee, c->content);
        }
    }

    template <typename IntType>
    void ir_gen::generate_common_generic(ir_function& function, cst* node, std::optional<std::string> type, type_of_push type_push) {
        bool needs_value = false;
        for (const auto& c : node->get_children()) {
            if (c->get_type() == cst_type::equals_operator || c->get_type() == cst_type::not_equals_operator || c->get_type() == cst_type::and_operator || c->get_type() == cst_type::or_operator ||
                c->get_type() == cst_type::greater_than_operator || c->get_type() == cst_type::less_than_operator || c->get_type() == cst_type::greater_than_or_equal_operator || c->get_type() == cst_type::less_than_or_equal_operator) {
                needs_value = true;
                break;
            }
        }

        if (needs_value) {
            generate_boolean_value(function, node, type_push);
            return;
        }

        std::string pending_ptr_pointee;
        for (const auto& c : node->get_children()) {
            switch (c->get_type()) {
            case cst_type::number_literal:
                function.code.emplace_back(op_push, from_numerical_string<IntType>(c->content));
                pending_ptr_pointee.clear();
                break;
            case cst_type::float_literal:
                function.code.emplace_back(op_push, from_numerical_string<IntType>(c->content));
                pending_ptr_pointee.clear();
                break;
            case cst_type::charliteral:
                {
                    std::int64_t char_val = c->content.empty() ? 0 : static_cast<std::int64_t>(static_cast<unsigned char>(c->content[0]));
                    function.code.emplace_back(op_push, char_val);
                    pending_ptr_pointee.clear();
                    break;
                }
            case cst_type::identifier:
                function.code.emplace_back(op_load, c->content);
                pending_ptr_pointee.clear();
                if (auto it = local_variable_map[function].find(c->content);
                    it != local_variable_map[function].end()) {
                    const auto& vt = it->second;
                    if (vt.ends_with("_ptr")) {
                        pending_ptr_pointee = vt.substr(0, vt.size() - 4);
                    }
                }
                break;
            case cst_type::stringliteral:
                function.code.emplace_back(op_push, c->content);
                pending_ptr_pointee.clear();
                break;
            case cst_type::functioncall:
                generate_function_call(function, c.get());
                pending_ptr_pointee.clear();
                break;
            case cst_type::arrayaccess:
                generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
                pending_ptr_pointee.clear();
                break;
            case cst_type::memberaccess:
                generate_member_access(function, cst::cast_raw<cst_memberaccess>(c.get()));
                pending_ptr_pointee.clear();
                break;
            case cst_type::reference:
                function.code.emplace_back(op_reference);
                break;
            case cst_type::dereference:
                if (!pending_ptr_pointee.empty()) {
                    function.code.emplace_back(op_pointee, pending_ptr_pointee);
                    pending_ptr_pointee.clear();
                }
                function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));
                break;
            case cst_type::sizeof_expr:
                function.code.emplace_back(op_sizeof, c->content);
                break;
            case cst_type::cast_to_datatype:
                {
                    generate_cast<IntType>(function, c.get());
                    break;
                }
            default:
                generate_arith_and_bitwise_operators(function, c.get(), (type.has_value() ? type.value() : "int64"));
                break;
            }
        }
    }

    void ir_gen::handle_push_types(ir_function& function, cst* c, std::optional<std::string> type) {
        // char literals are stored as the raw character; push ASCII value directly
        if (c->get_type() == cst_type::charliteral) {
            std::int64_t char_val = c->content.empty() ? 0 : static_cast<std::int64_t>(static_cast<unsigned char>(c->content[0]));
            function.code.emplace_back(op_push, char_val);
            return;
        }

        if (!type.has_value() || type.value().empty()) {
            type = function.type.empty() ? "int64" : function.type;
        }

        if (custom_type_map.contains(type.value())) {
            function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(c->content));
            return;
        }

        if (auto it = ir_typemap.find(type.value()); it != ir_typemap.end()) {
            switch (it->second) {
            case ir_typename::int64:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(c->content));

                    break;
                }
            case ir_typename::int32:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::int32_t>(c->content));

                    break;
                }
            case ir_typename::int16:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::int16_t>(c->content));

                    break;
                }
            case ir_typename::int8:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::int8_t>(c->content));

                    break;
                }
            case ir_typename::boolean:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::int8_t>(c->content));

                    break;
                }
            case ir_typename::uint64:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::uint64_t>(c->content));

                    break;
                }
            case ir_typename::uint32:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::uint32_t>(c->content));

                    break;
                }
            case ir_typename::uint16:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::uint16_t>(c->content));

                    break;
                }
            case ir_typename::uint8:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::uint8_t>(c->content));

                    break;
                }
            case ir_typename::float64:
                {
                    function.code.emplace_back(op_push, from_numerical_string<double>(c->content));

                    break;
                }
            case ir_typename::float32:
                {
                    function.code.emplace_back(op_push, from_numerical_string<float>(c->content));

                    break;
                }
            case ir_typename::string:
                {
                    function.code.emplace_back(op_push, from_numerical_string<std::string>(c->content));

                    break;
                }
            }
        }
        else {
            // assume struct type
            generate_common_generic<std::int64_t>(function, c);
        }
    }

    void ir_gen::generate_common(ir_function& function, cst* c, std::string type, type_of_push type_push) {
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
            case ir_typename::int64:
                {
                    generate_common_generic<std::int64_t>(function, c, "int64", type_push);

                    break;
                }
            case ir_typename::int32:
                {
                    generate_common_generic<std::int32_t>(function, c, "int32", type_push);

                    break;
                }
            case ir_typename::int16:
                {
                    generate_common_generic<std::int16_t>(function, c, "int16", type_push);

                    break;
                }
            case ir_typename::int8:
                {
                    generate_common_generic<std::int8_t>(function, c, "int8", type_push);

                    break;
                }
            case ir_typename::uint64:
                {
                    generate_common_generic<std::uint64_t>(function, c, "uint64", type_push);

                    break;
                }
            case ir_typename::uint32:
                {
                    generate_common_generic<std::uint32_t>(function, c, "uint32", type_push);

                    break;
                }
            case ir_typename::uint16:
                {
                    generate_common_generic<std::uint16_t>(function, c, "uint16", type_push);

                    break;
                }
            case ir_typename::uint8:
                {
                    generate_common_generic<std::uint8_t>(function, c, "uint8", type_push);

                    break;
                }
            case ir_typename::float64:
                {
                    generate_common_generic<double>(function, c, "float64", type_push);

                    break;
                }
            case ir_typename::float32:
                {
                    generate_common_generic<float>(function, c, "float32", type_push);

                    break;
                }
            case ir_typename::string:
                {
                    generate_common_generic<std::string>(function, c, "str", type_push);

                    break;
                }
            case ir_typename::boolean:
                {
                    generate_common_generic<std::int8_t>(function, c, "bool", type_push);

                    break;
                }
            }
        }
        else {
            // assume struct type - emit reference if needed
            if (is_ref) {
                function.code.emplace_back(op_reference);
            }
            generate_common_generic<std::int64_t>(function, c);
        }
    }

    void ir_gen::generate_function_call(ir_function& function, cst* c, bool result_used) {
        generate_call_itself(function, c, result_used);
        if (!c->handler.empty()) {
            function.code.emplace_back(op_load, std::string("__occult_err_active"));
            function.code.emplace_back(op_push, std::int64_t(0));
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_error_jump, c->handler);
            if (c->handler == "__oc_prop") {
                propagates = true;
            }
        }
    }

    void ir_gen::generate_call_itself(ir_function& function, cst* c, bool result_used) {
        const auto node = cst::cast_raw<cst_functioncall>(c);

        // chained call `fn()(args)`: the first child is itself a function
        // call whose return value is the callee for this one. evaluate the
        // inner call into a hidden local, then route through the fn-pointer
        // path with that local as the callee name. the signature comes from
        // the inner callee's declared return type.
        if (!node->get_children().empty()
            && node->get_children().front()->get_type() == cst_type::functioncall) {
            cst* inner = node->get_children().front().get();

            std::string inner_fsig;
            if (!inner->get_children().empty()) {
                cst* inner_name = inner->get_children().front().get();
                if (!inner_name->fnptr_sig.empty()) {
                    inner_fsig = inner_name->fnptr_sig;
                }
                else {
                    const std::string inner_fname = inner_name->content;
                    if (auto it = func_map.find(inner_fname); it != func_map.end()
                        && !it->second.return_fnptr_sig.empty()) {
                        inner_fsig = it->second.return_fnptr_sig;
                    }
                }
            }

            generate_function_call(function, inner, true);

            const std::string hidden = std::string("__call_fp_") + std::to_string(hidden_slot_count++);
            function.code.emplace_back(op_declare_local, hidden, std::string("int64"));
            function.code.emplace_back(op_store, hidden);
            local_variable_map[function][hidden] = "int64";
            if (!inner_fsig.empty()) {
                fnptr_sigs[function.name][hidden] = inner_fsig;
            }

            std::vector<std::string> params;
            if (!inner_fsig.empty()) {
                int depth = 0;
                std::string cur;
                for (std::size_t k = 3; k < inner_fsig.size(); k++) {
                    const char ch = inner_fsig[k];
                    if (ch == '(') depth++;
                    if (ch == ')') {
                        if (depth == 0) break;
                        depth--;
                    }
                    if (ch == ',' && depth == 0) {
                        params.push_back(cur);
                        cur.clear();
                        continue;
                    }
                    cur += ch;
                }
                if (!cur.empty()) params.push_back(cur);
            }

            function.code.emplace_back(op_load, hidden); // callee first for the indirect call

            std::size_t argi = 0;
            auto arg_location = 1;
            while (arg_location < static_cast<int>(node->get_children().size())
                   && node->get_children().at(arg_location).get()->content != "end_call") {
                const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(arg_location).get());
                std::string ptype = argi < params.size() ? params[argi] : "int64";
                if (ptype.starts_with("fn(")) {
                    ptype = "int64";
                }
                generate_common(function, arg_node, ptype);
                ++arg_location;
                ++argi;
            }

            const std::string fsig_for_op = inner_fsig.empty()
                ? std::string("fn(") + std::string(argi, 'i') + std::string(")i") // placeholder
                : inner_fsig;
            function.code.emplace_back(op_call_indirect, fsig_for_op, std::to_string(argi), result_used);
            return;
        }

        const auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get()); // name of call
        std::string resolved_call_name = identifier->content;

        if (!resolved_call_name.empty() && resolved_call_name.front() == '.' && node->get_children().size() > 1) {
            const auto self_arg = cst::cast_raw<cst_functionarg>(node->get_children().at(1).get());
            if (self_arg && !self_arg->get_children().empty() && self_arg->get_children().front()->get_type() == cst_type::identifier) {
                const std::string self_name = self_arg->get_children().front()->content;
                auto self_it = local_variable_map[function].find(self_name);
                if (self_it != local_variable_map[function].end()) {
                    std::string self_type = self_it->second;
                    if (self_type.ends_with("_ptr")) {
                        self_type = self_type.substr(0, self_type.size() - 4);
                    }
                    if (self_type.ends_with("_reference")) {
                        self_type = self_type.substr(0, self_type.size() - 10);
                    }
                    std::string method_suffix = resolved_call_name;
                    if (!method_suffix.empty() && method_suffix.front() == '.') {
                        method_suffix = "::" + method_suffix.substr(1);
                    }
                    const std::string direct_candidate = self_type + method_suffix;

                    if (func_map.contains(direct_candidate)) {
                        resolved_call_name = direct_candidate;
                    }
                    else {
                        for (const auto& [fname, _] : func_map) {
                            if (fname.size() >= direct_candidate.size() && fname.compare(fname.size() - direct_candidate.size(), direct_candidate.size(), direct_candidate) == 0) {
                                resolved_call_name = fname;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (resolved_call_name != identifier->content) {
            identifier->content = resolved_call_name;
        }

        // Handle bitcast intrinsics: __bitcast_f64(i64) -> f64, __bitcast_i64(f64) ->
        // i64
        if (resolved_call_name == "__bitcast_f64") {
            const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(1).get());
            generate_common(function, arg_node, "int64");
            function.code.emplace_back(op_bitcast, std::string("float64"), std::string("int64"));
            return;
        }
        if (resolved_call_name == "__bitcast_i64") {
            const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(1).get());
            generate_common(function, arg_node, "float64");
            function.code.emplace_back(op_bitcast, std::string("int64"), std::string("float64"));
            return;
        }
        if (resolved_call_name == "__bitcast_f32") {
            const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(1).get());
            generate_common(function, arg_node, "int32");
            function.code.emplace_back(op_bitcast, std::string("float32"), std::string("int32"));
            return;
        }
        if (resolved_call_name == "__bitcast_i32") {
            const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(1).get());
            generate_common(function, arg_node, "float32");
            function.code.emplace_back(op_bitcast, std::string("int32"), std::string("float32"));
            return;
        }

        // call through a function-pointer variable (local, parameter or global)
        std::string fsig;
        if (auto f = fnptr_sigs.find(function.name); f != fnptr_sigs.end()) {
            if (auto v = f->second.find(resolved_call_name); v != f->second.end()) {
                fsig = v->second;
            }
        }
        if (fsig.empty()) {
            if (auto g = global_fnptr_sigs.find(resolved_call_name); g != global_fnptr_sigs.end()) {
                fsig = g->second;
            }
        }
        if (!fsig.empty()) {
            // split "fn(a,b)ret" into top-level parameter types
            std::vector<std::string> params;
            {
                int depth = 0;
                std::string cur;
                for (std::size_t i = 3; i < fsig.size(); i++) {
                    const char ch = fsig[i];
                    if (ch == '(') depth++;
                    if (ch == ')') {
                        if (depth == 0) break;
                        depth--;
                    }
                    if (ch == ',' && depth == 0) {
                        params.push_back(cur);
                        cur.clear();
                        continue;
                    }
                    cur += ch;
                }
                if (!cur.empty()) params.push_back(cur);
            }

            function.code.emplace_back(op_load, resolved_call_name); // callee first

            std::size_t argi = 0;
            auto arg_location = 1;
            while (node->get_children().at(arg_location).get()->content != "end_call") {
                const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(arg_location).get());
                std::string ptype = argi < params.size() ? params[argi] : "int64";
                if (ptype.starts_with("fn(")) {
                    ptype = "int64";
                }
                generate_common(function, arg_node, ptype);
                ++arg_location;
                ++argi;
            }

            if (argi != params.size()) {
                throw std::runtime_error("call through function pointer '" + resolved_call_name + "' expects " + std::to_string(params.size()) + " argument(s), got " + std::to_string(argi));
            }

            function.code.emplace_back(op_call_indirect, fsig, std::to_string(argi), result_used);
            return;
        }

        const auto func_it = func_map.find(resolved_call_name);
        if (func_it == func_map.end()) {
            if (debug) {
                std::cout << CYAN << "[IR GEN] External/built-in function call: " << resolved_call_name << RESET << std::endl;
            }

            auto arg_location = 1;
            while (node->get_children().at(arg_location).get()->content != "end_call") {
                const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(arg_location).get());

                generate_common(function, arg_node, "int64");

                ++arg_location;
            }

            int actual_arg_count = arg_location - 1; 
            function.code.emplace_back(op_call, resolved_call_name, std::to_string(actual_arg_count), result_used);
            
            return;
        }

        if (debug) {
            std::cout << CYAN << "[IR GEN] Args for call " << identifier->content << ": " << RESET << func_it->second.args.size() << std::endl;
        }

        auto arg_location = 1;
        while (node->get_children().at(arg_location).get()->content != "end_call") {
            const auto arg_node = cst::cast_raw<cst_functionarg>(node->get_children().at(arg_location).get());

            std::string argument_type;
            if (static_cast<std::size_t>(arg_location - 1) < func_it->second.args.size()) {
                argument_type = func_it->second.args.at(arg_location - 1).type;
            }
            else {
                argument_type = "int64"; // default type for variadic arguments
            }

            // For variadic functions, detect float arguments and preserve their bits
            if (func_it->second.is_variadic) {
                bool is_va_slot = false;
                if (static_cast<std::size_t>(arg_location - 1) < func_it->second.args.size()) {
                    const auto& arg_name = func_it->second.args.at(arg_location - 1).name;
                    is_va_slot = (arg_name.size() >= 4 && arg_name.substr(0, 4) == "__va");
                }
                else {
                    is_va_slot = true;
                }

                const bool c_abi = func_it->second.is_external;

                if (is_va_slot) {
                    bool has_float_literal = false;
                    bool has_float_var = false;
                    bool has_float_member_access = false;
                    bool has_unary_minus = false;
                    std::string float_lit_content;
                    std::string float_var_name;
                    std::string float_member_type;

                    for (const auto& child : arg_node->get_children()) {
                        if (child->get_type() == cst_type::float_literal) {
                            has_float_literal = true;
                            float_lit_content = child->content;
                        }
                        else if (child->get_type() == cst_type::unary_minus_operator) {
                            has_unary_minus = true;
                        }
                        else if (child->get_type() == cst_type::identifier) {
                            auto var_it = local_variable_map[function].find(child->content);
                            if (var_it != local_variable_map[function].end()) {
                                if (var_it->second == "float32" || var_it->second == "float64") {
                                    has_float_var = true;
                                    float_var_name = child->content;
                                }
                            }
                        }
                        else if (child->get_type() == cst_type::memberaccess) {
                            std::string last_member;
                            for (const auto& mc : child->get_children()) {
                                if (mc->get_type() == cst_type::identifier) {
                                    last_member = mc->content;
                                }
                            }
                            if (!last_member.empty()) {
                                for (const auto& [ct_name, ct_node] : custom_type_map) {
                                    for (const auto& member : ct_node->get_children()) {
                                        for (const auto& mid : member->get_children()) {
                                            if (mid->get_type() == cst_type::identifier && mid->content == last_member) {
                                                if (member->get_type() == cst_type::float32_datatype || member->get_type() == cst_type::float64_datatype) {
                                                    has_float_member_access = true;
                                                    float_member_type = (member->get_type() == cst_type::float32_datatype) ? "float32" : "float64";
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (has_float_literal && c_abi) {
                        generate_common(function, arg_node, std::string("float64"));
                        ++arg_location;
                        continue;
                    }
                    if ((has_float_var || has_float_member_access) && c_abi) {
                        const std::string& t = has_float_var
                            ? local_variable_map[function][float_var_name]
                            : float_member_type;
                        generate_common(function, arg_node, t);
                        // C promotes a float argument to double before a variadic call
                        if (t == "float32") {
                            function.code.emplace_back(op_cast, std::string("f64"), std::string("float32"));
                        }
                        ++arg_location;
                        continue;
                    }

                    if (has_float_literal) {
                        // Convert the float literal directly to double bits as int64
                        double dval = from_numerical_string<double>(float_lit_content);
                        if (has_unary_minus) {
                            dval = -dval;
                        }
                        std::int64_t bits = std::bit_cast<std::int64_t>(dval);
                        function.code.emplace_back(op_push, bits);
                        ++arg_location;
                        continue;
                    }
                    else if (has_float_var) {
                        std::string var_float_type = local_variable_map[function][float_var_name];
                        generate_common(function, arg_node, var_float_type);
                        if (var_float_type == "float32") {
                            function.code.emplace_back(op_cast, std::string("f64"), std::string("float32"));
                        }
                        function.code.emplace_back(op_bitcast, std::string("int64"), std::string("float64"));
                        ++arg_location;
                        continue;
                    }
                    else if (has_float_member_access) {
                        // Load the struct member (goes into SIMD pool);
                        // if f32 promote to f64 first, then bitcast to int64
                        generate_common(function, arg_node, float_member_type);
                        if (float_member_type == "float32") {
                            function.code.emplace_back(op_cast, std::string("f64"), std::string("float32"));
                        }
                        function.code.emplace_back(op_bitcast, std::string("int64"), std::string("float64"));
                        ++arg_location;
                        continue;
                    }
                }
            }

            if (debug) {
                std::cout << CYAN << "[IR GEN] Generating argument for call (type, loc): " << RESET << "(" << argument_type << ", " << arg_location - 1 << ")" << std::endl;
            }

            generate_common(function, arg_node, argument_type);

            ++arg_location;
        }

        int actual_arg_count = arg_location - 1;
        if (func_it->second.is_variadic) {
            function.code.emplace_back(op_call, resolved_call_name, std::to_string(actual_arg_count), result_used);
        }
        else {
            function.code.emplace_back(op_call, resolved_call_name, "", result_used);
        }
    }

    void ir_gen::generate_return(ir_function& function, cst_returnstmt* return_node) {
        if (!try_finally_stack.empty()) {
            const auto& ctx = try_finally_stack.back();
            generate_common(function, return_node, function.type, type_of_push::single);
            function.code.emplace_back(op_store, ctx.ret_var);
            function.code.emplace_back(op_push, std::int64_t(1));
            function.code.emplace_back(op_store, ctx.exit_var);
            function.code.emplace_back(op_jmp, ctx.finally_label);
            return;
        }
        generate_common(function, return_node, function.type, type_of_push::ret);
        emit_defers(function);
        function.code.emplace_back(op_ret);
    }

    void ir_gen::emit_defers(ir_function& function) {
        std::vector<cst*> snapshot;
        for (auto frame_it = defer_bodies.rbegin(); frame_it != defer_bodies.rend(); ++frame_it) {
            for (auto it = frame_it->rbegin(); it != frame_it->rend(); ++it) {
                snapshot.push_back(*it);
            }
        }
        for (auto* raw : snapshot) {
            if (auto* block = cst::cast_raw<cst_block>(raw)) {
                generate_block(function, block, std::string(), std::string());
            }
        }
    }

    void ir_gen::emit_defers_above(ir_function& function, std::size_t from_depth) {
        if (from_depth >= defer_bodies.size()) return;
        std::vector<cst*> snapshot;
        for (std::size_t frame_i = defer_bodies.size(); frame_i > from_depth; --frame_i) {
            auto& frame = defer_bodies[frame_i - 1];
            for (auto it = frame.rbegin(); it != frame.rend(); ++it) {
                snapshot.push_back(*it);
            }
        }
        for (auto* raw : snapshot) {
            if (auto* block = cst::cast_raw<cst_block>(raw)) {
                generate_block(function, block, std::string(), std::string());
            }
        }
    }

    void ir_gen::generate_condition(ir_function& function, cst* node, const std::string& false_label, const std::string& true_label) {
        std::optional<std::string> expr_type = float_deref_type(node); // determine type for fpu comparison and or integer

        for (const auto& c : node->get_children()) {
            if (expr_type.has_value()) {
                break;
            }
            if (c->get_type() == cst_type::memberaccess) {
                if (auto t = member_access_type(function, c.get()); t.has_value()) {
                    expr_type = t;
                    break;
                }
            }
            else if (c->get_type() == cst_type::identifier) {
                if (local_variable_map.find(function) != local_variable_map.end()) {
                    auto& var_map = local_variable_map[function];
                    if (var_map.find(c->content) != var_map.end()) {
                        expr_type = var_map[c->content];
                        break;
                    }
                }
            }
            else {
                if (c->get_type() == cst_type::float_literal) {
                    expr_type = "float64";
                    break;
                }
            }
        }

        if (!expr_type.has_value()) {
            for (const auto& c : node->get_children()) {
                if (c->get_type() == cst_type::float_literal) {
                    expr_type = "float64";
                    break;
                }
            }
        }

        if (expr_type.has_value() && (expr_type->find("_ptr") != std::string::npos ||
                                      expr_type->find("_reference") != std::string::npos)) {
            expr_type = "int64";
        }

        auto handle_internal_push = [&, this](cst* expr) -> void {
            switch (expr->get_type()) {
            case cst_type::number_literal:
                {
                    handle_push_types(function, expr, expr_type);
                    break;
                }
            case cst_type::float_literal:
                {
                    handle_push_types(function, expr, expr_type);
                    break;
                }
            case cst_type::charliteral:
                {
                    handle_push_types(function, expr, expr_type);
                    break;
                }
            case cst_type::identifier:
                {
                    function.code.emplace_back(op_load, expr->content);
                    break;
                }
            case cst_type::functioncall:
                {
                    generate_function_call(function, expr);
                    break;
                }
            case cst_type::arrayaccess:
                {
                    generate_array_access(function, cst::cast_raw<cst_arrayaccess>(expr));
                    break;
                }
            case cst_type::memberaccess:
                {
                    generate_member_access(function, cst::cast_raw<cst_memberaccess>(expr));
                    break;
                }
                    // a string literal is a pointer to its bytes, pushed like a number
            case cst_type::stringliteral:
                function.code.emplace_back(op_push, expr->content);
                break;
            case cst_type::cast_to_datatype:
                {
                        // the default below dispatches operator nodes only
                    generate_cast<std::int64_t>(function, expr);
                    break;
                }
            case cst_type::dereference:
                {
                    function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(expr->content));
                    break;
                }
            default:
                {
                    generate_arith_and_bitwise_operators(function, expr, expr_type);
                    break;
                }
            }
        };

        {
            struct cond_node {
                cst* tok;
                int lhs = -1;
                int rhs = -1;
            };
            enum class arity { leaf, unary, binary, unknown };
            auto arity_of = [](cst_type t) {
                switch (t) {
                case cst_type::number_literal:
                case cst_type::float_literal:
                case cst_type::charliteral:
                case cst_type::identifier:
                case cst_type::stringliteral:
                case cst_type::functioncall:
                case cst_type::arrayaccess:
                case cst_type::memberaccess:
                case cst_type::cast_to_datatype:
                case cst_type::sizeof_expr:
                    return arity::leaf;
                case cst_type::unary_not_operator:
                case cst_type::unary_minus_operator:
                case cst_type::unary_plus_operator:
                case cst_type::unary_bitwise_not:
                case cst_type::dereference:
                case cst_type::reference:
                    return arity::unary;
                case cst_type::add_operator:
                case cst_type::subtract_operator:
                case cst_type::multiply_operator:
                case cst_type::division_operator:
                case cst_type::modulo_operator:
                case cst_type::bitwise_and:
                case cst_type::bitwise_or:
                case cst_type::xor_operator:
                case cst_type::bitwise_lshift:
                case cst_type::bitwise_rshift:
                case cst_type::equals_operator:
                case cst_type::not_equals_operator:
                case cst_type::greater_than_operator:
                case cst_type::less_than_operator:
                case cst_type::greater_than_or_equal_operator:
                case cst_type::less_than_or_equal_operator:
                case cst_type::and_operator:
                case cst_type::or_operator:
                    return arity::binary;
                default:
                    return arity::unknown;
                }
            };
            auto is_cmp = [](cst_type t) {
                return t == cst_type::equals_operator || t == cst_type::not_equals_operator ||
                       t == cst_type::greater_than_operator || t == cst_type::less_than_operator ||
                       t == cst_type::greater_than_or_equal_operator || t == cst_type::less_than_or_equal_operator;
            };
            auto is_boolean = [&](cst_type t) {
                return is_cmp(t) || t == cst_type::and_operator || t == cst_type::or_operator ||
                       t == cst_type::unary_not_operator;
            };
            auto is_float = [](const std::optional<std::string>& t) {
                return t.has_value() && (*t == "float32" || *t == "float64");
            };

            std::vector<cond_node> nodes;
            std::vector<int> pending;
            bool understood = true;
            for (const auto& ch : node->get_children()) {
                const cst_type t = ch->get_type();
                if (t == cst_type::expr_start || t == cst_type::expr_end || t == cst_type::block ||
                    t == cst_type::elseifstmt || t == cst_type::elsestmt) {
                    continue;
                }
                const arity a = arity_of(t);
                cond_node n{ch.get()};
                if (a == arity::unknown ||
                    (a == arity::unary && pending.empty()) ||
                    (a == arity::binary && pending.size() < 2)) {
                    understood = false;
                    break;
                }
                if (a == arity::binary) {
                    n.rhs = pending.back();
                    pending.pop_back();
                }
                if (a != arity::leaf) {
                    n.lhs = pending.back();
                    pending.pop_back();
                }
                nodes.push_back(n);
                pending.push_back(static_cast<int>(nodes.size()) - 1);
            }

            const std::optional<std::string> whole_type = expr_type;

            std::function<void(int, std::optional<std::string>&, bool&)> find_type =
                [&](int i, std::optional<std::string>& t, bool& saw_bool) {
                    if (i < 0 || t.has_value()) {
                        return;
                    }
                    cst* tok = nodes[i].tok;
                    if (is_boolean(tok->get_type())) {
                        saw_bool = true;
                        return;
                    }
                    find_type(nodes[i].lhs, t, saw_bool);
                    find_type(nodes[i].rhs, t, saw_bool);
                    if (t.has_value()) {
                        return;
                    }
                    switch (tok->get_type()) {
                    case cst_type::cast_to_datatype:
                        if (tok->content == "f32" || tok->content == "f64") {
                            t = (tok->content == "f32") ? "float32" : "float64";
                        }
                        break;
                    case cst_type::memberaccess:
                        t = member_access_type(function, tok);
                        break;
                    case cst_type::identifier:
                        if (auto vm = local_variable_map.find(function); vm != local_variable_map.end()) {
                            if (auto v = vm->second.find(tok->content); v != vm->second.end()) {
                                t = v->second;
                            }
                        }
                        break;
                    case cst_type::float_literal:
                        t = "float64";
                        break;
                    default:
                        break;
                    }
                };
            auto settle = [&](std::optional<std::string> t, bool saw_bool) {
                if (!t.has_value()) {
                    t = saw_bool ? std::optional<std::string>("int64") : whole_type;
                }
                if (t.has_value() && (t->find("_ptr") != std::string::npos ||
                                      t->find("_reference") != std::string::npos)) {
                    t = "int64";
                }
                return t;
            };
            auto comparison_type = [&](int i) {
                std::optional<std::string> t;
                bool saw_bool = false;
                find_type(nodes[i].lhs, t, saw_bool);
                find_type(nodes[i].rhs, t, saw_bool);
                return settle(t, saw_bool);
            };
            auto value_type = [&](int i) {
                std::optional<std::string> t;
                bool saw_bool = false;
                find_type(i, t, saw_bool);
                return settle(t, saw_bool);
            };

            if (understood && pending.size() == 1) {
                for (std::size_t i = 0; i < nodes.size(); ++i) {
                    if (nodes[i].tok->get_type() == cst_type::unary_not_operator &&
                        !is_boolean(nodes[nodes[i].lhs].tok->get_type()) && is_float(value_type(nodes[i].lhs))) {
                        understood = false;
                    }
                }
                std::function<bool(int)> truth_ok = [&](int i) {
                    const cst_type k = nodes[i].tok->get_type();
                    if (k == cst_type::unary_not_operator) {
                        return truth_ok(nodes[i].lhs);
                    }
                    if (k == cst_type::and_operator || k == cst_type::or_operator) {
                        return truth_ok(nodes[i].lhs) && truth_ok(nodes[i].rhs);
                    }
                    return is_cmp(k) || !is_float(value_type(i));
                };
                if (understood && !truth_ok(pending.back())) {
                    understood = false;
                }
            }

            if (understood && pending.size() == 1) {
                std::function<void(int, const std::optional<std::string>&)> emit_value;
                std::function<void(int, const std::string&, bool)> emit_jump;

                emit_value = [&](int i, const std::optional<std::string>& t) {
                    const cond_node& n = nodes[i];
                    const cst_type k = n.tok->get_type();
                    if (is_cmp(k)) {
                        const auto ct = comparison_type(i);
                        emit_value(n.lhs, ct);
                        emit_value(n.rhs, ct);
                        emit_comparison(function, ct);
                        switch (k) {
                        case cst_type::equals_operator: function.code.emplace_back(op_setz); break;
                        case cst_type::not_equals_operator: function.code.emplace_back(op_setnz); break;
                        case cst_type::greater_than_operator: function.code.emplace_back(op_setg); break;
                        case cst_type::less_than_operator: function.code.emplace_back(op_setl); break;
                        case cst_type::greater_than_or_equal_operator: function.code.emplace_back(op_setge); break;
                        default: function.code.emplace_back(op_setle); break;
                        }
                        return;
                    }
                    if (k == cst_type::and_operator || k == cst_type::or_operator) {
                        const std::string is_false = create_label();
                        const std::string done = create_label();
                        emit_jump(i, is_false, false);
                        function.code.emplace_back(op_push, std::int64_t(1));
                        function.code.emplace_back(op_jmp, done);
                        place_label(function, is_false);
                        function.code.emplace_back(op_push, std::int64_t(0));
                        place_label(function, done);
                        return;
                    }
                    if (k == cst_type::unary_not_operator) {
                        emit_value(n.lhs, value_type(n.lhs));
                        function.code.emplace_back(op_push, std::int64_t(0));
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setz);
                        return;
                    }
                    if (n.lhs >= 0) {
                        emit_value(n.lhs, t);
                    }
                    if (n.rhs >= 0) {
                        emit_value(n.rhs, t);
                    }
                    expr_type = t;
                    switch (k) {
                    case cst_type::reference:
                        function.code.emplace_back(op_reference);
                        break;
                    case cst_type::sizeof_expr:
                        function.code.emplace_back(op_sizeof, n.tok->content);
                        break;
                    default:
                        handle_internal_push(n.tok);
                        break;
                    }
                };

                emit_jump = [&](int i, const std::string& label, bool when) {
                    const cond_node& n = nodes[i];
                    const cst_type k = n.tok->get_type();
                    if (k == cst_type::unary_not_operator) {
                        emit_jump(n.lhs, label, !when);
                        return;
                    }
                    if (k == cst_type::and_operator || k == cst_type::or_operator) {
                        const bool decisive = (k == cst_type::or_operator);
                        if (when == decisive) {
                            emit_jump(n.lhs, label, when);
                            emit_jump(n.rhs, label, when);
                        }
                        else {
                            const std::string past = create_label();
                            emit_jump(n.lhs, past, decisive);
                            emit_jump(n.rhs, label, when);
                            place_label(function, past);
                        }
                        return;
                    }
                    if (is_cmp(k)) {
                        const auto ct = comparison_type(i);
                        emit_value(n.lhs, ct);
                        emit_value(n.rhs, ct);
                        emit_comparison(function, ct);
                        if (when) {
                            generate_or_jump(function, n.tok, label, is_float(ct));
                        }
                        else {
                            generate_normal_jump(function, n.tok, label, is_float(ct));
                        }
                        return;
                    }
                    emit_value(i, value_type(i));
                    function.code.emplace_back(op_push, std::int64_t(0));
                    function.code.emplace_back(op_cmp);
                    function.code.emplace_back(when ? op_jnz : op_jz, label);
                };

                emit_jump(pending.back(), false_label, false);
                return;
            }
        }

        std::vector<cst*> logic_stack;
        std::vector<cst*> eval_stack;

        for (std::size_t i = 0; i < node->get_children().size(); i++) {
            const auto& c = node->get_children().at(i);

            switch (c->get_type()) {
            case cst_type::expr_end:
            case cst_type::unary_not_operator:
            case cst_type::elseifstmt:
            case cst_type::elsestmt:
            case cst_type::block:
                {
                    break;
                }
            case cst_type::and_operator:
            case cst_type::or_operator:
                {
                    logic_stack.emplace_back(c.get());
                    break;
                }
            case cst_type::expr_start:
                {
                    const std::size_t curr_pos = i;

                    if (i > 0 && node->get_children().at(i - 1)->get_type() == cst_type::unary_not_operator) {
                        std::size_t not_pos = i;
                        std::size_t not_count = 0;

                        while (not_pos > 0 && node->get_children().at(not_pos - 1)->get_type() == cst_type::unary_not_operator) {
                            not_pos--;
                            not_count++;
                        }

                        const bool apply_not = not_count % 2 != 0;

                        for (int j = static_cast<int>(curr_pos); j >= 0; j--) {
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
            default:
                {
                    eval_stack.emplace_back(c.get());
                    break;
                }
            }
        }

        if (debug && !logic_stack.empty()) {
            std::cout << BLUE << "[IR GEN] Filled logic stack:" << RESET << std::endl;
            for (const auto& n : logic_stack) {
                n->visualize(2);
            }
            std::cout << BLUE << "[IR GEN] Logic stack size: " << RESET << logic_stack.size() << std::endl;
        }

        if (debug && !eval_stack.empty()) {
            std::cout << BLUE << "[IR GEN] Filled eval stack:" << RESET << std::endl;
            for (const auto& n : eval_stack) {
                n->visualize(2);
            }
            std::cout << BLUE << "[IR GEN] Eval stack size: " << RESET << eval_stack.size() << std::endl;
        }

        std::vector<std::vector<cst*>> eval_stack_m;

        std::vector<cst*> current_expr;
        for (auto& i : eval_stack) {
            current_expr.push_back(i);

            if (is_logical(i)) {
                eval_stack_m.push_back(current_expr);
                current_expr.clear();
            }
        }

        if (debug && !eval_stack_m.empty()) {
            std::cout << BLUE << "[IR GEN] Filled eval stack (m):" << RESET << std::endl;
            for (auto& arr : eval_stack_m) {
                std::cout << CYAN << "{" << RESET << std::endl;
                for (const auto& n : arr) {
                    n->visualize(2);
                }
                std::cout << CYAN << "}" << RESET << std::endl;
            }
            std::cout << BLUE << "[IR GEN] Eval stack (m) size: " << RESET << eval_stack.size() << std::endl;
        }

        // the operand type is decided per comparison: `a == 1 && h == 4.5` mixes an int and a
        // float comparison, so one type for the whole condition would push 4.5 as the integer 4
        const std::optional<std::string> whole_expr_type = expr_type;
        bool is_float_cmp = false;
        auto begin_comparison = [&](const std::vector<cst*>& chunk) {
            std::optional<std::string> t = std::nullopt;
            for (auto* e : chunk) {
                if (e->get_type() == cst_type::cast_to_datatype) {
                    if (e->content == "f32" || e->content == "f64") {
                        t = (e->content == "f32") ? "float32" : "float64";
                        break;
                    }
                }
                else if (e->get_type() == cst_type::memberaccess) {
                    // the member's own type beats the literal's default below
                    if (auto mt = member_access_type(function, e); mt.has_value()) {
                        t = mt;
                        break;
                    }
                }
                else if (e->get_type() == cst_type::identifier) {
                    if (auto vm = local_variable_map.find(function); vm != local_variable_map.end()) {
                        if (auto v = vm->second.find(e->content); v != vm->second.end()) {
                            t = v->second;
                            break;
                        }
                    }
                }
                else if (e->get_type() == cst_type::float_literal) {
                    t = "float64";
                    break;
                }
            }
            if (!t.has_value()) {
                t = whole_expr_type;
            }
            if (t.has_value() && (t->find("_ptr") != std::string::npos ||
                                  t->find("_reference") != std::string::npos)) {
                t = "int64";
            }
            expr_type = t;
            is_float_cmp = expr_type.has_value() && (expr_type.value() == "float32" || expr_type.value() == "float64");
        };

        if (!logic_stack.empty()) {
            std::size_t eval_index = 0;

            for (const auto c : logic_stack) {
                switch (c->get_type()) {
                case cst_type::or_operator:
                    {
                        if (eval_index < eval_stack_m.size()) {
                            const auto& expr = eval_stack_m[eval_index];
                            begin_comparison(expr);

                            for (size_t j = 0; j < expr.size() - 1; j++) {
                                handle_internal_push(expr[j]);
                            }

                            emit_comparison(function, expr_type);

                            if (expr.back()->do_not) {
                                generate_and_jump(function, expr.back(), true_label, is_float_cmp);
                            }
                            else {
                                generate_or_jump(function, expr.back(), true_label, is_float_cmp);
                            }

                            eval_index++;
                        }
                        break;
                    }
                case cst_type::and_operator:
                    {
                        if (eval_index < eval_stack_m.size()) {
                            const auto& expr = eval_stack_m[eval_index];
                            begin_comparison(expr);

                            for (size_t j = 0; j < expr.size() - 1; j++) {
                                handle_internal_push(expr[j]);
                            }

                            emit_comparison(function, expr_type);

                            if (expr.back()->do_not) {
                                generate_or_jump(function, expr.back(), false_label, is_float_cmp);
                            }
                            else {
                                generate_and_jump(function, expr.back(), false_label, is_float_cmp);
                            }

                            eval_index++;
                        }
                        break;
                    }
                default:
                    {
                        break;
                    }
                }
            }

            while (eval_index < eval_stack_m.size()) {
                const auto& expr = eval_stack_m[eval_index];
                begin_comparison(expr);

                for (size_t j = 0; j < expr.size() - 1; j++) {
                    handle_internal_push(expr[j]);
                }

                emit_comparison(function, expr_type);

                if (expr.back()->do_not) {
                    generate_inverted_jump(function, expr.back(), false_label, is_float_cmp);
                }
                else {
                    generate_normal_jump(function, expr.back(), false_label, is_float_cmp);
                }

                eval_index++;
            }
        }
        else {
            if (!eval_stack_m.empty()) {
                const auto& expr = eval_stack_m[0];
                begin_comparison(expr);

                for (size_t j = 0; j < expr.size() - 1; j++) {
                    handle_internal_push(expr[j]);
                }

                emit_comparison(function, expr_type);

                if (expr.back()->do_not) {
                    generate_inverted_jump(function, expr.back(), false_label, is_float_cmp);
                }
                else {
                    generate_normal_jump(function, expr.back(), false_label, is_float_cmp);
                }
            }
        }
    }

    void ir_gen::generate_if(ir_function& function, cst_ifstmt* if_node, const std::string& current_break_label, const std::string& current_loop_start) {
        const std::string false_label = create_label(); // exit / else or whatever
        const std::string true_label = create_label();  // block
        const std::string end_label = create_label();   // end of entire if/elseif/else chain

        generate_condition(function, if_node, false_label, true_label);

        place_label(function, true_label);

        for (const auto& c : if_node->get_children()) {
            if (c->get_type() == cst_type::block) {
                generate_block(function, cst::cast_raw<cst_block>(c.get()), current_break_label, current_loop_start);
                break;
            }
        }

        function.code.emplace_back(op_jmp, end_label);

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

                function.code.emplace_back(op_jmp, end_label);

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

        place_label(function, end_label);
    }

    void ir_gen::generate_or_jump(ir_function& function, cst* comparison, const std::string& true_label, bool is_float_cmp) {
        switch (comparison->get_type()) {
        case cst_type::equals_operator:
            function.code.emplace_back(op_jz, true_label);

            break;
        case cst_type::not_equals_operator:
            function.code.emplace_back(op_jnz, true_label);

            break;
        case cst_type::greater_than_operator:
            function.code.emplace_back(is_float_cmp ? op_ja : op_jg, true_label);

            break;
        case cst_type::less_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jb : op_jl, true_label);

            break;
        case cst_type::greater_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jae : op_jge, true_label);

            break;
        case cst_type::less_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jbe : op_jle, true_label);

            break;
        default:
            break;
        }
    }

    void ir_gen::generate_and_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp) {
        switch (comparison->get_type()) {
        case cst_type::equals_operator:
            function.code.emplace_back(op_jnz, false_label);
            break;
        case cst_type::not_equals_operator:
            function.code.emplace_back(op_jz, false_label);
            break;
        case cst_type::greater_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jbe : op_jle, false_label);
            break;
        case cst_type::less_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jae : op_jge, false_label);
            break;
        case cst_type::greater_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jb : op_jl, false_label);
            break;
        case cst_type::less_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_ja : op_jg, false_label);
            break;
        default:
            break;
        }
    }

    void ir_gen::generate_normal_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp) {
        switch (comparison->get_type()) {
        case cst_type::equals_operator:
            function.code.emplace_back(op_jnz, false_label);
            break;
        case cst_type::not_equals_operator:
            function.code.emplace_back(op_jz, false_label);
            break;
        case cst_type::greater_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jbe : op_jle, false_label);
            break;
        case cst_type::less_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jae : op_jge, false_label);
            break;
        case cst_type::greater_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jb : op_jl, false_label);
            break;
        case cst_type::less_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_ja : op_jg, false_label);
            break;
        default:
            break;
        }
    }

    void ir_gen::generate_inverted_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp) {
        switch (comparison->get_type()) {
        case cst_type::equals_operator:
            function.code.emplace_back(op_jz, false_label);
            break;
        case cst_type::not_equals_operator:
            function.code.emplace_back(op_jnz, false_label);
            break;
        case cst_type::greater_than_operator:
            function.code.emplace_back(is_float_cmp ? op_jb : op_jl, false_label);
            break;
        case cst_type::less_than_operator:
            function.code.emplace_back(is_float_cmp ? op_ja : op_jg, false_label);
            break;
        case cst_type::greater_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jbe : op_jle, false_label);
            break;
        case cst_type::less_than_or_equal_operator:
            function.code.emplace_back(is_float_cmp ? op_jae : op_jge, false_label);
            break;
        default:
            break;
        }
    }

    // b1 is break, l1 is loop start (disorganized names cuz function rewrite)

    void ir_gen::generate_loop(ir_function& function, cst_loopstmt* loop_node) {
        auto L1 = create_label();
        const auto B1 = create_label();

        place_label(function, L1);

        loop_defer_depths.push_back(defer_bodies.size());
        for (const auto& c : loop_node->get_children()) {
            if (c->get_type() == cst_type::block) {
                generate_block(function, cst::cast_raw<cst_block>(c.get()), B1, L1);
            }
        }
        loop_defer_depths.pop_back();

        function.code.emplace_back(op_jmp, L1);

        place_label(function, B1);
    }

    void ir_gen::generate_while(ir_function& function, cst_whilestmt* while_node) {
        std::string loop_label = create_label();
        const std::string break_label = create_label();

        place_label(function, loop_label);

        generate_condition(function, while_node, break_label, loop_label);

        loop_defer_depths.push_back(defer_bodies.size());
        for (const auto& c : while_node->get_children()) {
            if (c->get_type() == cst_type::block) {
                generate_block(function, cst::cast_raw<cst_block>(c.get()), break_label, loop_label);
            }
        }
        loop_defer_depths.pop_back();

        function.code.emplace_back(op_jmp, loop_label);

        place_label(function, break_label);
    }

    void ir_gen::generate_for(ir_function& function, cst_forstmt* for_node) {
        const auto first_node = for_node->get_children().front().get();
        const auto condition = for_node->get_children().at(1).get();
        const auto body = for_node->get_children().back().get();

        switch (for_node->get_children().front()->get_type()) {
        case cst_type::int8_datatype:
        case cst_type::int16_datatype:
        case cst_type::int32_datatype:
        case cst_type::int64_datatype:
            {
                const auto identifier = cst::cast_raw<cst_identifier>(first_node->get_children().front().get()); // name
                const auto assignment = cst::cast_raw<cst_assignment>(first_node->get_children().back().get());
                // expression stuff
                if (assignment->get_children().front()->get_type() == cst_type::cast_to_datatype) {
                    const auto casting_datatype = cst::cast_raw<cst_cast_to_datatype>(assignment->get_children().front().get());
                    generate_common_generic<std::int64_t>(function, casting_datatype);

                    function.code.emplace_back(op_cast, casting_datatype->content);
                }
                else {
                    generate_common_generic<std::int64_t>(function, assignment);
                }

                function.code.emplace_back(op_store, identifier->content, first_node->to_string().substr(4, first_node->to_string().size()));
                break;
            }
        case cst_type::uint8_datatype:
        case cst_type::uint16_datatype:
        case cst_type::uint32_datatype:
        case cst_type::uint64_datatype:
            {
                const auto identifier = cst::cast_raw<cst_identifier>(first_node->get_children().front().get());
                const auto assignment = cst::cast_raw<cst_assignment>(first_node->get_children().back().get());

                if (assignment->get_children().front()->get_type() == cst_type::cast_to_datatype) {
                    const auto casting_datatype = cst::cast_raw<cst_cast_to_datatype>(assignment->get_children().front().get());
                    generate_common_generic<std::uint64_t>(function, casting_datatype);

                    function.code.emplace_back(op_cast, casting_datatype->content);
                }
                else {
                    generate_common_generic<std::uint64_t>(function, assignment);
                }

                function.code.emplace_back(op_store, identifier->content, first_node->to_string().substr(4, first_node->to_string().size()));

                break;
            }
        default:
            {
                break;
            }
        }

        auto L1 = create_label();
        const auto B1 = create_label();

        place_label(function, L1); // L1 START

        generate_condition(function, condition, B1, L1);

        auto step_label = create_label(); // continue target = step expression

        loop_defer_depths.push_back(defer_bodies.size());
        generate_block(function, cst::cast_raw<cst_block>(body), B1, step_label);
        loop_defer_depths.pop_back();

        place_label(function, step_label); // step expression starts here

        const auto new_block = cst::new_node<cst_block>(); // swapping parent
        for (auto& c : for_node->get_children().at(2)->get_children()) {
            new_block->add_child(std::move(c));
        }

        generate_block(function, new_block.get()); // iterator

        function.code.emplace_back(op_jmp, L1);

        place_label(function, B1);
    }

    void ir_gen::generate_switch(ir_function& function, cst_switchstmt* switch_node, const std::string& current_break_label, const std::string& current_loop_start) {
        const std::string end_label = create_label();

        std::vector<cst*> switch_expr_nodes;
        std::vector<cst*> case_nodes;
        cst* default_node = nullptr;

        for (const auto& c : switch_node->get_children()) {
            if (c->get_type() == cst_type::casestmt) {
                case_nodes.push_back(c.get());
            }
            else if (c->get_type() == cst_type::defaultstmt) {
                default_node = c.get();
            }
            else {
                switch_expr_nodes.push_back(c.get());
            }
        }

        std::string switch_tmp = "__switch_tmp_" + std::to_string(label_count);
        label_count++;

        for (auto* expr_node : switch_expr_nodes) {
            switch (expr_node->get_type()) {
            case cst_type::identifier:
                function.code.emplace_back(op_load, expr_node->content);
                break;
            case cst_type::number_literal:
                function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(expr_node->content));
                break;
            case cst_type::charliteral:
                {
                    std::int64_t char_val = expr_node->content.empty() ? 0 : static_cast<std::int64_t>(static_cast<unsigned char>(expr_node->content[0]));
                    function.code.emplace_back(op_push, char_val);
                    break;
                }
            case cst_type::functioncall:
                generate_function_call(function, expr_node);
                break;
            case cst_type::memberaccess:
                generate_member_access(function, cst::cast_raw<cst_memberaccess>(expr_node));
                break;
            case cst_type::arrayaccess:
                generate_array_access(function, cst::cast_raw<cst_arrayaccess>(expr_node));
                break;
            default:
                generate_arith_and_bitwise_operators(function, expr_node, "int64");
                break;
            }
        }
        function.code.emplace_back(op_store, switch_tmp, std::string("int64"));
        local_variable_map[function][switch_tmp] = "int64";

        struct case_info {
            cst* node;
            std::vector<cst*> val_nodes;
            cst_block* block;
            std::string body_label;
            std::string false_label;
        };
        std::vector<case_info> case_infos;

        for (auto* case_n : case_nodes) {
            case_info ci;
            ci.node = case_n;
            ci.block = nullptr;

            for (const auto& cc : case_n->get_children()) {
                if (cc->get_type() == cst_type::block) {
                    ci.block = cst::cast_raw<cst_block>(cc.get());
                }
                else {
                    ci.val_nodes.push_back(cc.get());
                }
            }
            ci.body_label = create_label();
            ci.false_label = create_label();
            case_infos.push_back(std::move(ci));
        }

        const std::string default_label = default_node ? create_label() : "";

        auto find_next_body_label = [&](std::size_t idx) -> std::string {
            for (std::size_t j = idx + 1; j < case_infos.size(); j++) {
                if (case_infos[j].block) {
                    return case_infos[j].body_label;
                }
            }
            if (default_node) {
                return default_label;
            }
            return end_label;
        };

        for (std::size_t i = 0; i < case_infos.size(); i++) {
            auto& ci = case_infos[i];

            function.code.emplace_back(op_load, switch_tmp);
            for (auto* cv : ci.val_nodes) {
                switch (cv->get_type()) {
                case cst_type::identifier:
                    function.code.emplace_back(op_load, cv->content);
                    break;
                case cst_type::number_literal:
                    function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(cv->content));
                    break;
                case cst_type::charliteral:
                    {
                        std::int64_t char_val = cv->content.empty() ? 0 : static_cast<std::int64_t>(static_cast<unsigned char>(cv->content[0]));
                        function.code.emplace_back(op_push, char_val);
                        break;
                    }
                case cst_type::functioncall:
                    generate_function_call(function, cv);
                    break;
                default:
                    generate_arith_and_bitwise_operators(function, cv, "int64");
                    break;
                }
            }
            function.code.emplace_back(op_cmp);
            function.code.emplace_back(op_jnz, ci.false_label); // jump if NOT equal

            if (ci.block) {
                place_label(function, ci.body_label);
                generate_block(function, ci.block, current_break_label, current_loop_start);
                function.code.emplace_back(op_jmp, end_label);
            }
            else {
                std::string target = find_next_body_label(i);
                function.code.emplace_back(op_jmp, target);
            }

            place_label(function, ci.false_label);
        }

        if (default_node) {
            place_label(function, default_label);
            for (const auto& dc : default_node->get_children()) {
                if (dc->get_type() == cst_type::block) {
                    generate_block(function, cst::cast_raw<cst_block>(dc.get()), current_break_label, current_loop_start);
                }
            }
        }

        place_label(function, end_label);

        if (auto it = local_variable_map.find(function); it != local_variable_map.end()) {
            it->second.erase(switch_tmp);
        }
    }

    void ir_gen::generate_array_decl(ir_function& function, cst_array* array_node) {
        auto type = array_node->get_children().front().get();

        auto identifier = cst::cast_raw<cst_identifier>(type->get_children().front().get());

        std::string elem_type_name = type->to_string().substr(4, type->to_string().size());
        if (type->get_type() == cst_type::structure) {
            elem_type_name = type->content;
        }

        local_array_map[function][identifier->content] = elem_type_name;

        if (debug) {
            std::cout << CYAN << "[IR GEN] Array type: " << RESET << elem_type_name << std::endl;
            std::cout << CYAN << "[IR GEN] Array name: " << RESET << identifier->content << std::endl;
        }

        function.code.emplace_back(op_array_decl, identifier->content);
        function.code.emplace_back(op_decl_array_type, elem_type_name);

        auto dimensions_count_node = cst::cast_raw<cst_dimensions_count>(array_node->get_children().at(1).get());

        if (debug) {
            std::cout << CYAN << "[IR GEN] Dimensions: " << RESET << dimensions_count_node->content << std::endl;
        }

        std::vector<std::uint64_t> dimensions;
        for (const auto& c : dimensions_count_node->get_children()) {
            if (c->get_type() == cst_type::dimension) {
                if (debug) {
                    std::cout << CYAN << "[IR GEN] D: " << RESET << c->content << std::endl;
                }
                dimensions.push_back(from_numerical_string<std::uint64_t>(c->content));
            }
        }

        function.code.emplace_back(op_array_dimensions, static_cast<std::uint64_t>(dimensions.size()));

        for (const auto& dim : dimensions) {
            function.code.emplace_back(op_array_size, dim);
        }

        // 1d arrays
        if (dimensions.size() == 1) {
            auto first_array_node = cst::cast_raw<cst_arraybody>(array_node->get_children().back().get());

            if (first_array_node == nullptr) {
                return;
            }

            for (std::size_t i = 0; i < dimensions.front(); ++i) {
                function.code.emplace_back(op_declare_where_to_store, static_cast<std::uint64_t>(i));

                auto element = cst::cast_raw<cst_arrayelement>(first_array_node->get_children().at(i).get());

                for (const auto& c : element->get_children()) {
                    switch (c->get_type()) {
                    case cst_type::number_literal:
                        {
                            handle_push_types(function, c.get(), local_array_map[function][identifier->content]);
                            break;
                        }
                    case cst_type::float_literal:
                        {
                            handle_push_types(function, c.get(), local_array_map[function][identifier->content]);
                            break;
                        }
                    case cst_type::identifier:
                        {
                            function.code.emplace_back(op_load, c->content);
                            break;
                        }
                    case cst_type::functioncall:
                        {
                            generate_function_call(function, c.get());
                            break;
                        }
                    case cst_type::arrayaccess:
                        {
                            generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
                            break;
                        }
                    // a string literal is a pointer to its bytes, pushed like a number
                    case cst_type::stringliteral:
                        function.code.emplace_back(op_push, c->content);
                        break;
                    case cst_type::cast_to_datatype:
                        {
                            // the default below dispatches operator nodes only
                            generate_cast<std::int64_t>(function, c.get());
                            break;
                        }
                    case cst_type::dereference:
                        {
                            function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));
                            break;
                        }
                    default:
                        {
                            generate_arith_and_bitwise_operators(function, c.get(), local_array_map[function][identifier->content]);

                            break;
                        }
                    }
                }

                function.code.emplace_back(op_array_store_element, identifier->content);
            }
        }
        // multidimensional arrays
        else {
            auto array_body = cst::cast_raw<cst_arraybody>(array_node->get_children().back().get());

            if (array_body == nullptr) {
                return;
            }

            std::uint64_t linear_index = 0;

            std::function<void(cst*, std::size_t)> traverse_array;
            traverse_array = [&](cst* current_body, std::size_t depth) {
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

                            for (const auto& c : element->get_children()) {
                                switch (c->get_type()) {
                                case cst_type::number_literal:
                                    {
                                        handle_push_types(function, c.get(), local_array_map[function][identifier->content]);
                                        break;
                                    }
                                case cst_type::float_literal:
                                    {
                                        handle_push_types(function, c.get(), local_array_map[function][identifier->content]);
                                        break;
                                    }
                                case cst_type::identifier:
                                    {
                                        function.code.emplace_back(op_load, c->content);
                                        break;
                                    }
                                case cst_type::functioncall:
                                    {
                                        generate_function_call(function, c.get());
                                        break;
                                    }
                                case cst_type::arrayaccess:
                                    {
                                        generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
                                        break;
                                    }
                                // a string literal is a pointer to its bytes, pushed like a number
                                case cst_type::stringliteral:
                                    function.code.emplace_back(op_push, c->content);
                                    break;
                                case cst_type::cast_to_datatype:
                                    {
                                        // the default below dispatches operator nodes only
                                        generate_cast<std::int64_t>(function, c.get());
                                        break;
                                    }
                                case cst_type::dereference:
                                    {
                                        function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));
                                        break;
                                    }
                                default:
                                    {
                                        generate_arith_and_bitwise_operators(function, c.get(), local_array_map[function][identifier->content]);

                                        break;
                                    }
                                }
                            }

                            function.code.emplace_back(op_array_store_element, identifier->content);
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

    void ir_gen::generate_array_access(ir_function& function, cst_arrayaccess* array_access_node) {
        bool pointer_base = false;
        if (!is_composite_access(array_access_node)
            && !array_access_node->get_children().empty()
            && array_access_node->get_children().front()->get_type() == cst_type::identifier) {
            const auto& base_name = array_access_node->get_children().front()->content;
            if (auto it = local_variable_map[function].find(base_name);
                it != local_variable_map[function].end()) {
                const auto& t = it->second;
                if ((t.ends_with("_ptr") || t.ends_with("_reference"))
                    && local_array_map[function].find(base_name) == local_array_map[function].end()) {
                    std::string pointee = t;
                    if (pointee.ends_with("_ptr"))       pointee = pointee.substr(0, pointee.size() - 4);
                    if (pointee.ends_with("_reference")) pointee = pointee.substr(0, pointee.size() - 10);
                    const bool primitive_pointee =
                        pointee == "int8"  || pointee == "int16"  || pointee == "int32"  || pointee == "int64" ||
                        pointee == "uint8" || pointee == "uint16" || pointee == "uint32" || pointee == "uint64" ||
                        pointee == "float32" || pointee == "float64" ||
                        pointee == "bool"  || pointee == "char"   || pointee == "str"    || pointee == "string";
                    if (primitive_pointee) {
                        pointer_base = true;
                    }
                }
            }
        }
        if (is_composite_access(array_access_node) || pointer_base) {
            const auto& kids = array_access_node->get_children();
            cst* assignment_node = nullptr;
            if (!kids.empty() && kids.back()->get_type() == cst_type::assignment) {
                assignment_node = kids.back().get();
            }
            // @-prefix: just leaves the address on the stack
            bool want_address_only = false;
            if (!function.code.empty() && function.code.back().op == op_reference) {
                function.code.pop_back();
                want_address_only = true;
            }
            const std::string leaf_type = generate_lvalue_addr(function, array_access_node);
            if (assignment_node) {
                emit_store_through_addr(function, assignment_node, leaf_type);
            }
            else if (!want_address_only) {
                emit_load_through_addr(function, leaf_type);
            }
            return;
        }

        bool reference_element = false;
        if (!function.code.empty() && function.code.back().op == op_reference) {
            function.code.pop_back();
            reference_element = true;
        }
        if (array_access_node->get_children().front()->get_type() == cst_type::identifier) {
            // 1-dimensional array access
            auto identifier = cst::cast_raw<cst_identifier>(array_access_node->get_children().front().get());

            auto has_assigment = array_access_node->get_children().back()->get_type() == cst_type::assignment;

            if (debug) {
                std::cout << CYAN << "[IR GEN] Access Array Type: " << local_array_map[function][identifier->content] << RESET << std::endl;
            }

            for (std::size_t i = 1; i < array_access_node->get_children().size() - ((has_assigment) ? 1 : 0); i++) {
                auto c = array_access_node->get_children().at(i).get();

                switch (c->get_type()) {
                case cst_type::number_literal:
                    {
                        handle_push_types(function, c, local_array_map[function][identifier->content]);

                        break;
                    }
                case cst_type::identifier:
                    {
                        function.code.emplace_back(op_load, c->content);

                        break;
                    }
                case cst_type::functioncall:
                    {
                        generate_function_call(function, c);

                        break;
                    }
                case cst_type::or_operator:
                    {
                        function.code.emplace_back(op_logical_or);

                        break;
                    }
                case cst_type::and_operator:
                    {
                        function.code.emplace_back(op_logical_and);

                        break;
                    }
                case cst_type::equals_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setz);

                        break;
                    }
                case cst_type::not_equals_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setnz);

                        break;
                    }
                case cst_type::greater_than_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setg);

                        break;
                    }
                case cst_type::less_than_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setl);

                        break;
                    }
                case cst_type::greater_than_or_equal_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setge);

                        break;
                    }
                case cst_type::less_than_or_equal_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setle);

                        break;
                    }
                case cst_type::assignment:
                    {
                        generate_common(function, c, local_array_map[function][identifier->content]);

                        break;
                    }
                case cst_type::arrayaccess:
                    {
                        generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c));

                        break;
                    }
                default:
                    {
                        generate_arith_and_bitwise_operators(function, c, local_array_map[function][identifier->content]);

                        break;
                    }
                }
            }

            function.code.emplace_back(op_mark_for_array_access);

            if (array_access_node->get_children().back()->get_type() == cst_type::assignment) {
                auto assignment = cst::cast_raw<cst_assignment>(array_access_node->get_children().back().get());

                for (const auto& c : assignment->get_children()) {
                    switch (c->get_type()) {
                    case cst_type::number_literal:
                        {
                            handle_push_types(function, c.get(), local_array_map[function][identifier->content]);
                            break;
                        }
                    case cst_type::identifier:
                        {
                            function.code.emplace_back(op_load, c->content);
                            break;
                        }
                    case cst_type::functioncall:
                        {
                            generate_function_call(function, c.get());
                            break;
                        }
                    case cst_type::arrayaccess:
                        {
                            generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));
                            break;
                        }
                    // a string literal is a pointer to its bytes, pushed like a number
                    case cst_type::stringliteral:
                        function.code.emplace_back(op_push, c->content);
                        break;
                    case cst_type::cast_to_datatype:
                        {
                            // the default below dispatches operator nodes only
                            generate_cast<std::int64_t>(function, c.get());
                            break;
                        }
                    case cst_type::dereference:
                        {
                            function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));
                            break;
                        }
                    default:
                        {
                            generate_arith_and_bitwise_operators(function, c.get(), local_array_map[function][identifier->content]);
                            break;
                        }
                    }
                }

                function.code.emplace_back(op_array_store_element, identifier->content);
            }
            else {
                if (reference_element) {
                    function.code.emplace_back(op_reference);
                }
                function.code.emplace_back(op_array_access_element, identifier->content);
            }
        }
        else if (array_access_node->get_children().front()->get_type() == cst_type::arrayaccess) {
            // 2d+
            auto depth = 1;
            std::vector<cst*> indices;
            cst_assignment* assignment = nullptr;

            std::function<void(cst*)> traverse_access_nodes;
            traverse_access_nodes = [&](cst* node) -> void {
                for (auto& child : node->get_children()) {
                    switch (child->get_type()) {
                    case cst_type::arrayaccess:
                        {
                            depth++;

                            traverse_access_nodes(child.get());

                            break;
                        }
                    case cst_type::assignment:
                        {
                            assignment = cst::cast_raw<cst_assignment>(child.get());

                            return;
                        }
                    default:
                        {
                            indices.emplace_back(child.get());
                            traverse_access_nodes(child.get());

                            break;
                        }
                    }
                }
            };

            traverse_access_nodes(array_access_node);

            cst_identifier* array_name = cst::cast_raw<cst_identifier>(indices.front()); // ALWAYS FRONT
            indices = std::vector(indices.begin() + 1, indices.end());

            for (auto& c : indices) {
                // push indices in order
                switch (c->get_type()) {
                case cst_type::number_literal:
                    {
                        handle_push_types(function, c, local_array_map[function][array_name->content]);
                        // function.code.emplace_back(op_mark_for_array_access);

                        break;
                    }
                case cst_type::identifier:
                    {
                        function.code.emplace_back(op_load, c->content);
                        // function.code.emplace_back(op_mark_for_array_access);

                        break;
                    }
                case cst_type::functioncall:
                    {
                        generate_function_call(function, c);
                        // function.code.emplace_back(op_mark_for_array_access);

                        break;
                    }
                case cst_type::or_operator:
                    {
                        function.code.emplace_back(op_logical_or);

                        break;
                    }
                case cst_type::and_operator:
                    {
                        function.code.emplace_back(op_logical_and);

                        break;
                    }
                case cst_type::equals_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setz);

                        break;
                    }
                case cst_type::not_equals_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setnz);

                        break;
                    }
                case cst_type::greater_than_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setg);

                        break;
                    }
                case cst_type::less_than_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setl);

                        break;
                    }
                case cst_type::greater_than_or_equal_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setge);

                        break;
                    }
                case cst_type::less_than_or_equal_operator:
                    {
                        function.code.emplace_back(op_cmp);
                        function.code.emplace_back(op_setle);

                        break;
                    }
                case cst_type::arrayaccess:
                    {
                        generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c));
                        // function.code.emplace_back(op_mark_for_array_access);

                        break;
                    }
                default:
                    {
                        generate_arith_and_bitwise_operators(function, c, local_array_map[function][array_name->content]);
                        // function.code.emplace_back(op_mark_for_array_access);

                        break;
                    }
                }
            }

            if (assignment) {
                if (debug) {
                    std::cout << CYAN << "[IR GEN] Array assignment child count: " << assignment->get_children().size() << RESET << std::endl;
                    for (const auto& child : assignment->get_children()) {
                        std::cout << CYAN << "  [IR GEN] Child type: " << static_cast<int>(child->get_type()) << RESET << std::endl;
                    }
                }
                for (const auto& c : assignment->get_children()) {
                    switch (c->get_type()) {
                    /* ADD OTHER TYPES AND CASES TO THIS */
                    case cst_type::number_literal:
                        {
                            handle_push_types(function, c.get(), local_array_map[function][array_name->content]);

                            break;
                        }
                    case cst_type::identifier:
                        {
                            function.code.emplace_back(op_load, c->content);

                            break;
                        }
                    case cst_type::functioncall:
                        {
                            generate_function_call(function, c.get());

                            break;
                        }
                    case cst_type::arrayaccess:
                        {
                            generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));

                            break;
                        }
                    // a string literal is a pointer to its bytes, pushed like a number
                    case cst_type::stringliteral:
                        function.code.emplace_back(op_push, c->content);
                        break;
                    case cst_type::cast_to_datatype:
                        {
                            // the default below dispatches operator nodes only
                            generate_cast<std::int64_t>(function, c.get());
                            break;
                        }
                    case cst_type::dereference:
                        {
                            function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(c->content));

                            break;
                        }
                    default:
                        {
                            generate_arith_and_bitwise_operators(function, c.get(), local_array_map[function][array_name->content]);

                            break;
                        }
                    }
                }

                function.code.emplace_back(op_array_store_element, array_name->content);
            }
            else {
                function.code.emplace_back(op_array_access_element, array_name->content);
            }
        }
    }

    void ir_gen::generate_struct_decl(ir_function& function, cst_struct* struct_node) {
        if (custom_type_map.contains(struct_node->content) && custom_type_map[struct_node->content]->get_type() == cst_type::enumeration) {
            const auto id = struct_node->get_children().front().get();
            local_variable_map[function][id->content] = "int64";

            if (struct_node->get_children().size() > 1) {
                const auto assignment = struct_node->get_children().back().get();
                generate_common_generic<std::int64_t>(function, assignment, "int64");
                function.code.emplace_back(op_store, id->content, std::string("int64"));
            }
            else {
                function.code.emplace_back(op_push, static_cast<std::int64_t>(0), std::string("int64"));
                function.code.emplace_back(op_store, id->content, std::string("int64"));
            }
            return;
        }

        // include pointer information in the struct type
        std::string struct_type = struct_node->content;
        if (struct_node->num_pointers > 0) {
            struct_type += kStructPtrSuffix;
        }
        function.code.emplace_back(op_struct_decl, struct_type);

        if (struct_node->get_children().size() > 1) {
            // assignment
            const auto id = struct_node->get_children().front().get();        // identifier
            const auto assignment = struct_node->get_children().back().get(); // assignment expression

            generate_common_generic<std::int64_t>(function, assignment);

            function.code.emplace_back(op_struct_store, id->content);
            // register the variable in the local variable map
            local_variable_map[function][id->content] = struct_type;
        }
        else {
            const auto id = struct_node->get_children().front().get(); // identifier

            function.code.emplace_back(op_struct_store, id->content);
            // register the variable in the local variable map
            local_variable_map[function][id->content] = struct_type;
        }
    }

    bool ir_gen::is_composite_access(cst* node) {
        if (!node) return false;
        if (node->get_type() != cst_type::memberaccess
            && node->get_type() != cst_type::arrayaccess) {
            return false;
        }
        auto& ch = node->get_children();
        if (ch.empty()) return false;
        return ch.front()->get_type() != cst_type::identifier;
    }

    std::string ir_gen::generate_lvalue_addr(ir_function& function, cst* node) {
        if (!node) return "";

        if (node->get_type() == cst_type::functioncall) {
            generate_function_call(function, node);
            cst* name_child = node->get_children().empty() ? nullptr
                : node->get_children().front().get();
            const std::string fname = name_child ? name_child->content : std::string();
            std::string ret_type;
            if (auto it = func_map.find(fname); it != func_map.end()) {
                ret_type = it->second.type;
            }
            std::string slot_type = ret_type;
            if (slot_type.ends_with("_ptr"))       slot_type = slot_type.substr(0, slot_type.size() - 4);
            if (slot_type.ends_with("_reference")) slot_type = slot_type.substr(0, slot_type.size() - 10);

            const std::string hidden = std::string("__fn_result_") + std::to_string(hidden_slot_count++);

            if (custom_type_map.contains(slot_type)) {
                function.code.emplace_back(op_struct_decl, slot_type);
                function.code.emplace_back(op_struct_store, hidden);
                local_variable_map[function][hidden] = slot_type;
                // op_load on a struct variable emits lea -- the slot's address
                function.code.emplace_back(op_load, hidden);
                return slot_type;
            }

            if (!slot_type.empty()) {
                function.code.emplace_back(op_declare_local, hidden, slot_type);
                function.code.emplace_back(op_store, hidden);
                local_variable_map[function][hidden] = slot_type;
                function.code.emplace_back(op_reference);
                function.code.emplace_back(op_load, hidden);
            }
            return slot_type;
        }

        if (node->get_type() == cst_type::identifier) {
            std::string var_type;
            if (auto it = local_variable_map[function].find(node->content);
                it != local_variable_map[function].end()) {
                var_type = it->second;
            }
            std::string base_type = var_type;
            if (base_type.ends_with("_ptr"))       base_type = base_type.substr(0, base_type.size() - 4);
            if (base_type.ends_with("_reference")) base_type = base_type.substr(0, base_type.size() - 10);

            const bool is_struct_like = custom_type_map.contains(base_type);
            const bool is_ptr_or_ref  = var_type.ends_with("_ptr") || var_type.ends_with("_reference");

            // array base: the local is really an array, not a variable
            if (auto ait = local_array_map[function].find(node->content);
                ait != local_array_map[function].end()) {
                function.code.emplace_back(op_reference);
                function.code.emplace_back(op_load, node->content);
                return ait->second;
            }

            if (is_struct_like || is_ptr_or_ref) {
                function.code.emplace_back(op_load, node->content);
                return base_type;
            }

            // plain scalar: push its address, not its value
            function.code.emplace_back(op_reference);
            function.code.emplace_back(op_load, node->content);
            return base_type;
        }

        if (node->get_type() == cst_type::memberaccess) {
            auto& ch = node->get_children();
            if (ch.empty()) return "";
            std::string cur_type = generate_lvalue_addr(function, ch[0].get());

            std::size_t last_field_idx = ch.size();
            for (std::size_t i = ch.size(); i-- > 1; ) {
                if (ch[i]->get_type() != cst_type::assignment) { last_field_idx = i; break; }
            }
            for (std::size_t i = 1; i < ch.size(); i++) {
                if (ch[i]->get_type() == cst_type::assignment) continue;
                const std::string field_name = ch[i]->content;
                const bool last = (i == last_field_idx);

                // find field type in current struct, descending through union groups
                std::string next_type;
                auto field_type_in = [this, &field_name](cst* parent) -> std::string {
                    std::function<std::string(cst*)> go = [&](cst* p) -> std::string {
                        for (const auto& mc : p->get_children()) {
                            cst* probe = mc.get();
                            if (probe->get_type() == cst_type::union_group) {
                                auto inner = go(probe);
                                if (!inner.empty()) return inner;
                                continue;
                            }
                            if (probe->get_type() == cst_type::array
                                && !probe->get_children().empty()) {
                                cst* elem = probe->get_children().front().get();
                                if (!elem->get_children().empty()
                                    && elem->get_children().front()->content == field_name) {
                                    return member_cst_type_name(elem);
                                }
                                continue;
                            }
                            if (!probe->get_children().empty()
                                && probe->get_children().front()->content == field_name) {
                                return member_cst_type_name(probe);
                            }
                        }
                        return "";
                    };
                    return go(parent);
                };
                if (auto st = custom_type_map.find(cur_type); st != custom_type_map.end()) {
                    next_type = field_type_in(st->second);
                }

                function.code.emplace_back(last ? op_member_addr : op_member_access, field_name);
                function.code.back().owner = cur_type;
                cur_type = next_type;
            }
            return cur_type;
        }

        if (node->get_type() == cst_type::arrayaccess) {
            auto& ch = node->get_children();
            if (ch.empty()) return "";
            // base address of the array (first element)
            std::string elem_type = generate_lvalue_addr(function, ch[0].get());

            std::size_t last_idx = ch.size();
            for (std::size_t i = ch.size(); i-- > 1; ) {
                if (ch[i]->get_type() != cst_type::assignment) { last_idx = i + 1; break; }
            }
            for (std::size_t i = 1; i < last_idx; i++) {
                cst* c = ch[i].get();
                switch (c->get_type()) {
                case cst_type::number_literal:
                    function.code.emplace_back(op_push, from_numerical_string<std::int64_t>(c->content));
                    break;
                case cst_type::identifier:
                    function.code.emplace_back(op_load, c->content);
                    break;
                default:
                    generate_common_generic<std::int64_t>(function, c);
                    break;
                }
                function.code.emplace_back(op_sizeof, elem_type);
                function.code.emplace_back(op_imul);
                function.code.emplace_back(op_add);
            }
            return elem_type;
        }

        return "";
    }

    static bool ir_is_integer_leaf(const std::string& t) {
        return t == "int8"  || t == "int16"  || t == "int32"  || t == "int64"
            || t == "uint8" || t == "uint16" || t == "uint32" || t == "uint64"
            || t == "bool";
    }
    static bool ir_is_float_leaf(const std::string& t) {
        return t == "float32" || t == "float64";
    }

    void ir_gen::emit_load_through_addr(ir_function& function, const std::string& type) {
        if (type.empty() || custom_type_map.contains(type)) {
            // struct leaf: the address itself is the result
            return;
        }
        function.code.emplace_back(op_pointee, type);
        function.code.emplace_back(op_dereference, std::int64_t(1));
    }

    void ir_gen::emit_store_through_addr(ir_function& function, cst* assignment_node, const std::string& type) {
        // generate the rhs value onto the appropriate stack
        if (ir_is_float_leaf(type)) {
            generate_common_generic<double>(function, assignment_node, type);
        }
        else {
            generate_common_generic<std::int64_t>(function, assignment_node);
        }
        function.code.emplace_back(op_pointee, type);
        function.code.emplace_back(op_store_at_addr);
    }

    static std::string member_cst_type_name_impl(cst* member) {
        if (!member) return "";
        if (member->get_type() == cst_type::array
            && !member->get_children().empty()) {
            return member_cst_type_name_impl(member->get_children().front().get());
        }
        switch (member->get_type()) {
        case cst_type::int8_datatype:    return "int8";
        case cst_type::int16_datatype:   return "int16";
        case cst_type::int32_datatype:   return "int32";
        case cst_type::int64_datatype:   return "int64";
        case cst_type::uint8_datatype:   return "uint8";
        case cst_type::uint16_datatype:  return "uint16";
        case cst_type::uint32_datatype:  return "uint32";
        case cst_type::uint64_datatype:  return "uint64";
        case cst_type::float32_datatype: return "float32";
        case cst_type::float64_datatype: return "float64";
        case cst_type::bool_datatype:    return "int8";
        case cst_type::string_datatype:  return "str";
        case cst_type::structure:        return member->content;
        default:                         return "";
        }
    }

    // bridge: the lambda above sees a free function with the right name
    std::string ir_gen::member_cst_type_name(cst* member) {
        return member_cst_type_name_impl(member);
    }

    void ir_gen::generate_member_access(ir_function& function, cst_memberaccess* member_access_node) {
        const auto& children = member_access_node->get_children();

        if (children.empty()) {
            throw std::runtime_error("Member access with no children");
        }

        if (is_composite_access(member_access_node)) {
            cst* assignment_node = nullptr;
            if (!children.empty() && children.back()->get_type() == cst_type::assignment) {
                assignment_node = children.back().get();
            }
            bool want_address_only = false;
            if (!function.code.empty() && function.code.back().op == op_reference) {
                function.code.pop_back();
                want_address_only = true;
            }
            const std::string leaf_type = generate_lvalue_addr(function, member_access_node);
            if (assignment_node) {
                emit_store_through_addr(function, assignment_node, leaf_type);
            }
            else if (!want_address_only) {
                emit_load_through_addr(function, leaf_type);
            }
            return;
        }

        std::string base_var = children[0]->content;
        std::vector<std::string> member_chain;
        bool is_assignment = false;
        cst* assignment_node = nullptr;
        bool is_deref = member_access_node->num_pointers > 0 || children[0]->num_pointers > 0;

        std::string base_struct_type;
        auto var_it_for_access = local_variable_map[function].find(base_var);
        if (var_it_for_access != local_variable_map[function].end()) {
            base_struct_type = var_it_for_access->second;
            if (base_struct_type.ends_with("_ptr")) {
                base_struct_type = base_struct_type.substr(0, base_struct_type.size() - 4);
            }
            if (base_struct_type.ends_with("_reference")) {
                base_struct_type = base_struct_type.substr(0, base_struct_type.size() - 10);
            }
        }

        auto owners_of = [&](const std::vector<std::string>& chain) {
            std::vector<std::string> owners(chain.size());
            std::string cur = base_struct_type;
            for (std::size_t ci = 0; ci < chain.size(); ci++) {
                auto st = custom_type_map.find(cur);
                if (st == custom_type_map.end()) {
                    break;
                }
                owners[ci] = cur;
                std::string next;
                for (const auto& mc : st->second->get_children()) {
                    if (mc->get_children().empty() || mc->get_children().front()->content != chain[ci]) {
                        continue;
                    }
                    if (mc->get_type() == cst_type::structure) {
                        next = mc->content;
                    }
                    break;
                }
                cur = next;
            }
            return owners;
        };

        for (size_t i = 1; i < children.size(); i++) {
            if (children[i]->get_type() == cst_type::assignment) {
                is_assignment = true;
                assignment_node = children[i].get();
            }
            else {
                member_chain.push_back(children[i]->content);
            }
        }

        if (debug) {
            std::cout << CYAN << "[IR GEN] Member " << (is_assignment ? "store: " : "access: ") << (is_deref ? "(deref) " : "") << RESET << base_var;
            for (const auto& member : member_chain) {
                std::cout << "." << member;
            }
            std::cout << std::endl;
        }

        if (is_assignment) {
            function.code.emplace_back(op_load, base_var);

            const auto chain_owners = owners_of(member_chain);
            for (size_t i = 0; i < member_chain.size() - 1; i++) {
                function.code.emplace_back(op_member_access, member_chain[i]);
                function.code.back().owner = chain_owners[i];
            }

            std::string member_type;
            auto var_it = local_variable_map[function].find(base_var);
            if (var_it != local_variable_map[function].end()) {
                std::string struct_type = var_it->second;
                if (struct_type.ends_with("_ptr")) {
                    struct_type = struct_type.substr(0, struct_type.size() - 4);
                }
                if (struct_type.ends_with("_reference")) {
                    struct_type = struct_type.substr(0, struct_type.size() - 10);
                }
                auto map_it = custom_type_map.find(struct_type);
                if (map_it != custom_type_map.end()) {
                    cst* current_struct = map_it->second;
                    for (size_t ci = 0; ci < member_chain.size(); ci++) {
                        for (const auto& mc : current_struct->get_children()) {
                            if (!mc->get_children().empty() && mc->get_children().front()->content == member_chain[ci]) {
                                if (ci == member_chain.size() - 1) {
                                    switch (mc->get_type()) {
                                    case cst_type::float32_datatype:
                                        member_type = "float32";
                                        break;
                                    case cst_type::float64_datatype:
                                        member_type = "float64";
                                        break;
                                    default:
                                        break;
                                    }
                                }
                                else if (mc->get_type() == cst_type::structure) {
                                    auto nested_it = custom_type_map.find(mc->content);
                                    if (nested_it != custom_type_map.end()) {
                                        current_struct = nested_it->second;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }

            if (member_type == "float32") {
                generate_common_generic<float>(function, assignment_node, "float32");
            }
            else if (member_type == "float64") {
                generate_common_generic<double>(function, assignment_node, "float64");
            }
            else {
                generate_common_generic<std::int64_t>(function, assignment_node);
            }

            if (!member_chain.empty()) {
                function.code.emplace_back(op_member_store, member_chain.back(), member_type);
                function.code.back().owner = chain_owners.back();
            }
        }
        else {
            bool address_of_member = false;
            if (!member_chain.empty() && !function.code.empty()
                && function.code.back().op == op_reference) {
                function.code.pop_back();
                address_of_member = true;
            }

            function.code.emplace_back(op_load, base_var);

            const auto chain_owners = owners_of(member_chain);
            for (size_t i = 0; i < member_chain.size(); i++) {
                const bool last = (i + 1 == member_chain.size());
                function.code.emplace_back(
                    (last && address_of_member) ? op_member_addr : op_member_access,
                    member_chain[i]);
                function.code.back().owner = chain_owners[i];
            }
        }
    }

    void ir_gen::generate_block(ir_function& function, cst_block* block_node, std::string current_break_label, std::string current_loop_start) {
        std::unordered_set<std::string> vars_before;
        if (auto it = local_variable_map.find(function); it != local_variable_map.end()) {
            for (const auto& [name, type] : it->second) {
                vars_before.insert(name);
            }
        }
        std::unordered_set<std::string> arrays_before;
        if (auto it = local_array_map.find(function); it != local_array_map.end()) {
            for (const auto& [name, type] : it->second) {
                arrays_before.insert(name);
            }
        }

        const bool is_top_level_block = defer_bodies.empty();
        defer_bodies.emplace_back();
        const std::size_t my_defer_depth = defer_bodies.size() - 1;

        for (const auto& c : block_node->get_children()) {
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
            case cst_type::string_datatype:
                {
                    const auto node = c.get();

                    std::string type_with_ptrs = c->to_string().substr(4, c->to_string().size());
                    if (node->num_pointers > 0) {
                        type_with_ptrs += "_ptr";
                    }

                    const auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get()); // name

                    if (!node->fnptr_sig.empty()) {
                        fnptr_sigs[function.name][identifier->content] = node->fnptr_sig;
                    }

                    if (node->get_children().size() > 1 && node->get_children().back()->get_type() == cst_type::assignment) {
                        const auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
                        generate_common(function, assignment,
                                        type_with_ptrs); // expression stuff
                        function.code.emplace_back(op_store, identifier->content, type_with_ptrs);
                    }
                    else if (node->fnptr_sig.empty()) {
                        function.code.emplace_back(op_declare_local, identifier->content, type_with_ptrs);
                    }

                    local_variable_map[function][identifier->content] = type_with_ptrs;

                    break;
                }
            case cst_type::bool_datatype:
                {
                    const auto node = c.get();

                    std::string type_with_ptrs = c->to_string().substr(4, c->to_string().size());
                    if (node->num_pointers > 0) {
                        type_with_ptrs += "_ptr";
                    }

                    const auto identifier = cst::cast_raw<cst_identifier>(node->get_children().front().get()); // name

                    if (node->get_children().size() > 1 && node->get_children().back()->get_type() == cst_type::assignment) {
                        const auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());
                        generate_common(function, assignment, type_with_ptrs,
                                        type_of_push::single); // expression stuff
                        function.code.emplace_back(op_store, identifier->content, type_with_ptrs);
                    }
                    else {
                        function.code.emplace_back(op_declare_local, identifier->content, type_with_ptrs);
                    }

                    local_variable_map[function][identifier->content] = type_with_ptrs;

                    break;
                }
            case cst_type::array:
                {
                    generate_array_decl(function, cst::cast_raw<cst_array>(c.get()));

                    break;
                }
            case cst_type::asm_code:
                {
                    for (auto& str_literal : c->get_children()) {
                        function.code.emplace_back(op_asm_code, str_literal->content, c->content); // type = architecture
                    }

                    break;
                }
            case cst_type::shellcode:
                {
                    for (auto& num_literal : c->get_children()) {
                        function.code.emplace_back(op_push_shellcode, from_numerical_string<std::uint8_t>(num_literal->content));
                    }

                    break;
                }
            case cst_type::arrayaccess:
                {
                    generate_array_access(function, cst::cast_raw<cst_arrayaccess>(c.get()));

                    break;
                }
            case cst_type::structure:
                {
                    generate_struct_decl(function, cst::cast_raw<cst_struct>(c.get()));

                    break;
                }
            case cst_type::enumeration:
                {
                    break;
                }
            case cst_type::identifier:
                {
                    if (debug) {
                        std::cout << CYAN << "[IR GEN] Loading: " << local_variable_map[function][c->content] << std::endl;
                    }
                    generate_common(function, c.get(), local_variable_map[function][c->content]);

                    function.code.emplace_back(op_store, c->content, local_variable_map[function][c->content]);

                    break;
                }
            // a string literal is a pointer to its bytes, pushed like a number
            case cst_type::stringliteral:
                function.code.emplace_back(op_push, c->content);
                break;
            case cst_type::cast_to_datatype:
                {
                    // the default below dispatches operator nodes only
                    generate_cast<std::int64_t>(function, c.get());
                    break;
                }
            case cst_type::dereference:
                {
                    // i think this shgould be good enough
                    auto node = c.get();

                    auto expr = cst::cast_raw<cst_generic_expr>(node->get_children().front().get());
                    auto assignment = cst::cast_raw<cst_assignment>(node->get_children().back().get());

                    auto deref_count = from_numerical_string<std::int64_t>(node->content);

                    function.code.emplace_back(op_dereference_assign, deref_count);
                    generate_common(function, expr, "int64");
                    // the value is written at the pointer's own type
                    std::string store_type = "int64";
                    for (const auto& ec : expr->get_children()) {
                        if (ec->get_type() == cst_type::cast_to_datatype && ec->num_pointers > 0) {
                            if (ec->content == "f64") store_type = "float64";
                            else if (ec->content == "f32") store_type = "float32";
                            else store_type = "int64";
                        }
                    }
                    if (assignment) {
                        for (const auto& ac : assignment->get_children()) {
                            switch (ac->get_type()) {
                            case cst_type::number_literal:
                            case cst_type::float_literal:
                            case cst_type::charliteral:
                                handle_push_types(function, ac.get(), store_type);
                                break;
                            case cst_type::identifier:
                                function.code.emplace_back(op_load, ac->content);
                                break;
                            case cst_type::functioncall:
                                generate_function_call(function, ac.get());
                                break;
                            case cst_type::arrayaccess:
                                generate_array_access(function, cst::cast_raw<cst_arrayaccess>(ac.get()));
                                break;
                            case cst_type::memberaccess:
                                generate_member_access(function, cst::cast_raw<cst_memberaccess>(ac.get()));
                                break;
                            // a string literal is a pointer to its bytes, pushed like a number
                            case cst_type::stringliteral:
                                function.code.emplace_back(op_push, ac->content);
                                break;
                            case cst_type::cast_to_datatype:
                                {
                                    // the default below dispatches operator nodes only
                                    generate_cast<std::int64_t>(function, ac.get());
                                    break;
                                }
                            case cst_type::dereference:
                                function.code.emplace_back(op_dereference, from_numerical_string<std::int64_t>(ac->content));
                                break;
                            case cst_type::reference:
                                function.code.emplace_back(op_reference); // $p = @x / $p = @fn
                                break;
                            default:
                                generate_arith_and_bitwise_operators(function, ac.get(), store_type);
                                break;
                            }
                        }
                    }

                    for (const auto& ec : expr->get_children()) {
                        if (ec->get_type() == cst_type::cast_to_datatype && ec->num_pointers > 0) {
                            function.code.emplace_back(op_pointee, ec->content);
                        }
                    }

                    function.code.emplace_back(op_store_at_addr);

                    break;
                }
            case cst_type::functioncall:
                {
                    generate_function_call(function, c.get(), false);

                    break;
                }
            case cst_type::ifstmt:
                {
                    generate_if(function, cst::cast_raw<cst_ifstmt>(c.get()), current_break_label, current_loop_start);

                    break;
                }
            case cst_type::continuestmt:
                {
                    if (!try_finally_stack.empty()) {
                        const auto& ctx = try_finally_stack.back();
                        function.code.emplace_back(op_push, std::int64_t(3));
                        function.code.emplace_back(op_store, ctx.exit_var);
                        function.code.emplace_back(op_jmp, ctx.finally_label);
                    }
                    else {
                        if (!loop_defer_depths.empty()) {
                            emit_defers_above(function, loop_defer_depths.back());
                        }
                        function.code.emplace_back(op_jmp, current_loop_start);
                    }
                    break;
                }
            case cst_type::breakstmt:
                {
                    if (!try_finally_stack.empty()) {
                        const auto& ctx = try_finally_stack.back();
                        function.code.emplace_back(op_push, std::int64_t(2));
                        function.code.emplace_back(op_store, ctx.exit_var);
                        function.code.emplace_back(op_jmp, ctx.finally_label);
                    }
                    else {
                        if (!loop_defer_depths.empty()) {
                            emit_defers_above(function, loop_defer_depths.back());
                        }
                        function.code.emplace_back(op_jmp, current_break_label);
                    }
                    break;
                }
            case cst_type::labelstmt:
                {
                    place_label(function, user_label(c->content));

                    break;
                }
            case cst_type::gotostmt:
                {
                    if (!try_finally_stack.empty()) {
                        auto& ctx = try_finally_stack.back();
                        auto& codes = *ctx.goto_codes;
                        auto it = codes.find(c->content);
                        if (it == codes.end()) {
                            it = codes.emplace(c->content, static_cast<std::int64_t>(4 + codes.size())).first;
                        }
                        function.code.emplace_back(op_push, it->second);
                        function.code.emplace_back(op_store, ctx.exit_var);
                        function.code.emplace_back(op_jmp, ctx.finally_label);
                    }
                    else {
                        function.code.emplace_back(op_jmp, user_label(c->content));
                    }
                    break;
                }
            case cst_type::loopstmt:
                {
                    generate_loop(function, cst::cast_raw<cst_loopstmt>(c.get()));

                    break;
                }
            case cst_type::whilestmt:
                {
                    generate_while(function, cst::cast_raw<cst_whilestmt>(c.get()));

                    break;
                }
            case cst_type::forstmt:
                {
                    generate_for(function, cst::cast_raw<cst_forstmt>(c.get()));

                    break;
                }
            case cst_type::switchstmt:
                {
                    generate_switch(function, cst::cast_raw<cst_switchstmt>(c.get()), current_break_label, current_loop_start);

                    break;
                }
            case cst_type::trystmt:
                {
                    generate_try(function, c.get(), current_break_label, current_loop_start);

                    break;
                }
            case cst_type::deferstmt:
                {
                    if (!c->get_children().empty() && !defer_bodies.empty()) {
                        defer_bodies.back().push_back(c->get_children().front().get());
                    }
                    break;
                }
            case cst_type::returnstmt:
                {
                    generate_return(function, cst::cast_raw<cst_returnstmt>(c.get()));

                    break;
                }
            case cst_type::memberaccess:
                {
                    generate_member_access(function, cst::cast_raw<cst_memberaccess>(c.get()));

                    break;
                }
            default:
                {
                    break;
                }
            }
        }

        if (!is_top_level_block && defer_bodies.size() > my_defer_depth) {
            auto& my_frame = defer_bodies.back();
            for (auto it = my_frame.rbegin(); it != my_frame.rend(); ++it) {
                if (auto* block = cst::cast_raw<cst_block>(*it)) {
                    generate_block(function, block, std::string(), std::string());
                }
            }
            defer_bodies.pop_back();
        }

        // remove variables/arrays declared inside this block (scope cleanup).
        if (auto it = local_variable_map.find(function); it != local_variable_map.end()) {
            for (auto var_it = it->second.begin(); var_it != it->second.end();) {
                if (!vars_before.count(var_it->first)) {
                    var_it = it->second.erase(var_it);
                }
                else {
                    ++var_it;
                }
            }
        }
        if (auto it = local_array_map.find(function); it != local_array_map.end()) {
            for (auto arr_it = it->second.begin(); arr_it != it->second.end();) {
                if (!arrays_before.count(arr_it->first)) {
                    arr_it = it->second.erase(arr_it);
                }
                else {
                    ++arr_it;
                }
            }
        }
    }

    std::string ir_gen::user_label(const std::string& name) { return "user_" + name; }

    std::string ir_gen::create_label() {
        std::string label_name = "label_" + std::to_string(label_count++);
        label_map[label_name] = label_count;

        return label_name;
    }

    void ir_gen::place_label(ir_function& function, std::string label_name) { function.code.emplace_back(label, label_name); }

    void ir_gen::visualize_stack_ir(const std::vector<ir_function>& funcs) {
        std::cout << CYAN << "Function(s): \n" << RESET;
        for (const auto& [code, args, name, type, return_fnptr_sig, uses_shellcode, is_external, is_variadic, uses_assembly, library] : funcs) {
            (void)return_fnptr_sig;
            if (!library.empty()) {
                std::cout << "Library: " << library << "\n";
            }
            std::cout << "Type: " << type << "\n";
            std::string sh = uses_shellcode ? "True" : "False";
            std::string sh_asm = uses_assembly ? "True" : "False";
            std::cout << "Shellcode: " << sh << "\n";
            std::cout << "Assembly: " << sh_asm << "\n";
            std::cout << "Name: " << name << "\n";

            std::cout << "Args:\n";
            for (auto& arg : args) {
                std::cout << "  " << arg.type << "\n";
                std::cout << "  " << arg.name << "\n";
            }

            std::cout << "Code:\n";
            for (auto& i : code) {
                std::cout << "  " << occult::opcode_to_string(i.op) << " ";
                std::visit(visitor_stack(), i.operand);

                if (!i.type.empty()) {
                    std::cout << "\t (type = " << i.type << ")\n";
                }
            }
            std::cout << "\n";
        }
    }

    void ir_gen::visualize_structs(const std::vector<ir_struct>& structs) {
        std::cout << CYAN << "Struct(s): \n" << RESET;
        for (auto& s : structs) {
            std::cout << "Type: " << s.datatype << "\n";

            std::cout << "Members: \n";
            for (auto& m : s.members) {
                std::cout << "  " << m.datatype << "\n";
                std::cout << "  " << m.name << "\n";
            }
            std::cout << "\n";
        }
    }

    std::vector<ir_function> ir_gen::lower_functions() {
        try {
            std::vector<ir_function> functions;

            std::vector<cst*> global_vars;
            std::unordered_map<std::string, std::string> global_var_types; // name -> type
            std::vector<std::pair<std::string, std::string>> runtime_globals;
            for (const auto& c : root->get_children()) {
                const auto type = c->get_type();
                if (type == cst_type::int8_datatype || type == cst_type::int16_datatype || type == cst_type::int32_datatype || type == cst_type::int64_datatype || type == cst_type::uint8_datatype || type == cst_type::uint16_datatype ||
                    type == cst_type::uint32_datatype || type == cst_type::uint64_datatype || type == cst_type::float32_datatype || type == cst_type::float64_datatype || type == cst_type::string_datatype ||
                    type == cst_type::bool_datatype ||
                    (type == cst_type::structure && c->num_pointers > 0 && !c->get_children().empty() &&
                     c->get_children().front()->get_type() == cst_type::identifier)) {

                    global_vars.push_back(c.get());

                    std::string type_with_ptrs = (type == cst_type::structure) ? c->content : c->to_string().substr(4, c->to_string().size());
                    if (c->num_pointers > 0) {
                        type_with_ptrs += "_ptr";
                    }
                    if (!c->get_children().empty()) {
                        const auto id = cst::cast_raw<cst_identifier>(c->get_children().front().get());
                        if (id) {
                            if (id->content.rfind("__occult_err_", 0) == 0) {
                                runtime_globals.emplace_back(id->content, type_with_ptrs);
                            }
                            global_var_types[id->content] = type_with_ptrs;
                            if (!c->fnptr_sig.empty()) {
                                global_fnptr_sigs[id->content] = c->fnptr_sig;
                            }
                        }
                    }
                }
            }

            program_globals.clear();
            for (const auto& [gname, gtype] : global_var_types) {
                if (gname.rfind("__occult_err_", 0) != 0) {
                    program_globals.emplace_back(gname, gtype);
                }
            }
            program_globals.insert(program_globals.end(), runtime_globals.begin(), runtime_globals.end());

            if (!global_vars.empty()) {
                ir_function init_func;
                init_func.name = "__global_init";
                init_func.type = "int64";

                for (const auto& [gname, gtype] : global_var_types) {
                    local_variable_map[init_func][gname] = gtype;
                }

                for (auto* gv : global_vars) {
                    std::string gv_type = (gv->get_type() == cst_type::structure) ? gv->content : gv->to_string().substr(4, gv->to_string().size());
                    if (gv->num_pointers > 0) {
                        gv_type += "_ptr";
                    }

                    if (gv->get_children().size() > 1 && gv->get_children().back()->get_type() == cst_type::assignment) {
                        const auto identifier = cst::cast_raw<cst_identifier>(gv->get_children().front().get());
                        const auto assignment = cst::cast_raw<cst_assignment>(gv->get_children().back().get());
                        generate_common(init_func, assignment, gv_type);
                        init_func.code.emplace_back(op_store, identifier->content, gv_type);
                    }
                }

                init_func.code.emplace_back(op_ret);
                func_map.emplace(init_func.name, init_func);
                functions.emplace_back(init_func);
            }

            for (const auto& c : root->get_children()) {
                if (const auto type = c->get_type(); type == cst_type::function) {
                    if (!global_var_types.empty()) {
                        for (const auto& ch : c->get_children()) {
                            if (ch->get_type() == cst_type::identifier) {
                                ir_function key;
                                key.name = ch->content;
                                for (const auto& [gname, gtype] : global_var_types) {
                                    local_variable_map[key][gname] = gtype;
                                }
                                break;
                            }
                        }
                    }

                    auto func = generate_function(cst::cast_raw<cst_function>(c.get()));
                    functions.emplace_back(std::move(func));
                }
            }

            return functions;
        }
        catch (std::exception& e) {
            std::cerr << RED << e.what() << RESET << std::endl;
            return std::vector<ir_function>();
        }
    }

    std::vector<ir_struct> ir_gen::lower_structs() {
        try {
            std::vector<ir_struct> structs;

            auto handle_struct = [this](const std::string& name, cst* child) -> ir_struct {
                ir_struct ir_s;

                ir_s.datatype = name;
                std::int32_t next_union_group = 0;

                auto emit_member = [&](cst* c, std::int32_t group) {
                    if (c->get_type() == cst_type::array) {
                        auto& kids = c->get_children();
                        if (kids.size() < 2) return;
                        cst* elem = kids[0].get();
                        cst* dims = kids[1].get();
                        std::int32_t count = 1;
                        for (auto& d : dims->get_children()) {
                            count *= static_cast<std::int32_t>(std::stoi(d->content));
                        }
                        std::string elem_type;
                        switch (elem->get_type()) {
                        case cst_type::int8_datatype:   elem_type = "int8";   break;
                        case cst_type::int16_datatype:  elem_type = "int16";  break;
                        case cst_type::int32_datatype:  elem_type = "int32";  break;
                        case cst_type::int64_datatype:  elem_type = "int64";  break;
                        case cst_type::uint8_datatype:  elem_type = "uint8";  break;
                        case cst_type::uint16_datatype: elem_type = "uint16"; break;
                        case cst_type::uint32_datatype: elem_type = "uint32"; break;
                        case cst_type::uint64_datatype: elem_type = "uint64"; break;
                        case cst_type::float32_datatype: elem_type = "float32"; break;
                        case cst_type::float64_datatype: elem_type = "float64"; break;
                        case cst_type::bool_datatype:   elem_type = "int8";   break;
                        case cst_type::string_datatype: elem_type = "str";    break;
                        default:
                            // Custom type: element type is the struct name.
                            elem_type = elem->content;
                            break;
                        }
                        const std::string member_name = elem->get_children().empty() ? std::string() : elem->get_children().front()->content;
                        ir_s.members.emplace_back(elem_type, member_name, group, count);
                        return;
                    }
                    const auto push = [&](const std::string& t) {
                        ir_s.members.emplace_back(t, c->get_children().front()->content, group);
                    };
                    switch (c->get_type()) {
                    case cst_type::int8_datatype:   push("int8");   return;
                    case cst_type::int16_datatype:  push("int16");  return;
                    case cst_type::int32_datatype:  push("int32");  return;
                    case cst_type::int64_datatype:  push("int64");  return;
                    case cst_type::uint8_datatype:  push("uint8");  return;
                    case cst_type::uint16_datatype: push("uint16"); return;
                    case cst_type::uint32_datatype: push("uint32"); return;
                    case cst_type::uint64_datatype: push("uint64"); return;
                    case cst_type::float32_datatype: push("float32"); return;
                    case cst_type::float64_datatype: push("float64"); return;
                    case cst_type::string_datatype: push("str");     return;
                    default: break;
                    }
                    if (c->get_children().size() == 1 && custom_type_map.contains(c->content) && custom_type_map[c->content]->get_type() == cst_type::enumeration) {
                        ir_s.members.emplace_back("int64", c->get_children().front()->content, group);
                        return;
                    }
                    if (c->get_children().size() == 1) {
                        if (ir_s.datatype != c->content) {
                            if (c->num_pointers > 0) {
                                ir_s.members.emplace_back(c->content += "_stuct_ptr", c->get_children().front()->content, group);
                            }
                            else {
                                ir_s.members.emplace_back(c->content, c->get_children().front()->content, group);
                            }
                        }
                        else {
                            if (c->num_pointers > 0) {
                                ir_s.members.emplace_back(c->content += "_stuct_ptr", c->get_children().front()->content, group);
                            }
                            else {
                                throw std::runtime_error("[IR ERR] Struct can't have nested type without ptr (sz = " + std::to_string(c->num_pointers) + ")");
                            }
                        }
                    }
                };

                for (const auto& c : child->get_children()) {
                    if (c->get_type() == cst_type::struct_is_opaque) {
                        ir_s.opaque = true;
                        continue;
                    }
                    if (c->get_type() == cst_type::struct_is_error) {
                        continue;   // handled by the error pass
                    }
                    if (c->get_type() == cst_type::union_group) {
                        ++next_union_group;
                        for (auto& m : c->get_children()) {
                            emit_member(m.get(), next_union_group);
                        }
                        continue;
                    }
                    emit_member(c.get(), 0);
                }

                return ir_s;
            };

            for (auto& [name, child] : custom_type_map) {
                if (child->get_type() == cst_type::enumeration) {
                    continue; // skip enums, they are not structs
                }
                structs.emplace_back(handle_struct(name, child));
            }

            return structs;
        }
        catch (std::exception& e) {
            std::cerr << RED << e.what() << RESET << std::endl;
            return std::vector<ir_struct>();
        }
    }
} // namespace occult
