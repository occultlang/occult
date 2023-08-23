#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <limits>
#include <cmath>
#include "../data structures/tree.hpp"

namespace occultlang {
	struct ast : public tree<std::shared_ptr<ast>> {
		ast() : tree<std::shared_ptr<ast>>(nullptr) {}
		virtual ~ast() {}

		virtual std::string to_string() { return "root"; }

		static void visualize(std::shared_ptr<ast> tree, int indent = 0);
	};

	struct ast_empty_body : public ast {
		std::string to_string() override { return "empty body"; }
	};

	struct ast_body : public ast {
		std::string to_string() override { return "body"; }
	};

	struct ast_arguments : public ast {
		std::string to_string() override { return "arguments"; }
	};

	struct ast_function_declaration : public ast {
		std::string name;
		std::string type;

		ast_function_declaration() = default;
		ast_function_declaration(const std::string& name, const std::string& type) : name(name), type(type) {}

		std::string to_string() override { return "function declaration: " + name + " " + type; }
	};

	struct ast_variable_declaration : public ast {
		std::string name;
		std::string type;

		ast_variable_declaration() = default;
		ast_variable_declaration(const std::string& name, const std::string& type) : name(name), type(type) {}

		std::string to_string() override { return "variable declaration: " + name + " " + type; }
	};

	struct ast_postfix_increment : public ast {
		std::string target;

		ast_postfix_increment() = default;
		ast_postfix_increment(const std::string& target) : target(target) {}

		std::string to_string() override { return "postfix increment: " + target; }
	};

	struct ast_postfix_decrement : public ast {
		std::string target;

		ast_postfix_decrement() = default;
		ast_postfix_decrement(const std::string& target) : target(target) {}

		std::string to_string() override { return "postfix decrement: " + target; }
	};

	struct ast_number_literal : public ast {
		std::string value;

		ast_number_literal() = default;
		ast_number_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "number literal: " + value; }
	};

	struct ast_float_literal : public ast {
		std::string value;

		ast_float_literal() = default;
		ast_float_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "float literal: " + value; }
	};

	struct ast_string_literal : public ast {
		std::string value;

		ast_string_literal() = default;
		ast_string_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "string literal: " + value; }
	};

	struct ast_return_statement : public ast {
		std::string to_string() override { return "return statement"; }
	};

	struct ast_function_call : public ast {
		std::string name;

		ast_function_call() = default;
		ast_function_call(const std::string& name) : name(name) {}

		std::string to_string() override { return "function call: " + name; }
	};

	struct ast_bool_literal : public ast {
		std::string value;

		ast_bool_literal() = default;
		ast_bool_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "boolean literal: " + value; }
	};

	struct ast_for_declaration : public ast {
		std::string to_string() override { return "for loop declaration"; }
	};

	struct ast_identifier : public ast {
		std::string name;

		ast_identifier(std::string name) : name(name) {}

		std::string to_string() override { return "identifier: " + name; }
	};
	
	struct ast_break_statement : public ast {
		std::string to_string() override { return "break statement"; }
	};

	struct ast_operator : public ast {
		std::string op;

		ast_operator(std::string op) : op(op) {}

		std::string to_string() override { return "operator: " + op; }
	};

	struct ast_delimiter : public ast {
		std::string delim;

		ast_delimiter(std::string delim) : delim(delim) {}

		std::string to_string() override { return "delimiter: " + delim; }
	};

	struct ast_expression : public ast {
		std::string to_string() override { return "expression"; }
	};

	struct ast_if_declaration : public ast {
		std::string to_string() override { return "if declaration"; }
	};

	struct ast_else_declaration : public ast {
		std::string to_string() override { return "else declaration"; }
	};

	struct ast_else_if_declaration : public ast {
		std::string to_string() override { return "else if declaration"; }
	};

	struct ast_while_declaration : public ast {
		std::string to_string() override { return "while declaration"; }
	};

	struct ast_do_while_declaration : public ast {
		std::string to_string() override { return "do while declaration"; }
	};
} // occultlang