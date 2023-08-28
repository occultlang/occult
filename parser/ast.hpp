#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <limits>
#include <cmath>
#include "../data structures/tree.hpp"

namespace occultlang {
	enum ast_type {
		root,
		empty_body,
		body,
		arguments,
		function_declaration,
		variable_declaration,
		postfix_increment,
		index,
		type_cast,
		_new,
		_delete,
		dereference,
		reference,
		postfix_decrement,
		number_literal,
		float_literal,
		string_literal,
		return_statement,
		function_call,
		bool_literal,
		for_declaration,
		identifier,
		break_statement,
		operator_,
		delimiter,
		expression,
		if_declaration,
		else_declaration,
		else_if_declaration,
		while_declaration,
		do_while_declaration
	};

	struct ast : public tree<std::shared_ptr<ast>> {
		ast() : tree<std::shared_ptr<ast>>(nullptr) {}
		virtual ~ast() {}

		virtual ast_type get_type() { return  ast_type::root; }

		virtual std::string to_string() { return "root"; }

		static void visualize(std::shared_ptr<ast> tree, int indent = 0);
	};

	struct ast_empty_body : public ast {
		ast_type get_type() override { return ast_type::empty_body; }

		std::string to_string() override { return "empty body"; }
	};

	struct ast_body : public ast {
		ast_type asttype = ast_type::body;

		ast_type get_type() override { return ast_type::body; }

		std::string to_string() override { return "body"; }
	};

	struct ast_arguments : public ast {
		ast_type asttype = ast_type::arguments;

		ast_type get_type() override { return ast_type::arguments; }

		std::string to_string() override { return "arguments"; }
	};

	struct ast_function_declaration : public ast {
		ast_type asttype = ast_type::function_declaration;

		ast_type get_type() override { return ast_type::function_declaration; }

		std::string name;
		std::string type;

		ast_function_declaration() = default;
		ast_function_declaration(const std::string& name, const std::string& type) : name(name), type(type) {}

		std::string to_string() override { return "function declaration: " + name + " " + type; }
	};

	struct ast_variable_declaration : public ast {
		ast_type asttype = ast_type::variable_declaration;

		ast_type get_type() override { return ast_type::variable_declaration; }

		std::string name;
		std::string type;

		ast_variable_declaration() = default;
		ast_variable_declaration(const std::string& name, const std::string& type) : name(name), type(type) {}
		ast_variable_declaration(const std::string& name) : name(name), type("no_decl_type") {}

		std::string to_string() override { return "variable declaration: " + name + " " + type; }
	};

	struct ast_postfix_increment : public ast {
		ast_type asttype = ast_type::postfix_increment;

		ast_type get_type() override { return ast_type::postfix_increment; }

		std::string target;

		ast_postfix_increment() = default;
		ast_postfix_increment(const std::string& target) : target(target) {}

		std::string to_string() override { return "postfix increment: " + target; }
	};

	struct ast_index : public ast {
		ast_type asttype = ast_type::index;

		ast_type get_type() override { return ast_type::index; }

		std::string target;
		std::string value;

		ast_index() = default;
		ast_index(const std::string& target, const std::string& value) : target(target), value(value) {}

		std::string to_string() override { return "index: " + target + " " + value; }
	};

	struct ast_type_cast : public ast {
		ast_type asttype = ast_type::type_cast;
		
		ast_type get_type() override { return ast_type::type_cast; }

		std::string type;

		ast_type_cast() = default;
		ast_type_cast(const std::string& type) : type(type) {}

		std::string to_string() override { return "type cast: " + type; }
	};

	struct ast_new : public ast {
		ast_type asttype = ast_type::_new;

		ast_type get_type() override { return ast_type::_new; }

		std::string size;

		ast_new() = default;
		ast_new(const std::string& size) : size(size) {}

		std::string to_string() override { return "new: " + size; }
	};

	struct ast_delete : public ast {
		ast_type asttype = ast_type::_delete;

		ast_type get_type() override { return ast_type::_delete; }

		ast_delete() = default;

		std::string to_string() override { return "delete"; }
	};

	struct ast_dereference : public ast {
		ast_type asttype = ast_type::dereference;

		ast_type get_type() override { return ast_type::dereference; }

		std::string target;
		ast_dereference() = default;
		ast_dereference(const std::string& target) : target(target) {}

		std::string to_string() override { return "dereference: " + target; }
	};

	struct ast_reference : public ast {
		ast_type asttype = ast_type::reference;

		ast_type get_type() override { return ast_type::reference; }

		std::string target;
		ast_reference() = default;
		ast_reference(const std::string& target) : target(target) {}

