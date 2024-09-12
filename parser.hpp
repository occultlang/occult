#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include "error.hpp"

namespace occult {
  class parser {
    public:
    std::unique_ptr<ast_root> root;
    std::vector<token_t> stream;
    std::uintptr_t pos = 0;
    
    token_t peek();
    token_t previous();
    void consume();
    bool match(token_t t, token_type tt);
    std::unique_ptr<ast_function> parse_function();
    std::unique_ptr<ast_block> parse_block();
    std::unique_ptr<ast_datatype> parse_datatype();
    std::unique_ptr<ast_binaryexpr> parse_binaryexpr();
    std::unique_ptr<ast_literalexpr> parse_literal();
    std::unique_ptr<ast_identifier> parse_identifier();
    std::unique_ptr<ast> parse_keyword();
    std::unique_ptr<ast_ifstmt> parse_if();
    std::unique_ptr<ast_elseifstmt> parse_elseif();
    std::unique_ptr<ast_elsestmt> parse_else();
    std::unique_ptr<ast_loopstmt> parse_loop();
    std::unique_ptr<ast_whilestmt> parse_while();
    std::unique_ptr<ast_forstmt> parse_for();
    std::unique_ptr<ast_matchstmt> parse_match();
    std::unique_ptr<ast_caseblock> parse_case();
    std::unique_ptr<ast_defaultcase> parse_defaultcase();
    std::unique_ptr<ast_continuestmt> parse_continue();
    std::unique_ptr<ast_breakstmt> parse_break();
    std::unique_ptr<ast_returnstmt> parse_return();
    std::unique_ptr<ast_instmt> parse_in();
  //public:
    parser(std::vector<token_t> stream) : root(ast::new_node<ast_root>()), stream(stream) {}
    
    std::unique_ptr<ast_root> parse();
  };
} // namespace occult
