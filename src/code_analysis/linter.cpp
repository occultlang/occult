#include "linter.hpp"
#include <iostream>
#include "../util/color_defs.hpp"

namespace occult {
    void linter::push_scope() { scope_stack.emplace_back(); }

    void linter::pop_scope() {
        if (!scope_stack.empty()) {
            scope_stack.pop_back();
        }
    }

    bool linter::declare_var(const std::string& name, const std::string& type) {
        if (scope_stack.empty())
            return false;
        auto& top = scope_stack.back();
        if (top.count(name)) {
            errors.emplace_back("Variable '" + name + "' redeclared in the same scope");
            return false;
        }
        top[name] = type;
        if (debug) {
            std::cout << CYAN << "[LINTER] Declared '" << name << "' : " << type << RESET << "\n";
        }
        return true;
    }

    bool linter::is_declared(const std::string& name) const {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
            if (it->count(name))
                return true;
        }
        return false;
    }

    std::string linter::lookup_type(const std::string& name) const {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
            if (auto f = it->find(name); f != it->end()) {
                return f->second;
            }
        }
        return "";
    }

    std::string linter::type_name_of(cst* node) {
        std::string base;
        switch (node->get_type()) {
        case cst_type::int8_datatype:
            base = "int8";
            break;
        case cst_type::int16_datatype:
            base = "int16";
            break;
        case cst_type::int32_datatype:
            base = "int32";
            break;
        case cst_type::int64_datatype:
            base = "int64";
            break;
        case cst_type::uint8_datatype:
            base = "uint8";
            break;
        case cst_type::uint16_datatype:
            base = "uint16";
            break;
        case cst_type::uint32_datatype:
            base = "uint32";
            break;
        case cst_type::uint64_datatype:
            base = "uint64";
            break;
        case cst_type::float32_datatype:
            base = "float32";
            break;
        case cst_type::float64_datatype:
            base = "float64";
            break;
        case cst_type::string_datatype:
            base = "str";
            break;
        case cst_type::bool_datatype:
            base = "bool";
            break;
        default:
            base = node->content;
            break;
        }
        base.append(node->num_pointers, '*');
        return base;
    }

    std::string linter::keyword_to_type_name(const std::string& kw) {
        static const std::unordered_map<std::string, std::string> map = {
            {"i8", "int8"},    {"char", "int8"},  {"i16", "int16"},   {"i32", "int32"},   {"i64", "int64"}, {"u8", "uint8"},   {"u16", "uint16"},
            {"u32", "uint32"}, {"u64", "uint64"}, {"f32", "float32"}, {"f64", "float64"}, {"bool", "bool"}, {"string", "str"},
        };
        auto it = map.find(kw);
        return (it != map.end()) ? it->second : kw;
    }

    bool linter::is_type_node(cst_type t) {
        switch (t) {
        case cst_type::int8_datatype:
        case cst_type::int16_datatype:
        case cst_type::int32_datatype:
        case cst_type::int64_datatype:
        case cst_type::uint8_datatype:
        case cst_type::uint16_datatype:
        case cst_type::uint32_datatype:
        case cst_type::uint64_datatype:
        case cst_type::float32_datatype:
        case cst_type::float64_datatype:
        case cst_type::string_datatype:
        case cst_type::bool_datatype:
            return true;
        default:
            return false;
        }
    }

    bool linter::types_compatible(const std::string& a, const std::string& b) const {
        if (a.empty() || b.empty())
            return true;
        if (a == b)
            return true;

        static const std::unordered_set<std::string> all_numeric = {"int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64", "float32", "float64", "bool"};
        static const std::unordered_set<std::string> float_types = {"float32", "float64"};
        static const std::unordered_set<std::string> int_types = {"int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64"};

        if (a == "<number>" && all_numeric.count(b))
            return true;
        if (b == "<number>" && all_numeric.count(a))
            return true;
        if (a == "<number>" && !b.empty() && b.back() == '*')
            return true;
        if (b == "<number>" && !a.empty() && a.back() == '*')
            return true;

        if (a == "<float>" && float_types.count(b))
            return true;
        if (b == "<float>" && float_types.count(a))
            return true;

        if (a == "<str>" && (b == "str" || int_types.count(b)))
            return true;
        if (b == "<str>" && (a == "str" || int_types.count(a)))
            return true;

        if (a == "bool" && int_types.count(b))
            return true;
        if (b == "bool" && int_types.count(a))
            return true;

        if (a == "str" && int_types.count(b))
            return true;
        if (b == "str" && int_types.count(a))
            return true;

        if (known_enum_types.count(a) && (b == "<number>" || int_types.count(b)))
            return true;
        if (known_enum_types.count(b) && (a == "<number>" || int_types.count(a)))
            return true;
        if (known_enum_types.count(a) && known_enum_types.count(b))
            return true;

        // In Occult's low-level memory model, 64-bit integer values are compatible
        // with pointer types (pointers are 64-bit addresses). This allows patterns
        // like reading a raw pointer from an i64* array slot and storing it in a
        // typed struct pointer variable (e.g. HashEntry* p = $raw_slot).
        auto ends_with_ptr = [](const std::string& s) { return !s.empty() && s.back() == '*'; };
        if ((a == "int64" || a == "uint64") && ends_with_ptr(b))
            return true;
        if ((b == "int64" || b == "uint64") && ends_with_ptr(a))
            return true;

        if (ends_with_ptr(a) && ends_with_ptr(b))
            return true;

        // All integer types are mutually compatible (implicit widening/narrowing).
        // This is natural in a low-level language where the underlying storage is
        // always 64-bit and the programmer controls the bit width explicitly.
        if (int_types.count(a) && int_types.count(b))
            return true;

        return false;
    }

    std::string linter::infer_type_rpn(const std::vector<cst*>& nodes) {
        std::vector<std::string> stk;
        std::vector<bool> is_zero; // parallel stack tracking literal-zero values

        for (std::size_t i = 0; i < nodes.size(); ++i) {
            cst* node = nodes[i];
            if (!node)
                continue;

            switch (node->get_type()) {
            case cst_type::number_literal:
                stk.push_back("<number>");
                is_zero.push_back(node->content == "0");
                break;
            case cst_type::float_literal:
                stk.push_back("<float>");
                {
                    // detect zero-valued float literals (0.0, 0., .0, 0e0, etc.)
                    bool fzero = false;
                    try {
                        fzero = std::stod(node->content) == 0.0;
                    }
                    catch (...) {
                    }
                    is_zero.push_back(fzero);
                }
                break;
            case cst_type::stringliteral:
                stk.push_back("<str>");
                is_zero.push_back(false);
                break;
            case cst_type::charliteral:
                stk.push_back("<number>");
                is_zero.push_back(false);
                break;

            case cst_type::identifier:
                stk.push_back(lookup_type(node->content));
                is_zero.push_back(false);
                break;

            case cst_type::reference:
                {
                    int count = 0;
                    try {
                        count = std::stoi(node->content);
                    }
                    catch (...) {
                    }
                    if (i + 1 < nodes.size()) {
                        ++i;
                        cst* next = nodes[i];
                        if (next && next->get_type() == cst_type::identifier) {
                            std::string t = lookup_type(next->content);
                            if (!t.empty())
                                t.append(count, '*');
                            stk.push_back(t);
                        }
                        else {
                            stk.push_back("");
                        }
                    }
                    else {
                        stk.push_back("");
                    }
                    is_zero.push_back(false);
                    break;
                }

            case cst_type::dereference:
                {
                    int count = 0;
                    try {
                        count = std::stoi(node->content);
                    }
                    catch (...) {
                    }
                    if (!stk.empty()) {
                        std::string t = stk.back();
                        stk.pop_back();
                        is_zero.pop_back();
                        for (int j = 0; j < count && !t.empty() && t.back() == '*'; ++j) {
                            t.pop_back();
                        }
                        stk.push_back(t);
                    }
                    else {
                        stk.push_back("");
                    }
                    is_zero.push_back(false);
                    break;
                }

            case cst_type::cast_to_datatype:
                stk.push_back(keyword_to_type_name(node->content));
                is_zero.push_back(false);
                break;

            case cst_type::functioncall:
                {
                    std::string ret_type;
                    if (!node->get_children().empty()) {
                        const auto& first = node->get_children().front();
                        if (first->get_type() == cst_type::identifier) {
                            auto it = function_return_types.find(first->content);
                            if (it != function_return_types.end()) {
                                ret_type = it->second;
                            }
                        }
                    }
                    stk.push_back(ret_type);
                    is_zero.push_back(false);
                    break;
                }
            case cst_type::memberaccess:
                stk.push_back("");
                is_zero.push_back(false);
                break;

            case cst_type::arrayaccess:
                {
                    std::string elem;
                    if (!node->get_children().empty()) {
                        const auto& base = node->get_children().front();
                        if (base && base->get_type() == cst_type::identifier) {
                            std::string t = lookup_type(base->content);
                            if (t.size() >= 2 && t.substr(t.size() - 2) == "[]") {
                                elem = t.substr(0, t.size() - 2);
                            }
                        }
                    }
                    stk.push_back(elem);
                    is_zero.push_back(false);
                    break;
                }

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
                {
                    if (stk.size() >= 2) {
                        std::string rhs = stk.back();
                        stk.pop_back();
                        bool rhs_zero = is_zero.back();
                        is_zero.pop_back();
                        std::string lhs = stk.back();
                        stk.pop_back();
                        is_zero.pop_back();

                        if (rhs_zero && (node->get_type() == cst_type::division_operator || node->get_type() == cst_type::modulo_operator)) {
                            errors.emplace_back("Division by zero", lint_error::severity::warning);
                        }

                        if (lhs.empty() || rhs.empty()) {
                            stk.push_back("");
                        }
                        else if (lhs == "<number>" || lhs == "<float>") {
                            stk.push_back(rhs);
                        }
                        else if (rhs == "<number>" || rhs == "<float>") {
                            stk.push_back(lhs);
                        }
                        else {
                            stk.push_back(lhs);
                        }
                    }
                    else {
                        stk.push_back("");
                    }
                    is_zero.push_back(false);
                    break;
                }

            case cst_type::equals_operator:
            case cst_type::not_equals_operator:
            case cst_type::greater_than_operator:
            case cst_type::less_than_operator:
            case cst_type::greater_than_or_equal_operator:
            case cst_type::less_than_or_equal_operator:
                {
                    if (stk.size() >= 2) {
                        std::string rhs = stk.back();
                        stk.pop_back();
                        is_zero.pop_back();
                        std::string lhs = stk.back();
                        stk.pop_back();
                        is_zero.pop_back();
                        if (!types_compatible(lhs, rhs)) {
                            errors.emplace_back("Type mismatch in comparison: cannot compare '" + lhs + "' with '" + rhs + "'");
                        }
                    }
                    stk.push_back("bool");
                    is_zero.push_back(false);
                    break;
                }

            case cst_type::and_operator:
            case cst_type::or_operator:
                if (stk.size() >= 2) {
                    stk.pop_back();
                    stk.pop_back();
                    is_zero.pop_back();
                    is_zero.pop_back();
                }
                stk.push_back("bool");
                is_zero.push_back(false);
                break;

            case cst_type::unary_minus_operator:
            case cst_type::unary_plus_operator:
            case cst_type::unary_bitwise_not:
                break;
            case cst_type::unary_not_operator:
                if (!stk.empty()) {
                    stk.pop_back();
                    is_zero.pop_back();
                }
                stk.push_back("bool");
                is_zero.push_back(false);
                break;

            case cst_type::expr_end:
            case cst_type::expr_start:
                break;

            default:
                stk.push_back("");
                is_zero.push_back(false);
                break;
            }
        }
        return stk.empty() ? "" : stk.back();
    }

    void linter::collect_functions() {
        // register compiler built-in functions
        static const char* builtins[] = {
            "print_string", "print_integer", "print_newline", "print_char", "alloc", "del", "__bitcast_f64", "__bitcast_i64",
        };
        for (const auto* name : builtins) {
            known_functions.insert(name);
        }

        // built-in return types (alloc/del have context-dependent return types)
        function_return_types["__bitcast_f64"] = "float64";
        function_return_types["__bitcast_i64"] = "int64";

        // built-in parameter counts
        function_param_counts["print_string"] = 1;
        function_param_counts["print_integer"] = 1;
        function_param_counts["print_newline"] = 0;
        function_param_counts["print_char"] = 1;
        function_param_counts["alloc"] = 1;
        function_param_counts["del"] = 1;
        function_param_counts["__bitcast_f64"] = 1;
        function_param_counts["__bitcast_i64"] = 1;

        for (const auto& c : root->get_children()) {
            if (c->get_type() == cst_type::enumeration) {
                known_enum_types.insert(c->content);
                continue;
            }
            if (c->get_type() == cst_type::moduledecl || c->get_type() == cst_type::importdecl) {
                continue;
            }
            if (c->get_type() == cst_type::function) {
                std::string func_name;
                std::string return_type;

                for (const auto& child : c->get_children()) {
                    if (func_name.empty()) {
                        if (child->get_type() == cst_type::identifier) {
                            func_name = child->content;
                            known_functions.insert(func_name);
                        }
                        continue;
                    }
                    if (child->get_type() == cst_type::functionarguments) {
                        int real_param_count = 0;
                        bool is_variadic = false;
                        for (const auto& arg : child->get_children()) {
                            if (arg->get_type() == cst_type::variadic) {
                                is_variadic = true;
                                break;
                            }
                            // skip synthetic variadic parameters (prefixed with "__va")
                            static constexpr const char* VARIADIC_PARAM_PREFIX = "__va";
                            bool is_synthetic = false;
                            for (const auto& id : arg->get_children()) {
                                if (id->get_type() == cst_type::identifier && id->content.size() >= 4 && id->content.compare(0, 4, VARIADIC_PARAM_PREFIX) == 0) {
                                    is_synthetic = true;
                                    break;
                                }
                            }
                            if (is_synthetic) {
                                is_variadic = true;
                                break;
                            }
                            real_param_count++;
                        }
                        if (is_variadic) {
                            function_param_counts[func_name] = -1;
                            function_min_param_counts[func_name] = real_param_count;
                        }
                        else {
                            function_param_counts[func_name] = real_param_count;
                        }
                        continue;
                    }
                    if (child->get_type() == cst_type::block) {
                        continue;
                    }
                    if (is_type_node(child->get_type()) || child->get_type() == cst_type::structure) {
                        return_type = type_name_of(child.get());
                    }
                }

                if (!func_name.empty() && !return_type.empty()) {
                    function_return_types[func_name] = return_type;
                }

                if (debug && !func_name.empty()) {
                    std::cout << CYAN << "[LINTER] Registered function '" << func_name << "'";
                    if (!return_type.empty()) {
                        std::cout << " -> " << return_type;
                    }
                    std::cout << RESET << "\n";
                }
            }
        }
    }

    void linter::lint_expr(cst* node) {
        if (!node)
            return;

        switch (node->get_type()) {
        case cst_type::identifier:
            {
                if (!is_declared(node->content) && !known_functions.count(node->content)) {
                    errors.emplace_back("Use of undeclared variable '" + node->content + "'");
                }
                break;
            }

        case cst_type::functioncall:
            {
                if (!node->get_children().empty()) {
                    const auto& first = node->get_children().front();
                    if (first->get_type() == cst_type::identifier) {
                        const bool is_method_style = !first->content.empty() && first->content.front() == '.';
                        if (!is_method_style && !known_functions.count(first->content)) {
                            errors.emplace_back("Call to undeclared function '" + first->content + "'");
                        }
                        else if (!is_method_style) {
                            // count actual arguments (functionargument nodes)
                            int actual_args = 0;
                            for (std::size_t i = 1; i < node->get_children().size(); ++i) {
                                if (node->get_children().at(i)->get_type() == cst_type::functionargument) {
                                    actual_args++;
                                }
                            }
                            auto pc_it = function_param_counts.find(first->content);
                            if (pc_it != function_param_counts.end()) {
                                int expected = pc_it->second;
                                if (expected == -1) {
                                    // variadic: check minimum required args
                                    auto min_it = function_min_param_counts.find(first->content);
                                    if (min_it != function_min_param_counts.end() && actual_args < min_it->second) {
                                        errors.emplace_back("Function '" + first->content + "' expects at least " + std::to_string(min_it->second) + " arguments, but " + std::to_string(actual_args) + " were provided");
                                    }
                                }
                                else if (actual_args != expected) {
                                    errors.emplace_back("Function '" + first->content + "' expects " + std::to_string(expected) + " arguments, but " + std::to_string(actual_args) + " were provided");
                                }
                            }
                        }
                    }
                    for (std::size_t i = 1; i < node->get_children().size(); ++i) {
                        lint_expr(node->get_children().at(i).get());
                    }
                }
                break;
            }

        case cst_type::memberaccess:
            {
                if (!node->get_children().empty()) {
                    const auto& base = node->get_children().front();
                    if (base->get_type() == cst_type::identifier) {
                        if (!is_declared(base->content)) {
                            errors.emplace_back("Use of undeclared variable '" + base->content + "'");
                        }
                    }
                }
                break;
            }

        case cst_type::arrayaccess:
            {
                if (!node->get_children().empty()) {
                    const auto& first = node->get_children().front();
                    if (first->get_type() == cst_type::identifier) {
                        if (!is_declared(first->content)) {
                            errors.emplace_back("Use of undeclared array '" + first->content + "'");
                        }
                    }
                    else {
                        lint_expr(first.get());
                    }
                }
                // check for negative array indices
                for (std::size_t i = 1; i < node->get_children().size(); ++i) {
                    const auto& child = node->get_children().at(i);
                    if (child->get_type() == cst_type::unary_minus_operator) {
                        if (i + 1 < node->get_children().size() && node->get_children().at(i + 1)->get_type() == cst_type::number_literal) {
                            errors.emplace_back("Negative array index is not allowed", lint_error::severity::warning);
                        }
                    }
                    lint_expr(child.get());
                }
                break;
            }

        default:
            {
                for (const auto& child : node->get_children()) {
                    lint_expr(child.get());
                }
                break;
            }
        }
    }

    static std::vector<cst*> to_raw(const std::vector<std::unique_ptr<cst>>& v) {
        std::vector<cst*> r;
        r.reserve(v.size());
        for (const auto& p : v)
            r.push_back(p.get());
        return r;
    }

    void linter::check_assignment_type(const std::string& decl_type, const std::string& rhs_type) {
        if (!types_compatible(decl_type, rhs_type)) {
            errors.emplace_back("Type mismatch: cannot assign '" + rhs_type + "' to variable of type '" + decl_type + "'");
        }
    }

    void linter::lint_block(cst_block* block_node) {
        push_scope();

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
            case cst_type::float32_datatype:
            case cst_type::float64_datatype:
            case cst_type::string_datatype:
            case cst_type::bool_datatype:
                {
                    const std::string tname = type_name_of(c.get());
                    if (!c->get_children().empty()) {
                        const auto& id_node = c->get_children().front();
                        if (id_node->get_type() == cst_type::identifier) {
                            declare_var(id_node->content, tname);
                            if (c->is_const) {
                                const_variables.insert(id_node->content);
                            }
                            if (c->is_const && c->get_children().size() < 2) {
                                errors.emplace_back("const variable '" + id_node->content + "' must be initialized at declaration");
                            }
                        }
                        for (std::size_t i = 1; i < c->get_children().size(); ++i) {
                            lint_expr(c->get_children().at(i).get());
                        }
                        if (c->get_children().size() >= 2) {
                            const auto& assign = c->get_children().at(1);
                            if (assign->get_type() == cst_type::assignment && !assign->get_children().empty()) {
                                check_assignment_type(tname, infer_type_rpn(to_raw(assign->get_children())));
                            }
                        }
                    }
                    break;
                }

            case cst_type::structure:
                {
                    const std::string tname = type_name_of(c.get());
                    if (!c->get_children().empty()) {
                        const auto& id_node = c->get_children().front();
                        if (id_node->get_type() == cst_type::identifier) {
                            declare_var(id_node->content, tname);
                            if (c->is_const) {
                                const_variables.insert(id_node->content);
                            }
                        }
                        for (std::size_t i = 1; i < c->get_children().size(); ++i) {
                            lint_expr(c->get_children().at(i).get());
                        }
                        if (c->get_children().size() >= 2) {
                            const auto& assign = c->get_children().at(1);
                            if (assign->get_type() == cst_type::assignment && !assign->get_children().empty()) {
                                check_assignment_type(tname, infer_type_rpn(to_raw(assign->get_children())));
                            }
                        }
                    }
                    break;
                }

            case cst_type::enumeration:
                {
                    break;
                }

            case cst_type::moduledecl:
            case cst_type::importdecl:
                {
                    break;
                }

            case cst_type::array:
                {
                    if (!c->get_children().empty()) {
                        auto& type_node = c->get_children().front();
                        if (!type_node->get_children().empty()) {
                            const auto& id_node = type_node->get_children().front();
                            if (id_node->get_type() == cst_type::identifier) {
                                declare_var(id_node->content, type_name_of(type_node.get()) + "[]");
                            }
                        }
                        for (std::size_t i = 1; i < c->get_children().size(); ++i) {
                            lint_expr(c->get_children().at(i).get());
                        }
                    }
                    break;
                }

            case cst_type::identifier:
                {
                    if (!is_declared(c->content)) {
                        errors.emplace_back("Assignment to undeclared variable '" + c->content + "'");
                    }
                    else if (const_variables.count(c->content)) {
                        errors.emplace_back("Cannot reassign to const variable '" + c->content + "'");
                    }
                    for (const auto& child : c->get_children()) {
                        lint_expr(child.get());
                    }
                    break;
                }

            case cst_type::functioncall:
                lint_expr(c.get());
                break;

            case cst_type::returnstmt:
                lint_expr(c.get());
                break;

            case cst_type::ifstmt:
                {
                    std::vector<cst*> cond;
                    for (const auto& child : c->get_children()) {
                        if (child->get_type() != cst_type::block && child->get_type() != cst_type::elsestmt && child->get_type() != cst_type::elseifstmt) {
                            cond.push_back(child.get());
                        }
                    }
                    infer_type_rpn(cond);

                    // check for bare condition without comparison operator
                    {
                        bool has_comparison = false;
                        for (auto* n : cond) {
                            auto t = n->get_type();
                            if (t == cst_type::equals_operator || t == cst_type::not_equals_operator || t == cst_type::greater_than_operator || t == cst_type::less_than_operator || t == cst_type::greater_than_or_equal_operator ||
                                t == cst_type::less_than_or_equal_operator) {
                                has_comparison = true;
                                break;
                            }
                        }
                        if (!has_comparison && !cond.empty()) {
                            errors.emplace_back("Conditions require an explicit binary comparison (==, !=, <, >, <=, >=)", lint_error::severity::warning);
                        }
                    }

                    for (const auto& child : c->get_children()) {
                        switch (child->get_type()) {
                        case cst_type::block:
                            lint_block(cst::cast_raw<cst_block>(child.get()));
                            break;
                        case cst_type::elsestmt:
                            for (const auto& eb : child->get_children()) {
                                if (eb->get_type() == cst_type::block) {
                                    lint_block(cst::cast_raw<cst_block>(eb.get()));
                                }
                                else {
                                    lint_expr(eb.get());
                                }
                            }
                            break;
                        case cst_type::elseifstmt:
                            {
                                std::vector<cst*> econd;
                                for (const auto& eib : child->get_children()) {
                                    if (eib->get_type() != cst_type::block) {
                                        econd.push_back(eib.get());
                                    }
                                }
                                infer_type_rpn(econd);

                                // check for bare condition in else-if
                                {
                                    bool has_cmp = false;
                                    for (auto* n : econd) {
                                        auto t = n->get_type();
                                        if (t == cst_type::equals_operator || t == cst_type::not_equals_operator || t == cst_type::greater_than_operator || t == cst_type::less_than_operator ||
                                            t == cst_type::greater_than_or_equal_operator || t == cst_type::less_than_or_equal_operator) {
                                            has_cmp = true;
                                            break;
                                        }
                                    }
                                    if (!has_cmp && !econd.empty()) {
                                        errors.emplace_back("Conditions require an explicit binary comparison (==, !=, <, >, <=, >=)", lint_error::severity::warning);
                                    }
                                }

                                for (const auto& eib : child->get_children()) {
                                    if (eib->get_type() == cst_type::block) {
                                        lint_block(cst::cast_raw<cst_block>(eib.get()));
                                    }
                                    else {
                                        lint_expr(eib.get());
                                    }
                                }
                                break;
                            }
                        default:
                            lint_expr(child.get());
                            break;
                        }
                    }
                    break;
                }

            case cst_type::whilestmt:
                {
                    std::vector<cst*> cond;
                    for (const auto& child : c->get_children()) {
                        if (child->get_type() != cst_type::block) {
                            cond.push_back(child.get());
                        }
                    }
                    infer_type_rpn(cond);

                    // check for bare condition without comparison operator
                    {
                        bool has_comparison = false;
                        for (auto* n : cond) {
                            auto t = n->get_type();
                            if (t == cst_type::equals_operator || t == cst_type::not_equals_operator || t == cst_type::greater_than_operator || t == cst_type::less_than_operator || t == cst_type::greater_than_or_equal_operator ||
                                t == cst_type::less_than_or_equal_operator) {
                                has_comparison = true;
                                break;
                            }
                        }
                        if (!has_comparison && !cond.empty()) {
                            errors.emplace_back("Conditions require an explicit binary comparison (==, !=, <, >, <=, >=)", lint_error::severity::warning);
                        }
                    }

                    for (const auto& child : c->get_children()) {
                        if (child->get_type() == cst_type::block) {
                            lint_block(cst::cast_raw<cst_block>(child.get()));
                        }
                        else {
                            lint_expr(child.get());
                        }
                    }
                    break;
                }

            case cst_type::loopstmt:
                {
                    for (const auto& child : c->get_children()) {
                        if (child->get_type() == cst_type::block) {
                            lint_block(cst::cast_raw<cst_block>(child.get()));
                        }
                    }
                    break;
                }

            case cst_type::forstmt:
                {
                    push_scope();
                    if (!c->get_children().empty()) {
                        auto& init_node = c->get_children().front();
                        if (is_type_node(init_node->get_type())) {
                            // check for float for-loop counter
                            if (init_node->get_type() == cst_type::float32_datatype || init_node->get_type() == cst_type::float64_datatype) {
                                errors.emplace_back("Float types are not allowed as for-loop counter variables", lint_error::severity::error);
                            }

                            if (!init_node->get_children().empty()) {
                                const auto& id_node = init_node->get_children().front();
                                if (id_node->get_type() == cst_type::identifier) {
                                    const std::string itype = type_name_of(init_node.get());
                                    declare_var(id_node->content, itype);
                                    if (init_node->get_children().size() >= 2) {
                                        const auto& init_assign = init_node->get_children().at(1);
                                        if (init_assign->get_type() == cst_type::assignment && !init_assign->get_children().empty()) {
                                            check_assignment_type(itype, infer_type_rpn(to_raw(init_assign->get_children())));
                                        }
                                    }
                                }
                                for (std::size_t i = 1; i < init_node->get_children().size(); ++i) {
                                    lint_expr(init_node->get_children().at(i).get());
                                }
                            }
                        }
                    }
                    for (std::size_t i = 1; i < c->get_children().size(); ++i) {
                        auto& child = c->get_children().at(i);
                        if (child->get_type() == cst_type::block) {
                            lint_block(cst::cast_raw<cst_block>(child.get()));
                        }
                        else if (child->get_type() == cst_type::forcondition) {
                            infer_type_rpn(to_raw(child->get_children()));

                            // check for bare condition in for-loop
                            {
                                bool has_comparison = false;
                                for (const auto& gc : child->get_children()) {
                                    auto t = gc->get_type();
                                    if (t == cst_type::equals_operator || t == cst_type::not_equals_operator || t == cst_type::greater_than_operator || t == cst_type::less_than_operator || t == cst_type::greater_than_or_equal_operator ||
                                        t == cst_type::less_than_or_equal_operator) {
                                        has_comparison = true;
                                        break;
                                    }
                                }
                                if (!has_comparison && !child->get_children().empty()) {
                                    errors.emplace_back("Conditions require an explicit binary comparison (==, !=, <, >, <=, >=)", lint_error::severity::warning);
                                }
                            }

                            for (const auto& gc : child->get_children()) {
                                lint_expr(gc.get());
                            }
                        }
                        else {
                            for (const auto& gc : child->get_children()) {
                                lint_expr(gc.get());
                            }
                        }
                    }
                    pop_scope();
                    break;
                }

            case cst_type::memberaccess:
            case cst_type::arrayaccess:
            case cst_type::dereference:
                lint_expr(c.get());
                break;

            case cst_type::breakstmt:
            case cst_type::continuestmt:
                break;

            case cst_type::switchstmt:
                {
                    for (const auto& child : c->get_children()) {
                        if (child->get_type() == cst_type::casestmt) {
                            for (const auto& cc : child->get_children()) {
                                if (cc->get_type() == cst_type::block) {
                                    lint_block(cst::cast_raw<cst_block>(cc.get()));
                                }
                                else {
                                    lint_expr(cc.get());
                                }
                            }
                        }
                        else if (child->get_type() == cst_type::defaultstmt) {
                            for (const auto& dc : child->get_children()) {
                                if (dc->get_type() == cst_type::block) {
                                    lint_block(cst::cast_raw<cst_block>(dc.get()));
                                }
                            }
                        }
                        else {
                            lint_expr(child.get());
                        }
                    }
                    break;
                }

            default:
                break;
            }
        }

        pop_scope();
    }

    void linter::lint_function(cst_function* func_node) {
        push_scope();

        for (const auto& c : func_node->get_children()) {
            switch (c->get_type()) {
            case cst_type::functionarguments:
                {
                    for (const auto& arg : c->get_children()) {
                        if (arg->get_type() == cst_type::variadic) {
                            declare_var("__varargs", "int64");
                            declare_var("__varargs_size", "int64");
                            continue;
                        }
                        const std::string atype = type_name_of(arg.get());
                        for (const auto& id : arg->get_children()) {
                            if (id->get_type() == cst_type::identifier) {
                                declare_var(id->content, atype);
                                break;
                            }
                        }
                    }
                    break;
                }
            case cst_type::block:
                lint_block(cst::cast_raw<cst_block>(c.get()));
                break;
            default:
                break;
            }
        }

        pop_scope();
    }

    bool linter::analyze() {
        collect_functions();

        push_scope();

        for (const auto& c : root->get_children()) {
            if (c->get_type() == cst_type::function) {
                lint_function(cst::cast_raw<cst_function>(c.get()));
            }
            else if (is_type_node(c->get_type())) {
                const std::string tname = type_name_of(c.get());
                if (!c->get_children().empty()) {
                    const auto& id_node = c->get_children().front();
                    if (id_node->get_type() == cst_type::identifier) {
                        declare_var(id_node->content, tname);
                        if (c->is_const) {
                            const_variables.insert(id_node->content);
                            if (c->get_children().size() < 2) {
                                errors.emplace_back("const variable '" + id_node->content + "' must be initialized at declaration");
                            }
                        }
                    }
                }
            }
        }

        pop_scope();

        for (const auto& e : errors) {
            if (e.level == lint_error::severity::error) {
                return false;
            }
        }
        return true;
    }
} // namespace occult