		std::string to_string() override { return "reference: " + target; }
	};

	struct ast_postfix_decrement : public ast {
		ast_type asttype = ast_type::postfix_decrement;

		ast_type get_type() override { return ast_type::postfix_decrement; }
		
		std::string target;

		ast_postfix_decrement() = default;
		ast_postfix_decrement(const std::string& target) : target(target) {}

		std::string to_string() override { return "postfix decrement: " + target; }
	};

	struct ast_number_literal : public ast {
		ast_type asttype = ast_type::number_literal;

		ast_type get_type() override { return ast_type::number_literal; }

		std::string value;

		ast_number_literal() = default;
		ast_number_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "number literal: " + value; }
	};

	struct ast_float_literal : public ast {
		ast_type asttype = ast_type::float_literal;

		ast_type get_type() override { return ast_type::float_literal; }
		
		std::string value;

		ast_float_literal() = default;
		ast_float_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "float literal: " + value; }
	};

	struct ast_string_literal : public ast {
		ast_type asttype = ast_type::string_literal;

		ast_type get_type() override { return ast_type::string_literal; }

		std::string value;

		ast_string_literal() = default;
		ast_string_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "string literal: " + value; }
	};

	struct ast_return_statement : public ast {
		ast_type asttype = ast_type::return_statement;

		ast_type get_type() override { return ast_type::return_statement; }

		std::string to_string() override { return "return statement"; }
	};

	struct ast_function_call : public ast {
		ast_type asttype = ast_type::function_call;
		ast_type get_type() override { return ast_type::function_call; }

		std::string name;

		ast_function_call() = default;
		ast_function_call(const std::string& name) : name(name) {}

		std::string to_string() override { return "function call: " + name; }
	};

	struct ast_bool_literal : public ast {
		ast_type asttype = ast_type::bool_literal;
		ast_type get_type() override { return ast_type::bool_literal; }
		std::string value;

		ast_bool_literal() = default;
		ast_bool_literal(const std::string& value) : value(value) {}

		std::string to_string() override { return "boolean literal: " + value; }
	};

	struct ast_for_declaration : public ast {
		ast_type asttype = ast_type::for_declaration;
		ast_type get_type() override { return ast_type::for_declaration; }
		std::string to_string() override { return "for loop declaration"; }
	};

	struct ast_identifier : public ast {
		ast_type asttype = ast_type::identifier;
		ast_type get_type() override { return ast_type::identifier; }
		std::string name;

		ast_identifier(std::string name) : name(name) {}

		std::string to_string() override { return "identifier: " + name; }
	};

	struct ast_break_statement : public ast {
		ast_type asttype = ast_type::break_statement;
		ast_type get_type() override { return ast_type::break_statement; }
		std::string to_string() override { return "break statement"; }
	};

	struct ast_operator : public ast {
		ast_type asttype = ast_type::operator_;
		ast_type get_type() override { return ast_type::operator_; }
		std::string op;

		ast_operator(std::string op) : op(op) {}

		std::string to_string() override { return "operator: " + op; }
	};

	struct ast_delimiter : public ast {
		ast_type asttype = ast_type::delimiter;
		ast_type get_type() override { return ast_type::delimiter; }
		std::string delim;

		ast_delimiter(std::string delim) : delim(delim) {}

		std::string to_string() override { return "delimiter: " + delim; }
	};

	struct ast_expression : public ast {
		ast_type asttype = ast_type::expression;
		ast_type get_type() override { return ast_type::expression; }
		std::string to_string() override { return "expression"; }
	};

	struct ast_if_declaration : public ast {
		ast_type asttype = ast_type::if_declaration;
		ast_type get_type() override { return ast_type::if_declaration; }
		std::string to_string() override { return "if declaration"; }
	};

	struct ast_else_declaration : public ast {
		ast_type asttype = ast_type::else_declaration;
		ast_type get_type() override { return ast_type::else_declaration; }
		std::string to_string() override { return "else declaration"; }
	};

	struct ast_else_if_declaration : public ast {
		ast_type asttype = ast_type::else_if_declaration;
		ast_type get_type() override { return ast_type::else_if_declaration; }
		std::string to_string() override { return "else if declaration"; }
	};

	struct ast_while_declaration : public ast {
		ast_type asttype = ast_type::while_declaration;
		ast_type get_type() override { return ast_type::while_declaration; }
		std::string to_string() override { return "while declaration"; }
	};

	struct ast_do_while_declaration : public ast {
		ast_type asttype = ast_type::do_while_declaration; 
		ast_type get_type() override { return ast_type::do_while_declaration; }
		std::string to_string() override { return "do while declaration"; }
	};
} // occultlang