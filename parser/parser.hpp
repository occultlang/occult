#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include <vector>
#include <variant>
#include <expected>
#include <functional>
#include <optional>
#include <tuple>
#include <stack>
#include "error.hpp"

namespace occultlang
{
	class parser
	{
		lexer _lexer;
		std::vector<token> tokens;
		int position = 0;

		enum error_code
		{
			ERROR = 0,
			EXPECTED_RIGHT_PARENTHESIS,
			EXPECTED_LEFT_PARENTHESIS,
			EXPECTED_RIGHT_BRACE,
			EXPECTED_LEFT_BRACE,
			EXPECTED_SEMICOLON,
			EXPECTED_COLON,
			EXPECTED_COMMA,
			EXPECTED_RIGHT_ARROW,
			EXPECTED_EQUAL,
			EXPECTED_TYPE,
			EXPECTED_FUNCTION,
			EXPECTED_IDENTIFIER,
			EXPECTED_NUMBER_LITERAL,
			EXPECTED_BOOLEAN_LITERAL,
			EXPECTED_KEYWORD,
			EXPECTED_DELIMITER,
			EXPECTED_OPERATOR,
			EXPECTED_RETURN_TYPE,
			EXPECTED_NUMBER_DECLARATION,
			EXPECTED_BOOLEAN_DECLARATION,
			NOT_IMPLEMENTED,
			EXPECTED_BREAK,
			EXPECTED_STRING_LITERAL,
		};

		std::unordered_map<error_code, const char *> parse_exceptions{
			{ERROR, "Unknown error"},
			{EXPECTED_RIGHT_PARENTHESIS, "Expected right parenthesis"},
			{EXPECTED_LEFT_PARENTHESIS, "Expected left parenthesis"},
			{EXPECTED_RIGHT_BRACE, "Expected right brace"},
			{EXPECTED_LEFT_BRACE, "Expected left brace"},
			{EXPECTED_SEMICOLON, "Expected semicolon"},
			{EXPECTED_COLON, "Expected colon"},
			{EXPECTED_COMMA, "Expected comma"},
			{EXPECTED_RIGHT_ARROW, "Expected right arrow"},
			{EXPECTED_EQUAL, "Expected equal"},
			{EXPECTED_TYPE, "Expected type"},
			{EXPECTED_FUNCTION, "Expected function"},
			{EXPECTED_IDENTIFIER, "Expected identifier"},
			{EXPECTED_NUMBER_LITERAL, "Expected number literal"},
			{EXPECTED_BOOLEAN_LITERAL, "Expected boolean literal"},
			{EXPECTED_KEYWORD, "Expected keyword"},
			{EXPECTED_DELIMITER, "Expected delimiter"},
			{EXPECTED_OPERATOR, "Expected operator"},
			{EXPECTED_RETURN_TYPE, "Expected return type"},
			{EXPECTED_NUMBER_DECLARATION, "Expected number declaration"},
			{EXPECTED_BOOLEAN_DECLARATION, "Expected boolean declaration"},
			{NOT_IMPLEMENTED, "Not implemented"},
			{EXPECTED_BREAK, "Expected break"},
			{EXPECTED_STRING_LITERAL, "Expected string literal"}};

	public:
		parser(const std::string &source) : _lexer(source) { tokens = _lexer.lex(); /* occultlang::lexer::visualize(get_tokens());*/ }

		token consume(token_type tt);
		token consume(token_type tt, token_type tt2);
		token peek();
		token peek(int amount);
		token peek_next();
		token consume();

		bool match(token_type tt, std::string match);
		bool match(token_type tt);
		bool match_next(token_type tt, std::string match);
		bool match_next(token_type tt);

		std::vector<token> &get_tokens() { return tokens; }

		std::shared_ptr<ast> parse_number_literal();
		std::shared_ptr<ast> parse_identifier();
		std::shared_ptr<ast> parse_function();
		std::shared_ptr<ast> parse_keywords();
		std::shared_ptr<ast> _parse_delaration(std::optional<std::shared_ptr<ast>> variable_declaration);
		std::shared_ptr<ast> parse_declaration();
		std::shared_ptr<ast> parse_expression();
		std::shared_ptr<ast> parse_if();
		std::shared_ptr<ast> parse_elseif();
		std::shared_ptr<ast> parse_else();
		std::shared_ptr<ast> parse_loop();
		std::shared_ptr<ast> parse_return();
		std::shared_ptr<ast> parse_break();
		std::shared_ptr<ast> parse_string_literal();
		std::shared_ptr<ast> parse_boolean_literal();
		std::shared_ptr<ast> parse_assignment();
		std::vector<std::shared_ptr<ast>> parse_term();
		std::shared_ptr<ast> parse_float_literal();
		std::shared_ptr<ast> parse_post_or_prefix();
		std::shared_ptr<ast> parse_while();
		std::shared_ptr<ast> parse_for();
		std::shared_ptr<ast> parse_continue();
		std::shared_ptr<ast> parse();
	};
} // occultlang
