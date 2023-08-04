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

	struct variable {
		std::string name;
		std::string type;
		int scope;
	};

	struct ast_function_declaration : public ast {
		std::string name;
		std::vector<variable> variables;
		std::string type;

		ast_function_declaration(const std::string& name) : name(name) {}

		std::string to_string() override { return "function declaration"; }
	};

	template<typename T>
	struct ast_number_literal : public ast {
		variable var;
		T value;

		std::string to_string() override { return "number literal"; }
	};
} // occultlang