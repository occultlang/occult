#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include "error.hpp"

inline bool verbose_parser;

namespace occult {
  class parser {
    std::unique_ptr<ast_root> root;
    std::vector<token_t> stream;
    std::uintptr_t pos = 0;
    
    token_t peek(std::uintptr_t pos = 0);
    token_t previous();
    void consume();
    bool match(token_t t, token_type tt);
    bool match_and_consume(token_t, token_type tt);
    std::vector<token_t> to_rpn(std::vector<token_t> expr);
    std::vector<std::unique_ptr<ast>> parse_expression(std::vector<token_t> expr);
    std::unique_ptr<ast_function> parse_function();
    std::unique_ptr<ast_block> parse_block();
    std::unique_ptr<ast_assignment> parse_assignment();
    std::unique_ptr<ast> parse_datatype();
    std::unique_ptr<ast_identifier> parse_identifier();
    template<typename IntegerAstType = ast>
    std::unique_ptr<IntegerAstType> parse_integer_type();
    std::unique_ptr<ast_string> parse_string();
    std::unique_ptr<ast> parse_keyword(bool nested_function = false);
    std::unique_ptr<ast_ifstmt> parse_if();
    std::unique_ptr<ast_elseifstmt> parse_elseif();
    std::unique_ptr<ast_elsestmt> parse_else();
    std::unique_ptr<ast_loopstmt> parse_loop();
    std::unique_ptr<ast_whilestmt> parse_while();
    std::unique_ptr<ast_forstmt> parse_for(); // reserved for later on
    std::unique_ptr<ast_continuestmt> parse_continue();
    std::unique_ptr<ast_breakstmt> parse_break();
    std::unique_ptr<ast_returnstmt> parse_return();
  public:
    parser(std::vector<token_t> stream) : root(ast::new_node<ast_root>()), stream(stream) {}
    
    std::unique_ptr<ast_root> parse();
  };
} // namespace occult
