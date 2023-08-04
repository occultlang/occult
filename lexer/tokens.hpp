#pragma once
#include <cstddef>
#include <unordered_set>
#include <string>

#define USE_ALL_KEYWORDS false

namespace occultlang {
	enum token_type {
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

	static std::unordered_set<std::string> operator_set = { // add ternary? 
		"==", "=", "*=", "*", "+=", "++", "+",
		"/=", "/", "-=", "--", "-", "%=", "%",
		"!=", "!", ">", ">=", "<", "<=", "&&", "||",
		"&=", "&", "|=", "|", "^=", "^", "<<=", "<<",
		">>=", ">>", "~", ":", "?", "->", "$", "...", "_"
	};

	static std::unordered_set<std::string> delimiter_set = {
		"(", ")", "{", "}", "[", "]", ";", ",", "."
	};

#if USE_ALL_KEYWORDS
	static std::unordered_set<std::string> keyword_set = {
		"fn", "match", "if", "else", "for", "do", "while",
		"return", "break", "import", "struct", "enum",
		"ref", "as", "i8", "i16", "i32", "i64", "u8",
		"u16", "u32", "u64", "f32", "f64", "bool", "char", "string", "void"
	};
#else
	static std::unordered_set<std::string> keyword_set = {
		"fn", "if", "else", "for", "do", "while",
		"return", "break", "as", "i8", "i16", "i32", "i64", "u8",
		"u16", "u32", "u64", "f32", "f64", "bool", "string", "void"
	};
#endif

	class token {
		token_type type;
		std::string lexeme;
		int line;
		int column;
	public:
		token() = default;
		token(token_type type, const std::string& lexeme, int line, int column) : type(type), lexeme(lexeme), line(line), column(column) {}

		token_type get_type();
		std::string get_lexeme();
		int get_line();
		int get_column();
	};
} // occultlang