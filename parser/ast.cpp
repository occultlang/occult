#include "ast.hpp"
#include <iostream>
#include <iomanip>

namespace occultlang {
	void ast::visualize(std::shared_ptr<ast> tree, int indent) {
		if (!tree) {
			return;
		}

		std::cout << std::string(indent, ' ') << tree->to_string() << std::endl;

		for (const auto& child : tree->get_children()) {
			visualize(std::static_pointer_cast<ast>(child), indent + 2);
		}
	}

	std::string ast::to_string() {
		return "root";
	}

	std::string ast_string_literal::get_value() {
		return value;
	}

	std::string ast_string_literal::to_string() {
		return "string_literal: " + value;
	}

	bool ast_boolean_literal::get_value() {
		return value;
	}

	std::string ast_boolean_literal::to_string() {
		return "boolean_literal: " + (value) ? "true" : "false";
	}

	std::int8_t ast_8int_literal::get_value() {
		return value;
	}

	std::string ast_8int_literal::to_string() {
		return "8int_literal: " + std::to_string(value);
	}

	std::string ast_data_type::to_string() {
		return "data type: " + value;
	}

	std::int16_t ast_16int_literal::get_value() {
		return value;
	}

	std::string ast_16int_literal::to_string() {
		return "16int_literal: " + std::to_string(value);
	}

	std::int32_t ast_32int_literal::get_value() {
		return value;
	}

	std::string ast_32int_literal::to_string() {
		return "32int_literal: " + std::to_string(value);
	}

	std::int64_t ast_64int_literal::get_value() {
		return value;
	}

	std::string ast_64int_literal::to_string() {
		return "64int_literal: " + std::to_string(value);
	}

	std::uint8_t ast_8uint_literal::get_value() {
		return value;
	}

	std::string ast_8uint_literal::to_string() {
		return "8uint_literal: " + std::to_string(value);
	}

	std::uint16_t ast_16uint_literal::get_value() {
		return value;
	}

	std::string ast_16uint_literal::to_string() {
		return "16uint_literal: " + std::to_string(value);
	}

	std::uint32_t ast_32uint_literal::get_value() {
		return value;
	}

	std::string ast_32uint_literal::to_string() {
		return "32uint_literal: " + std::to_string(value);
	}

	std::uint64_t ast_64uint_literal::get_value() {
		return value;
	}

	std::string ast_64uint_literal::to_string() {
		return "64uint_literal: " + std::to_string(value);
	}

	std::float_t ast_32float_literal::get_value() {
		return value;
	}

	std::string ast_32float_literal::to_string() {
		return "32float_literal: " + std::to_string(value);
	}

	std::double_t ast_64float_literal::get_value() {
		return value;
	}

	std::string ast_64float_literal::to_string() {
		return "64float_literal: " + std::to_string(value);
	}

	std::string ast_identifier::get_name() {
		return name;
	}

	std::string ast_identifier::to_string() {
		return "identifier: " + name;
	}

	std::string ast_variable_declaration::to_string() {
		return "variable declaration";
	}

	std::string ast_assignment::to_string() {
		return "assignment";
	}

	std::string ast_multiplication::to_string() {
		return "multiplication";
	}

	std::string ast_division::to_string() {
		return "division";
	}

	std::string ast_addition::to_string() {
		return "addition";
	}

	std::string ast_subtraction::to_string() {
		return "subtraction";
	}

	std::string ast_modulo::to_string() {
		return "modulo";
	}

	std::string ast_function_declaration::to_string() {
		return "function declaration";
	}

	std::string ast_function_argument_declaration::to_string() {
		return "function argument";
	}

	std::string ast_struct_declaration::to_string() {
		return "struct declaration";
	}
} // occultlang