#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include <vector>
#include <variant>
#include <expected>
#include <functional>
#include <optional>
#include <stack>

namespace occultlang {
	class parser {
		lexer _lexer;
		std::vector<token> tokens;
		int position;

		enum error_code {
			NO_DECLARED_TYPE,
			NOT_AN_IDENTIFIER,
			EXPECTED_PARENTACES,
			EXPECTED_COMMA,
			NONREAL_TYPE,
			EXPECTED_ARROW,
			EXPECTED_BRACKET,
			NOT_IN_SCOPE,
		};

		std::unordered_map<error_code, const char*> parse_exceptions {
			{ NO_DECLARED_TYPE, "No declared type" },
			{ NOT_AN_IDENTIFIER, "Token is not an identifier" },
			{ EXPECTED_PARENTACES, "Expected parentaces" },
			{ EXPECTED_COMMA, "Expected comma" },
			{ NONREAL_TYPE, "Nonreal type" },
			{ EXPECTED_ARROW, "Expected arrow" },
			{ EXPECTED_BRACKET, "Expected bracket" },
			{ NOT_IN_SCOPE, "Variable not in scope" },
		};

		std::unordered_set<std::string> data_type_set = {
			"i8", "i16", "i32", "i64",
			"u8", "u16", "u32", "u64",
			"f32", "f64", "bool", "string", "void"
		};
	public:
		parser(const std::string& source) : _lexer(source) { tokens = _lexer.lex(); }

		token consume(token_type tt) {
			if (position < tokens.size() && tokens[position].get_type() == tt) {
				return tokens[position++];
			}
		}

		token consume(token_type tt, token_type tt2) {
			if (position < tokens.size() && tokens[position].get_type() == tt || tokens[position].get_type() == tt2) {
				return tokens[position++];
			}
		}

		token peek() {
			return tokens[position];
		}

		token peek_next() {
			return tokens[position + 1];
		}

		token consume() {
			if (position < tokens.size()) {
				return tokens[position++];
			}
		}

		bool match(token_type tt, std::string match) { 
			return (tokens[position].get_type() == tt && tokens[position].get_lexeme() == match) ? true : false;
		}

		bool match(token_type tt) {
			return (tokens[position].get_type() == tt) ? true : false;
		}

		bool match_next(token_type tt, std::string match) {
			return (tokens[position + 1].get_type() == tt && tokens[position + 1].get_lexeme() == match) ? true : false;
		}

		bool match_next(token_type tt) {
			return (tokens[position + 1].get_type() == tt) ? true : false;
		}

		std::vector<token>& get_tokens() { return tokens; }

		void parse_comma();
		std::string parse_identifier();
		std::vector<std::shared_ptr<ast>> parse_function_arguments();
		std::shared_ptr<ast> parse_function();
		std::vector<std::shared_ptr<ast>> parse_term();
		std::shared_ptr<ast> parse_expression(std::optional<std::string> data_type);
		std::shared_ptr<ast> parse_keywords(bool nested = false); // flag is here to make sure there isn't nested functions
		std::shared_ptr<ast> parse();
	};
} // occultlang