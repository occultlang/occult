#include <cstring>
#include <iostream>
#include <sstream>
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

int main(int argc, char* argv[]) {
	occultlang::parser parser{ R"(
		fn main() i32 {
			f32 x = 0.123 + 0.4;
		}
	)"};

	auto ast = parser.parse();

	occultlang::lexer::visualize(parser.get_tokens());

	occultlang::ast::visualize(ast);

	return 0;
}