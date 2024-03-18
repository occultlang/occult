#pragma once
#include "tokens.hpp"
#include <string>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <vector>

namespace occultlang
{
	class lexer
	{
		std::string source;
		int position;
		int line;
		int column;

	public:
		lexer(const std::string &source) : source(source), position(0), line(1), column(1) {}

		token handle_whitespaces();
		token handle_comment();
		token handle_symbol();
		token handle_punctuation();
		token handle_float();
		token handle_string();
		token handle_hex();
		token handle_binary();
		token handle_octal();
		token handle_number();
		token get_next();
		std::vector<token> lex();
		static std::string get_typename(token_type tt);
		static void visualize(std::vector<token> &tokens);
	};
} // occultlang
