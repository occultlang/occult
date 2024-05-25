#pragma once
#include <cstddef>
#include <unordered_set>
#include <string>

namespace occultlang
{
	enum token_type
	{
		tk_identifier,
		tk_keyword,
		tk_comment,
		tk_number_literal,
		tk_float_literal,
		tk_string_literal,
		tk_boolean_literal,
		tk_operator,
		tk_delimiter,
		tk_error,
		tk_eof
	};

	static std::unordered_set<std::string> operator_set = {
		"==", "=", "*=", "*", "+=", "++", "+",
		"/=", "/", "-=", "--", "-", "%=", "%",
		"!=", "!", ">", ">=", "<", "<=", "&&", "||",
		"&=", "&", "|=", "|", "^=", "^", "<<=", "<<",
		">>=", ">>", "~", "?", "$", "...", "_"};

	static std::unordered_set<std::string> delimiter_set = {
		"(", ")", "{", "}", "[", "]", ";", ",", ".", ":", "->"};

	static std::unordered_set<std::string> keyword_set = {
		"fn", "if", "else", "loop", "import", "ptr", "as", "deref", "in", // null is a keyword
		"return", "break", "num", "bool", "rnum", "continue", // rnum is real number
		"str", "true", "false", "void", "while", "for", "match", "array", "generic", "step_by", "range", "default", "case", "compilerbreakpoint"};

	class token
	{
		token_type type;
		std::string lexeme;
		int line;
		int column;

	public:
		token() = default;
		token(token_type type, const std::string &lexeme, int line, int column) : type(type), lexeme(lexeme), line(line), column(column) {}

		token_type get_type();
		std::string get_lexeme();
		int get_line();
		int get_column();
		token_type get_type() const;
		std::string get_lexeme() const;
		int get_line() const;
		int get_column() const;
	};
} // occultlang
