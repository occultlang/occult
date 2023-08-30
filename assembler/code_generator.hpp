#pragma once
#include "../parser/parser.hpp"

namespace occultlang {
	class code_generator {
	private:
		std::shared_ptr<ast> root;
	public:
		code_generator(std::shared_ptr<ast> root) : root{ root } {}

		std::string generate(std::shared_ptr<ast> node);
		std::string generate();
	};
} // occultlang