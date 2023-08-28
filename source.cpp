#include <cstring>
#include <iostream>
#include <sstream>
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "translator/code_generator.hpp"

int main(int argc, char* argv[]) {
	std::string source_original = R"(fn main() i32 { })";
	
	std::cout << "original source\n" << source_original << "\n\n";

	occultlang::parser parser{ source_original };
	
	auto ast = parser.parse();

	//occultlang::lexer::visualize(parser.get_tokens());

	//occultlang::ast::visualize(ast);

	//std::cout << std::endl;

	occultlang::code_generator generator{ ast };

	auto source = generator.generate();

	std::cout << "generated source\n" << source << "\n\n";

	return 0;
}