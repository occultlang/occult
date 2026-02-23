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

        return false;
    }

    std::string linter::infer_type_rpn(const std::vector<cst*>& nodes) {
        std::vector<std::string> stk;

        for (std::size_t i = 0; i < nodes.size(); ++i) {
            cst* node = nodes[i];
            if (!node)
                continue;

            switch (node->get_type()) {
            case cst_type::number_literal:
                stk.push_back("<number>");
                break;
            case cst_type::float_literal:
                stk.push_back("<float>");
                break;
            case cst_type::stringliteral:
                stk.push_back("<str>");
                break;
            case cst_type::charliteral:
                stk.push_back("<number>");
                break;

            case cst_type::identifier:
                stk.push_back(lookup_type(node->content));
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
                        for (int j = 0; j < count && !t.empty() && t.back() == '*'; ++j) {
                            t.pop_back();
                        }
                        stk.push_back(t);
                    }
                    else {
                        stk.push_back("");
                    }
                    break;
                }

            case cst_type::cast_to_datatype:
                stk.push_back(keyword_to_type_name(node->content));
                break;

            case cst_type::functioncall:
            case cst_type::memberaccess:
                stk.push_back("");
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
                        std::string lhs = stk.back();
                        stk.pop_back();
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
                        std::string lhs = stk.back();
                        stk.pop_back();
                        if (!types_compatible(lhs, rhs)) {
                            errors.emplace_back("Type mismatch in comparison: cannot compare '" + lhs + "' with '" + rhs + "'");
                        }
                    }
                    stk.push_back("bool");
                    break;
                }

            case cst_type::and_operator:
            case cst_type::or_operator:
                if (stk.size() >= 2) {
                    stk.pop_back();
                    stk.pop_back();
                }
                stk.push_back("bool");
                break;

            case cst_type::unary_minus_operator:
            case cst_type::unary_plus_operator:
            case cst_type::unary_bitwise_not:
                break;
            case cst_type::unary_not_operator:
                if (!stk.empty())
                    stk.pop_back();
                stk.push_back("bool");
                break;

            case cst_type::expr_end:
            case cst_type::expr_start:
                break;

            default:
                stk.push_back("");
                break;
            }
        }
        return stk.empty() ? "" : stk.back();
    }

    void linter::collect_functions() {
        for (const auto& c : root->get_children()) {
            if (c->get_type() == cst_type::function) {
                for (const auto& child : c->get_children()) {
                    if (child->get_type() == cst_type::identifier) {
                        known_functions.insert(child->content);
                        if (debug) {
                            std::cout << CYAN << "[LINTER] Registered function '" << child->content << "'" << RESET << "\n";
                        }
                        break;
                    }
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
                        if (!known_functions.count(first->content)) {
                            errors.emplace_back("Call to undeclared function '" + first->content + "'");
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
                for (std::size_t i = 1; i < node->get_children().size(); ++i) {
                    lint_expr(node->get_children().at(i).get());
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

        for (const auto& c : root->get_children()) {
            if (c->get_type() == cst_type::function) {
                lint_function(cst::cast_raw<cst_function>(c.get()));
            }
        }

        for (const auto& e : errors) {
            if (e.level == lint_error::severity::error) {
                return false;
            }
        }
        return true;
    }
} // namespace occult
