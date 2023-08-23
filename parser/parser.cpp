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

		auto astargs = std::make_shared<ast_arguments>();
		auto rbody = std::make_shared<ast_body>();

		function_decl->add_child(astargs); // not in final ast
		function_decl->add_child(rbody); // not in final ast

		for (auto& arg : arguments) {
			astargs->add_child(arg);
		}

		if (match(tk_delimiter, "{")) {
			consume(tk_delimiter);

			while (!match(tk_delimiter, "}")) {
				auto body = parse_keywords(true);

				rbody->add_child(body);
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

	std::vector<std::shared_ptr<ast>> parser::parse_term() {
		std::vector<std::shared_ptr<ast>> vec;

		while (true) {
			if (match(tk_identifier)) {
				vec.push_back(std::make_shared<ast_identifier>(parse_identifier()));
			}

			if (match(tk_number_literal)) {
				vec.push_back(std::make_shared<ast_number_literal>(consume(tk_number_literal).get_lexeme()));
			}

			if (match(tk_float_literal)) {
				vec.push_back(std::make_shared<ast_float_literal>(consume(tk_float_literal).get_lexeme()));
			}

			if (match(tk_operator)) {
				vec.push_back(std::make_shared<ast_operator>(consume(tk_operator).get_lexeme()));
			}

			if (match(tk_delimiter, "(") || match(tk_delimiter, ")")) {
				vec.push_back(std::make_shared<ast_delimiter>(consume(tk_delimiter).get_lexeme()));
			}

			if (match(tk_delimiter, ";") || match(tk_delimiter, "{")) {
				break;
			}
		}

		return vec;
	}

	std::shared_ptr<ast> parser::parse_expression(std::optional<std::string> data_type) {
		if (match(tk_identifier) && match_next(tk_operator, "++") || match_next(tk_operator, "--")) { // postfix increment
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
		else if (match(tk_keyword, "true")) {
			consume(tk_keyword);

			return std::make_shared<ast_bool_literal>("true");
		}
		else if (match(tk_keyword, "false")) {
			consume(tk_keyword);

			return std::make_shared<ast_bool_literal>("false");
		}
		else if (match(tk_number_literal) && match_next(tk_operator) || match(tk_identifier) && match_next(tk_operator) || match(tk_delimiter, "(")) {
			auto expr = parse_term();

			auto real_expr = std::make_shared<ast_expression>();

			for (auto& term : expr) {
				real_expr->add_child(term);
			}

			return real_expr;
		}
		else if (match(tk_float_literal) && match_next(tk_operator) || match(tk_identifier) && match_next(tk_operator) || match(tk_delimiter, "(")) {
			auto expr = parse_term();

			auto real_expr = std::make_shared<ast_expression>();

			for (auto& term : expr) {
				real_expr->add_child(term);
			}

			return real_expr;
		}
		else if (match(tk_number_literal)) {
			auto number = consume(tk_number_literal).get_lexeme();

			return std::make_shared<ast_number_literal>(number);
		}
		else if (match(tk_float_literal)) {
			auto number = consume(tk_float_literal).get_lexeme();

			return std::make_shared<ast_float_literal>(number);
		}
		else if (match(tk_string_literal)) {
			auto str = consume(tk_string_literal).get_lexeme();

			return std::make_shared<ast_string_literal>(str);
		}
		else if (match(tk_identifier)) {
			auto identifier = parse_identifier();

			return std::make_shared<ast_identifier>(identifier);
		}

		return nullptr;
	}

	std::shared_ptr<ast> parser::parse_keywords(bool nested) {
		if (match(tk_keyword, "fn") && !nested) {
			return parse_function();
		}
		else if (match(tk_keyword, "i8")) {
			consume(tk_keyword);

			auto var = parse_expression("i8");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "i16")) {
			consume(tk_keyword);

			auto var = parse_expression("i16");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "i32")) {
			consume(tk_keyword);

			auto var = parse_expression("i32");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "i64")) {
			consume(tk_keyword);

			auto var = parse_expression("i64");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "u8")) {
			consume(tk_keyword);

			auto var = parse_expression("u8");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "u16")) {
			consume(tk_keyword);

			auto var = parse_expression("u16");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "u32")) {
			consume(tk_keyword);

			auto var = parse_expression("u32");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "u64")) {
			consume(tk_keyword);

			auto var = parse_expression("u64");

			consume(tk_delimiter);

			return var;
		}
		else if (match(tk_keyword, "bool")) {
			consume(tk_keyword);

			auto expr = parse_expression("bool");

			consume(tk_delimiter);

			return expr;
		}
		else if (match(tk_keyword, "string")) {
			consume(tk_keyword);

			auto expr = parse_expression("string");

			consume(tk_delimiter);

			return expr;
		}
		else if (match(tk_keyword, "return")) {
			consume(tk_keyword);

			auto expr = parse_expression({});

			consume(tk_delimiter);

			auto return_stmt = std::make_shared<ast_return_statement>();

			return_stmt->add_child(expr);

			return return_stmt;
		}
		else if (match(tk_keyword, "do")) {
			consume(tk_keyword);

			auto decl = std::make_shared<ast_do_while_declaration>();

			auto b = std::make_shared<ast_body>();

			decl->add_child(b);

			if (match(tk_delimiter, "{")) {
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) {
					auto body = parse_keywords(true);

					b->add_child(body);
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

			consume(tk_keyword);

			auto term = parse_expression({});

			decl->add_child(term);

			consume(tk_delimiter);

			return decl;
		}
		else if (match(tk_keyword, "while")) {
			consume(tk_keyword);

			auto decl = std::make_shared<ast_while_declaration>();

			auto b = std::make_shared<ast_body>();

			auto term = parse_expression({});

			decl->add_child(term);

			decl->add_child(b);

			if (match(tk_delimiter, "{")) {
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) {
					auto body = parse_keywords(true);

					b->add_child(body);
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

			return decl;
		}
		else if (match(tk_keyword, "if")) {
			consume(tk_keyword);

			auto decl = std::make_shared<ast_if_declaration>();

			auto b = std::make_shared<ast_body>();

			auto term = parse_expression({});

			decl->add_child(term);

			decl->add_child(b);

			if (match(tk_delimiter, "{")) {
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) {
					auto body = parse_keywords(true);

					b->add_child(body);
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

			return decl;
		}
		else if (match(tk_keyword, "else") && match_next(tk_keyword, "if")) {
			consume(tk_keyword);
			consume(tk_keyword);

			auto decl = std::make_shared<ast_else_if_declaration>();

			auto b = std::make_shared<ast_body>();

			auto term = parse_expression({});

			decl->add_child(term);

			decl->add_child(b);

			if (match(tk_delimiter, "{")) {
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) {
					auto body = parse_keywords(true);

					b->add_child(body);
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

			return decl;
		}
		else if (match(tk_keyword, "else")) {
			consume(tk_keyword);

			auto if_decl = std::make_shared<ast_else_declaration>();

			auto b = std::make_shared<ast_body>();

			if_decl->add_child(b);

			if (match(tk_delimiter, "{")) {
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) {
					auto body = parse_keywords(true);

					b->add_child(body);
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

			return if_decl;
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, "(")) {
			std::cout << "function call" << std::endl;

			auto function_name = parse_identifier();

			consume(tk_delimiter); // Consume '('

			std::vector<std::shared_ptr<ast>> arguments;

			while (!match(tk_delimiter, ")")) {
				auto arg_expr = parse_expression({});

				arguments.push_back(arg_expr);

				parse_comma();
			}

			consume(tk_delimiter); // Consume ')'

			auto function_call = std::make_shared<ast_function_call>(function_name);

			auto astargs = std::make_shared<ast_arguments>();

			function_call->add_child(astargs);

			for (auto& arg : arguments) {
				astargs->add_child(arg);
			}

			consume(tk_delimiter);

			return function_call;
		}
		else if (match(tk_identifier)) {
			auto expr = parse_expression({});

			if (match(tk_delimiter, ";")) {
				consume(tk_delimiter);
			}

			return expr;
		}
		else if (match(tk_keyword, "break")) {
			consume(tk_keyword);

			if (match(tk_delimiter, ";")) {
				consume(tk_delimiter);
			}

			return std::make_shared<ast_break_statement>();
		}
		else if (match(tk_keyword, "f32")) {
			consume(tk_keyword);

			auto expr = parse_expression("f32");

			consume(tk_delimiter);

			return expr;
		}
		else if (match(tk_keyword, "f64")) {
			consume(tk_keyword);

			auto expr = parse_expression("f64");

			consume(tk_delimiter);

			return expr;
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