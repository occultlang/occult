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
        std::vector<lint_error> errors;

        void push_scope();
        void pop_scope();

        // declare a variable in the innermost scope. emits an error and returns
        // false if the name is already declared in the same scope
        bool declare_var(const std::string& name, const std::string& type);

        // Returns true if name is visible in any active scope frame
        bool is_declared(const std::string& name) const;

        // returns the declared type of name, or "" if not found
        std::string lookup_type(const std::string& name) const;

        void collect_functions();
        void lint_expr(cst* node);
        void lint_block(cst_block* block_node);
        void lint_function(cst_function* func_node);

        // evaluates a flat RPN expression on a type stack and returns the result
        // type. Emits errors for type mismatches in comparison operators
        // tags: "<number>", "<float>", "<str>", "" (unknown)
        std::string infer_type_rpn(const std::vector<cst*>& nodes);

        bool types_compatible(const std::string& a, const std::string& b) const;

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
