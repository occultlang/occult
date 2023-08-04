#include "parser.hpp"
#include "../lexer/lexer.hpp"

namespace occultlang {
	void parser::parse_comma() {
		if (match(tk_delimiter, ",")) {
			consume(tk_delimiter);
		}
		else {
			throw std::exception(parse_exceptions[EXPECTED_COMMA]);
		}
	}

	std::string parser::parse_identifier() {
		if (match(tk_identifier)) {
			return consume(tk_identifier).get_lexeme(); // consume, get, and return identifier
		}
		else {
			throw std::exception(parse_exceptions[NOT_AN_IDENTIFIER]);
		}
	}

	std::vector<variable> parser::parse_function_arguments() {
		std::vector<variable> variables;

		if (match(tk_delimiter, "(")) {
			consume(tk_delimiter);

			while (!match(tk_delimiter, ")")) {
				if (match(tk_keyword)) {
					variable var;

					auto type = consume(tk_keyword);

					if (data_type_set.count(type.get_lexeme())) {
						auto name = parse_identifier();

						parse_comma();

						var.type = type.get_lexeme();
						var.name = name;
						var.scope = FUNCTION_SCOPE;
					}
					else {
						throw std::exception(parse_exceptions[NONREAL_TYPE]);
					}
				}
			}

			return variables;
		}
		else {
			throw std::exception(parse_exceptions[EXPECTED_PARENTACES]);
		}
	}

	std::shared_ptr<ast> parser::parse_function(bool& nested, int& scope) {
		consume(tk_keyword); // consume "fn" keyword

		auto name = parse_identifier();

		auto function_decl = std::make_shared<ast_function_declaration>(name);

		auto arguments = parse_function_arguments();

		consume(tk_delimiter); // consume ")"

		function_decl->variables = arguments;

		if (match(tk_operator, "->")) {
			consume(tk_operator);

			if (match(tk_keyword)) {
				variable var;

				auto type = consume(tk_keyword);

				if (data_type_set.count(type.get_lexeme())) {
					function_decl->type = type.get_lexeme();
				}
				else {
					throw std::exception(parse_exceptions[NONREAL_TYPE]);
				}
			}
			else {
				throw std::exception(parse_exceptions[NO_DECLARED_TYPE]);
			}
		}
		else {
			throw std::exception(parse_exceptions[EXPECTED_ARROW]);
		}

		if (match(tk_delimiter, "{")) {
			consume(tk_delimiter);

			auto body = parse_keywords(true, ++scope);

			function_decl->add_child(body);
		}
		else {
			throw std::exception(parse_exceptions[EXPECTED_BRACKET]);
		}

		if (match(tk_delimiter, "}")) {
			consume(tk_delimiter);
		}
		else {
			throw std::exception(parse_exceptions[EXPECTED_BRACKET]);
		}

		return function_decl;
	}

	std::shared_ptr<ast> parser::parse_expression(int& scope) {
		if (match(tk_delimiter, "(")) {

		}
	}

	std::shared_ptr<ast> parser::parse_condition() {
		return {};
	}

	std::shared_ptr<ast> parser::parse_keywords(bool nested, int scope) {
		if (match(tk_keyword, "fn") && !nested) {
			return parse_function(nested, scope);
		}
		else if (match(tk_keyword, "i8")) {

		}
		else if (match(tk_keyword, "if")) {
			consume(tk_keyword);

			if (match(tk_keyword, "else")) {
				consume(tk_keyword);
			}
			else {

			}
		}
		else if (match(tk_keyword, "else")) {
			consume(tk_keyword);

			if (match(tk_delimiter, "{")) {

			}
			else {
				throw std::exception(parse_exceptions[EXPECTED_BRACKET]);
			}
		}
		else {
			return std::make_shared<ast_empty_body>();
		}
	}

	std::shared_ptr<ast> parser::parse() {

		std::shared_ptr<ast> root = std::make_shared<ast>();

		while (position < tokens.size() && tokens[position].get_type() != tk_eof) {
			auto statement = std::shared_ptr<ast>();

			try {
				statement = parse_keywords();
			}
			catch (std::exception& e) {
				std::cout << e.what() << std::endl;

				return 0;
			}

			if (statement) {
				root->add_child(statement);
			}
		}

		return root;
	}
} //occultlang