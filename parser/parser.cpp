#include "parser.hpp"
#include "../lexer/lexer.hpp"

namespace occultlang {
	void parser::parse_comma() {
		if (match(tk_delimiter, ",")) {
			consume(tk_delimiter);
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

	std::vector<std::shared_ptr<ast>> parser::parse_function_arguments() {
		std::vector<std::shared_ptr<ast>> variables;

		if (match(tk_delimiter, "(")) {
			consume(tk_delimiter);

			while (!match(tk_delimiter, ")")) {
				if (match(tk_keyword)) {

					auto type = consume(tk_keyword).get_lexeme();

					if (data_type_set.count(type)) {
						auto name = parse_identifier();

						parse_comma();

						auto var = std::make_shared<ast_variable_declaration>(name, type);

						variables.push_back(var);
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

	std::shared_ptr<ast> parser::parse_function() {
		consume(tk_keyword); // consume "fn" keyword

		auto name = parse_identifier();

		auto function_decl = std::make_shared<ast_function_declaration>();

		auto arguments = parse_function_arguments();

		consume(tk_delimiter); // consume ")"

		if (match(tk_keyword)) {
			auto type = consume(tk_keyword).get_lexeme();

			if (data_type_set.count(type)) {
				function_decl = std::make_shared<ast_function_declaration>(name, type);
			}
			else {
				throw std::exception(parse_exceptions[NONREAL_TYPE]);
			}
		}
		else {
			throw std::exception(parse_exceptions[NO_DECLARED_TYPE]);
		}

		function_decl->arguments = std::make_shared<ast_arguments>();
		function_decl->body = std::make_shared<ast_body>();

		function_decl->add_child(function_decl->arguments); // not in final ast
		function_decl->add_child(function_decl->body); // not in final ast

		for (auto& arg : arguments) {
			function_decl->arguments->add_child(arg);
		}

		if (match(tk_delimiter, "{")) {
			consume(tk_delimiter);

			while (!match(tk_delimiter, "}")) {
				auto body = parse_keywords(true);
			
				function_decl->body->add_child(body);
			}
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

	std::shared_ptr<ast> parser::parse_term(std::optional<std::string> data_type) {

	}

	std::shared_ptr<ast> parser::parse_expression(std::optional<std::string> data_type) {
		if (match(tk_delimiter, "(")) {

		}
		else if (match(tk_identifier) && match_next(tk_operator, "++") || match_next(tk_operator, "--")) { // postfix increment
			auto var = parse_identifier();

			auto inc = consume(tk_operator).get_lexeme();

			if (inc == "++") {
				return std::make_shared<ast_postfix_increment>(var);
			}
			else {
				return std::make_shared<ast_postfix_decrement>(var);
			}
		}
		else if (match(tk_identifier) && match_next(tk_operator, "=") && data_type.has_value()) {
			auto variable_name = parse_identifier();

			consume(tk_operator);

			auto assigned_value = parse_expression(data_type); // parse right side

			auto assignment_ast = std::make_shared<ast_variable_declaration>(variable_name, data_type.value());

			assignment_ast->add_child(assigned_value);

			return assignment_ast;
		}
		else if (match(tk_number_literal)) {
			if (match_next(tk_operator)) {
				return parse_term(data_type);
			}

			auto number = consume(tk_number_literal).get_lexeme();

			return std::make_shared<ast_number_literal>(number);
		}

		return nullptr;
	}

	std::shared_ptr<ast> parser::parse_condition() {
		return {};
	}

	std::shared_ptr<ast> parser::parse_keywords(bool nested) {
		if (match(tk_keyword, "fn") && !nested) {
			return parse_function();
		}
		else if (match(tk_keyword, "i8")) {
			consume(tk_keyword);

			auto var = parse_expression("i8");

			return var;
		}
		//else if (match(tk_keyword, "if")) {
		//	consume(tk_keyword);

		//	if (match(tk_keyword, "else")) {
		//		consume(tk_keyword);
		//	}
		//	else {

		//	}
		//}
		//else if (match(tk_keyword, "else")) {
		//	consume(tk_keyword);

		//	if (match(tk_delimiter, "{")) {

		//	}
		//	else {
		//		throw std::exception(parse_exceptions[EXPECTED_BRACKET]);
		//	}
		//}
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