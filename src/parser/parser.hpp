#pragma once
#include <source_location>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "../lexer/lexer.hpp"
#include "cst.hpp"
#include "error.hpp"

namespace occult {
    struct generic_template {
        std::vector<std::string> type_params;
        std::vector<token_t> tokens;
        std::string name;
    };

    class parser {
    public:
        enum class state : std::uint8_t { neutral, failed, success };

    private:
        std::unique_ptr<cst_root> root;
        std::vector<token_t> stream;
        std::size_t pos = 0;
        state parser_state = state::neutral;
        std::unordered_map<std::string, cst*> custom_type_map; // used for structures
        std::string source_file_path;
        std::unordered_map<std::string, std::unique_ptr<cst_generic_type>> cst_generic_type_cache;

        std::unordered_map<std::string, generic_template> generic_struct_templates;
        std::unordered_map<std::string, generic_template> generic_func_templates;
        std::unordered_set<std::string> instantiated_generics;

        std::vector<std::string> source_lines;
        std::size_t error_count = 0;
        std::size_t recursion_depth = 0;
        static constexpr std::size_t max_recursion_depth = 256;

        token_t peek(std::uintptr_t _pos = 0);

        token_t previous();

        void consume(std::uintptr_t amt = 1);

        static bool match(const token_t& t, token_type tt);

        void parse_function_call_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, const std::vector<token_t>& expr_ref, const token_t& curr_tok_ref, std::size_t& i_ref);

        void parse_array_access_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, const std::vector<token_t>& expr_ref, const token_t& curr_tok_ref, std::size_t& i_ref);

        void parse_struct_member_access_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, const std::vector<token_t>& expr_ref, const token_t& curr_tok_ref, std::size_t& i_ref) const;

        void shunting_yard(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref, const token_t& curr_tok_ref) const;

        void shunting_yard_stack_cleanup(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref) const;

        bool is_castable(token_t t);

        std::vector<std::unique_ptr<cst>> parse_expression(const std::vector<token_t>& expr);

        std::unique_ptr<cst_function> parse_function();

        std::unique_ptr<cst_block> parse_block();

        std::unique_ptr<cst_assignment> parse_assignment();

        std::unique_ptr<cst> parse_datatype();

        std::unique_ptr<cst_identifier> parse_identifier();

        template <typename ParentNode>
        void parse_expression_until(ParentNode* parent, token_type t);

        template <typename IntegerCstType = cst>
        std::unique_ptr<IntegerCstType> parse_integer_type();

        std::unique_ptr<cst_string> parse_string();

        std::unique_ptr<cst> parse_compound_assignment_identifier(std::unique_ptr<cst_identifier> to_assign);

        std::unique_ptr<cst> parse_keyword(bool nested_function = false);

        std::unique_ptr<cst_struct> parse_custom_type();

        std::unique_ptr<cst_ifstmt> parse_if();

        std::unique_ptr<cst_elseifstmt> parse_elseif();

        std::unique_ptr<cst_elsestmt> parse_else();

        std::unique_ptr<cst_loopstmt> parse_loop();

        std::unique_ptr<cst_whilestmt> parse_while();

        std::unique_ptr<cst_forstmt> parse_regular_for(std::unique_ptr<cst_forstmt> existing_for_node);

        std::unique_ptr<cst_forstmt> parse_for();

        std::unique_ptr<cst_continuestmt> parse_continue();

        std::unique_ptr<cst_breakstmt> parse_break();

        std::unique_ptr<cst_returnstmt> parse_return();

        std::unique_ptr<cst_array> parse_array();

        std::unique_ptr<cst_struct> parse_struct();

        void instantiate_generic_struct(const std::string& template_name, const std::vector<token_t>& concrete_type_args);

        void instantiate_generic_function(const std::string& template_name, const std::vector<token_t>& concrete_type_args);

        std::vector<token_t> preprocess_generic_calls(const std::vector<token_t>& expr);

        // Returns true if the current position looks like a forward-referenced generic struct
        // type in a template context (e.g., child_entry<T>* where child_entry isn't defined yet).
        // If true, advances pos past the type reference.
        bool try_parse_forward_generic_struct_type(std::unique_ptr<cst>& out_node);

        void synchronize(const parsing_error& e);

    public:
        explicit parser(const std::vector<token_t>& stream, const std::string& source_file_path = "", const std::string& source = "");

        std::unique_ptr<cst_root> parse();

        std::unordered_map<std::string, cst*> get_custom_type_map() const;

        state get_state() const { return parser_state; }
        std::size_t get_error_count() const { return error_count; }
    };
} // namespace occult
