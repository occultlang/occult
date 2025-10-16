#pragma once
#include "cst.hpp"
#include "../lexer/lexer.hpp"
#include "error.hpp"

#include <variant>
#include <source_location>

namespace occult {
  class parser {
  public:
    enum class state : std::uint8_t { 
      neutral,
      failed,
      success
    };
  private:
    std::unique_ptr<cst_root> root;
    std::vector<token_t> stream;
    std::uintptr_t pos = 0;
    state parser_state = state::neutral;
    std::unordered_map<std::string, cst*> custom_type_map; // used for structures

    token_t peek(std::uintptr_t pos = 0);
    token_t previous();
    void consume(std::uintptr_t amt = 1);
    bool match(token_t t, token_type tt);
    void parse_function_call_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, std::vector<token_t>& expr_ref, token_t& curr_tok_ref, std::size_t& i_ref);
    void parse_array_access_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, std::vector<token_t>& expr_ref, token_t& curr_tok_ref, std::size_t& i_ref);
    void parse_struct_member_access_expr(std::vector<std::unique_ptr<cst>>& expr_cst_ref, std::vector<token_t>& expr_ref, token_t& curr_tok_ref, std::size_t& i_ref);
    void shunting_yard(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref, token_t& curr_tok_ref);
    void shunting_yard_stack_cleanup(std::stack<token_t>& stack_ref, std::vector<std::unique_ptr<cst>>& expr_cst_ref);
    std::vector<std::unique_ptr<cst>> parse_expression(std::vector<token_t> expr);
    std::unique_ptr<cst_function> parse_function();
    std::unique_ptr<cst_block> parse_block();
    std::unique_ptr<cst_assignment> parse_assignment();
    std::unique_ptr<cst> parse_datatype();
    std::unique_ptr<cst_identifier> parse_identifier();
    template<typename ParentNode>
    void parse_expression_until(ParentNode* parent, token_type t);
    template<typename IntegercstType = cst>
    std::unique_ptr<IntegercstType> parse_integer_type();
    std::unique_ptr<cst_string> parse_string();
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
    void synchronize(std::string what);
  public:
    parser(std::vector<token_t> stream) : root(cst::new_node<cst_root>()), stream(stream) {}
    
    std::unique_ptr<cst_root> parse();
    std::unordered_map<std::string, cst*> get_custom_type_map() const;
    state get_state() const { return parser_state; }
  };
} // namespace occult
