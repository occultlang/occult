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
} // occultlang