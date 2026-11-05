#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../parser/cst.hpp"

/*
    todo:
    unused variables
    tracking return type + missing returns
    unreachable code
    dead assignments
    array bounds
    maybe some pointer stuff as well
*/

namespace occult {
    struct lint_error {
        enum class severity : std::uint8_t { warning, error } level;
        std::string message;
        lint_error(std::string msg, severity s = severity::error) : level(s), message(std::move(msg)) {}
    };

    class linter {
        cst_root* root;
        bool debug;

        std::vector<std::unordered_map<std::string, std::string>> scope_stack;
        std::unordered_set<std::string> known_functions;
        std::unordered_map<std::string, std::string> fnptr_vars; // variable -> fn-pointer signature
        std::unordered_map<std::string, std::string> function_return_types;
        std::unordered_map<std::string, int> function_param_counts;     // -1 = variadic minimum count stored separately
        std::unordered_map<std::string, std::vector<std::string>> function_param_types;
        std::string current_return_type;
        std::unordered_map<std::string, int> function_min_param_counts; // minimum required args for variadic functions
        std::vector<lint_error> errors;
        std::unordered_set<std::string> known_enum_types;
        std::unordered_set<std::string> const_variables;

        void push_scope();
        void pop_scope();

        bool declare_var(const std::string& name, const std::string& type);

        // Returns true if name is visible in any active scope frame
        bool is_declared(const std::string& name) const;

        // returns the declared type of name, or "" if not found
        std::string lookup_type(const std::string& name) const;

        void collect_functions();
        void lint_expr(cst* node);
        void lint_block(cst_block* block_node);
        void lint_function(cst_function* func_node);

        std::string infer_type_rpn(const std::vector<cst*>& nodes);

        bool types_compatible(const std::string& a, const std::string& b) const;
        bool assignable(const std::string& to, const std::string& from) const;

        static std::string type_name_of(cst* node);
        static std::string keyword_to_type_name(const std::string& kw);
        static bool is_type_node(cst_type t);

        void check_assignment_type(const std::string& decl_type, const std::string& rhs_type);

    public:
        explicit linter(cst_root* root, bool debug = false) : root(root), debug(debug) {}

        bool analyze();

        const std::vector<lint_error>& get_errors() const { return errors; }
    };

} // namespace occult
