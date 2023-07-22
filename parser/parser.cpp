#include "parser.hpp"
#include "../lexer/lexer.hpp"

// recursive descent parser

namespace occultlang {
	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_statement() {
		if (match(tk_keyword, "i8")) {
			consume(tk_keyword); // consume "i8"

			auto i8_decl = std::make_shared<ast_data_type>("i8");

			auto var_decl = std::make_shared<ast_variable_declaration>();

			var_decl->add_child(i8_decl);

			auto identifier_tok = consume(tk_identifier); // consume identifier

			auto identifier = std::make_shared<ast_identifier>(identifier_tok.get_lexeme());

			var_decl->add_child(identifier);

			if (match(tk_operator, "=")) { // assignment
				consume(tk_operator); // consume "="

				auto assignment = std::make_shared<ast_assignment>();

				identifier->add_child(assignment);

				auto expression = parse_subtraction();

				assignment->add_child(*expression);
			}

			consume(tk_delimiter); // consume ";"

			return var_decl;
		}
		else if (match(tk_keyword, "fn")) {
			consume(tk_keyword); // consume "fn"

			auto function_decl = std::make_shared<ast_function_declaration>();

			auto identifier_tok = consume(tk_identifier); // consume identifier

			auto identifier = std::make_shared<ast_identifier>(identifier_tok.get_lexeme());

			function_decl->add_child(identifier);

			if (match(tk_delimiter, "(")) {
				consume(tk_delimiter);

				auto args = parse_function_arguments();

				for (auto& arg : *args) {
					function_decl->add_child(arg);
				}
			}

			consume(tk_delimiter); // consume ")"

			consume(tk_operator); // consume "->"

			auto type = consume(tk_keyword, tk_identifier);

			if (data_type_set.count(type.get_lexeme())) {
				function_decl->add_child(std::make_shared<ast_data_type>(type.get_lexeme()));
			}
			else if (user_data_type_set.count(type.get_lexeme())) {
				function_decl->add_child(std::make_shared<ast_data_type>(type.get_lexeme()));
			}
			else {
				return std::unexpected<parse_error>(parse_error::TYPE_DOESNT_EXIST);
			}

			consume(tk_delimiter); // consume "{"

			function_decl->add_child(*parse_statement()); // function body LOL

			consume(tk_delimiter); // consume "}"

			return function_decl;
		}
		else if (match(tk_keyword, "struct")) { // custom data types :D
			consume(tk_keyword);

			auto struct_decl = std::make_shared<ast_struct_declaration>();

			auto identifier_tok = consume(tk_identifier); // consume identifier

			auto identifier = std::make_shared<ast_identifier>(identifier_tok.get_lexeme());

			struct_decl->add_child(identifier);

			user_data_type_set.insert(identifier_tok.get_lexeme());

			consume(tk_delimiter); // consume "{"
			
			while (!match(tk_delimiter, "}")) {
				auto type = consume(tk_keyword, tk_identifier);

				std::shared_ptr<ast_data_type> struct_data = nullptr;

				if (data_type_set.count(type.get_lexeme())) { // check data types
					struct_data = std::make_shared<ast_data_type>(type.get_lexeme());
				}
				else if (user_data_type_set.count(type.get_lexeme())) {
					struct_data = std::make_shared<ast_data_type>(type.get_lexeme());
				}
				else {
					return std::unexpected<parse_error>(parse_error::TYPE_DOESNT_EXIST);
				}

				struct_decl->add_child(struct_data);

				auto type_identifier_tok = consume(tk_identifier); // consume identifier

				auto type_identifier = std::make_shared<ast_identifier>(type_identifier_tok.get_lexeme());

				struct_data->add_child(type_identifier); // name of the data type in the struct

				if (match(tk_delimiter, ",")) {
					consume(tk_delimiter); // consume ","
				}
			}

			consume(tk_delimiter); // consume "}"

			return struct_decl;
		}
		else if (match(tk_keyword, "enum")) {
			consume(tk_keyword);

			auto enum_decl = std::make_shared<ast_enum_declaration>();

			auto identifier_tok = consume(tk_identifier); // consume identifier

			auto identifier = std::make_shared<ast_identifier>(identifier_tok.get_lexeme());

			enum_decl->add_child(identifier);

			if (!match(tk_delimiter, "{")) {
				return std::unexpected<parse_error>(parse_error::EXPECTED_OPEN);
			}

			consume(tk_delimiter);

			while (!match(tk_delimiter, "}")) {
				auto value_name = consume(tk_identifier).get_lexeme();

				enum_decl->add_child(std::make_shared<ast_enum_value>(value_name));

				if (match(tk_delimiter, ",")) {
					consume(tk_delimiter); // consume ","
				}
			}

			consume(tk_delimiter); // consume "}"

			return enum_decl;
		}
		else {

		}
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_struct_body() {
		
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_function_argument(const std::string& keyword) { 
		consume(tk_keyword); // consume keyword

		auto function_arg = std::make_shared<ast_function_argument_declaration>();

		function_arg->add_child(std::make_shared<ast_data_type>(keyword)); // add data type to the declaration

		auto identifier_tok = consume(tk_identifier); // consume identifier

		auto identifier = std::make_shared<ast_identifier>(identifier_tok.get_lexeme());

		function_arg->add_child(identifier);

		if (match(tk_delimiter, ",")) {
			consume(tk_delimiter); // consume ","
		}

		return function_arg;
	}

	std::expected<std::vector<std::shared_ptr<ast>>, parse_error> parser::parse_function_arguments() {
		std::vector<std::shared_ptr<ast>> args;

		while (peek().get_type() == tk_keyword || peek().get_type() == tk_identifier) {
			auto data_type = peek().get_lexeme();

			if (data_type_set.count(data_type)) {
				args.push_back(*parse_function_argument(data_type));
			}
			else if (user_data_type_set.count(data_type)) {
				args.push_back(*parse_function_argument(data_type));
			}
			else {
				return std::unexpected<parse_error>(parse_error::TYPE_DOESNT_EXIST);
			}
		}

		return args;
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_factor() {
		if (match(tk_delimiter, "(")) {
			consume(tk_delimiter);

			auto expression = parse_subtraction();

			if (!expression) {
				return expression;
			}

			if (!match(tk_delimiter, ")")) {
				return std::unexpected<parse_error>(parse_error::EXPECTED_CLOSE);
			}

			consume(tk_delimiter);

			return expression;
		}
		else if (match(tk_number_literal)) {
			auto number = consume(tk_number_literal);

			auto value = std::stoll(number.get_lexeme());

			if (value >= min_i8 && value <= max_i8) {
				return std::make_shared<ast_8int_literal>(value);
			}
			else {
				return std::unexpected<parse_error>(parse_error::INT_OUT_OF_BOUNDS);
			}
		}

		// handle other types and literals

		return std::unexpected<parse_error>(parse_error::EXPECTED_INTEGER);
	}

	template<typename AstType>
	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_term(const std::string& operation, std::function<std::expected<std::shared_ptr<ast>, parse_error>()> parse_next) {
		auto left = parse_next();

		if (!left) {
			return left;
		}

		while (match(tk_operator, operation)) {
			auto operation = consume(tk_operator);

			auto right = parse_next();

			if (!right) {
				return right;
			}

			auto node = std::make_shared<AstType>();

			node->add_child(*left);
			node->add_child(*right);

			left = node;
		}

		return left;
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_multiplication() {
		return parse_term<ast_multiplication>("*", [this]() { return this->parse_factor(); });
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_division() {
		return parse_term<ast_division>("/", [this]() { return this->parse_multiplication(); });
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_modulo() {
		return parse_term<ast_modulo>("%", [this]() { return this->parse_division(); });
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_addition() {
		return parse_term<ast_addition>("+", [this]() { return this->parse_modulo(); });
	}

	std::expected<std::shared_ptr<ast>, parse_error> parser::parse_subtraction() {
		return parse_term<ast_subtraction>("-", [this]() { return this->parse_addition(); });
	}

	std::shared_ptr<ast> parser::parse() {
		std::shared_ptr<ast> root = std::make_shared<ast>();

		while (position < tokens.size() && tokens[position].get_type() != tk_eof) {
			auto statement = parse_statement();

			if (statement) {
				root->add_child(*statement);
			}
		}

		return root;
	}
} //occultlang