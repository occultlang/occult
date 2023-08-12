#include <cstring>
#include <iostream>
#include <sstream>
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

int main(int argc, char* argv[]) {
	occultlang::parser parser{ R"( 
		i8 x = 10 * 3 / 5;
 )"};

	auto ast = parser.parse();

	occultlang::lexer::visualize(parser.get_tokens());

	occultlang::ast::visualize(ast);

	return 0;
}