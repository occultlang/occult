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
		std::shared_ptr<ast_body> body;
		std::shared_ptr<ast_arguments> arguments;

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

	/*struct ast_binary : public ast {
		std::shared_ptr<ast> left_operand;
		std::shared_ptr<ast> right_operand;

		ast_binary(std::shared_ptr<ast> left, std::shared_ptr<ast> right)
			: left_operand(left), right_operand(right) {
		}
		
		ast_binary() = default;
	};*/

	struct ast_addition : public ast {//public ast_binary {
		//ast_addition(std::shared_ptr<ast> left, std::shared_ptr<ast> right) : ast_binary(left, right) {}

		std::string to_string() override { return "addition"; }
	};

	struct ast_subtraction : public ast {//public ast_binary {
		//ast_subtraction(std::shared_ptr<ast> left, std::shared_ptr<ast> right) : ast_binary(left, right) {}

		std::string to_string() override { return "subtraction"; }
	};

	struct ast_multiplication : public ast {//public ast_binary {
		//ast_multiplication(std::shared_ptr<ast> left, std::shared_ptr<ast> right) : ast_binary(left, right) {}

		std::string to_string() override { return "multiplication"; }
	};

	struct ast_division : public ast {//public ast_binary {
		//ast_division(std::shared_ptr<ast> left, std::shared_ptr<ast> right) : ast_binary(left, right) {}

		std::string to_string() override { return "division"; }
	};
	
	struct ast_modulus : public ast {// public ast_binary {
		//ast_modulus(std::shared_ptr<ast> left, std::shared_ptr<ast> right) : ast_binary(left, right) {}

		std::string to_string() override { return "modulus"; }
	};
} // occultlang