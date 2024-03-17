#include "lexer.hpp"

namespace occultlang {
	bool isoctal(char c) {
		return (c >= '0' && c <= '7');
	}

	bool ishex(char c) {
		return (std::isdigit(c) || (std::tolower(c) >= 'a' && std::tolower(c) <= 'f'));
	}

	bool isbinary(char c) {
		return (c == '0' || c == '1');
	}

	token lexer::handle_whitespaces() {
		while (std::isspace(source[position])) {
			if (source[position] == '\n' || source[position] == '\r') {
				position++;
				line++;
				column = 1;
			}
			else {
				position++;
				column++;
			}

			if (position >= source.length()) {
				return token{ tk_eof, "", line, column };
			}
		}

		return get_next();
	}

	token lexer::handle_comment() {
		std::string s;
		position += 2;
		column += 2;

		while (position < source.size() && source[position] != '\n' && source[position] != '\r') {
			s += source[position];
			position++;
			column++;
		}

		return token{ tk_comment, s, line, column };
	}

	token lexer::handle_symbol() {
		std::string s;

		while (position < source.size() && (std::isalnum(source[position]) || source[position] == '_')) {
			s += source[position];
			position++;
			column++;
		}

		if (keyword_set.count(s) > 0) {
			return token{ tk_keyword, s, line, column };
		}

		return token{ tk_identifier, s, line, column };
	}

	token lexer::handle_punctuation() {
		std::string s;

		while (position < source.size() && std::ispunct(source[position])) {
			s += source[position];
			if (operator_set.count(s) > 0) {
				if (s == "--" || s == "++") {
					if (std::isalnum(source[position + 1]) || source[position + 1] == '_') {
						position += s.length() - 1; 
						column += s.length() - 1;
						return token{ tk_operator, s, line, column };
					} else {
						position += s.length();
						column += s.length();
					}
				}
				else {
					position++;
					column++;
				}
			}
			else if (delimiter_set.count(s) > 0) {
				position++;
				column++;
				return token{ tk_delimiter, s, line, column };
			}
			else {
				break;
			}
		}

		if (operator_set.count(s) > 0) {
			return token{ tk_operator, s, line, column };
		}

		return token{ tk_error, "unexpected character", line, column };
	}

	token lexer::handle_float() {
		std::string s;

		s += source[position];
		s += source[position + 1];
		position += 2;
		column += 2;

		while (position < source.size() && std::isdigit(source[position])) {
			s += source[position];
			position++;
			column++;
		}

		return token{ tk_float_literal, s, line, column };
	}

	token lexer::handle_string() {
		std::string s;

		position++;
		column++;

		while (position < source.size() && source[position] != '"') {
			s += source[position];
			position++;
			column++;
		}

		position++;
		column++;

		return token{ tk_string_literal, s, line, column };
	}

	token lexer::handle_hex() {
		std::string s;

		s += source[position];
		position += 2;
		column += 2;

		while (ishex(source[position])) {
			s += source[position];
			position++;
			column++;
		}

		s = std::to_string(std::stoll(s, nullptr, 16));

		return token{ tk_number_literal, s, line, column };
	}

	token lexer::handle_binary() {
		std::string s;

		s += source[position];
		position += 2;
		column += 2;

		while (isbinary(source[position])) {
			s += source[position];
			position++;
			column++;
		}

		s = std::to_string(std::stoll(s, nullptr, 2));

		return token{ tk_number_literal, s, line, column };
	}

	token lexer::handle_octal() {
		std::string s;

		s += source[position];
		position++;
		column += 2;

		while (isoctal(source[position])) {
			s += source[position];
			position++;
			column++;
		}

		s = std::to_string(std::stoll(s, nullptr, 8));

		return token{ tk_number_literal, s, line, column };
	}

	token lexer::handle_number() {
		std::string s;

		while (position < source.size() && std::isdigit(source[position])) {
			s += source[position];
			position++;
			column++;
		}

		return token{ tk_number_literal, s, line, column };
	}

	token lexer::get_next() {
		if (position >= source.length()) {
			return token{ tk_eof, "", line, column };
		}

		if (std::isspace(source[position])) { // whitespaces
			return handle_whitespaces();
		}

		if (source[position] == '/' && source[position + 1] == '/') { // comment
			return handle_comment();
		}

		if (std::isalpha(source[position])) { // keywords and identifiers
			return handle_symbol();
		}

		if (std::ispunct(source[position]) && source[position] != '"') { // operators and delimiters
			return handle_punctuation();
		}

		if (std::isdigit(source[position]) && source[position + 1] == '.') { // float literals
			return handle_float();
		}

		if (std::isdigit(source[position])) { // number literals
			if (source[position] == '0' && source[position + 1] == 'x' || source[position + 1] == 'X') { // hex
				return handle_hex();
			}

			if (source[position] == '0' && source[position + 1] == 'b' || source[position + 1] == 'B') { // binary
				return handle_binary();
			}

			if (source[position] == '0') { // octal
				return handle_octal();
			}

			return handle_number();
		}

		if (source[position] == '"') { // string literals
			return handle_string();
		}

		return token{ tk_error, "unexpected character", line, column };
	}

	std::vector<token> lexer::lex() {
		std::vector<token> tokens;

		token current_token = get_next();

		while (true) {
			tokens.push_back(current_token);

			current_token = get_next();

			if (current_token.get_type() == tk_error || current_token.get_type() == tk_eof) break;
		}

		return tokens;
	}

	std::string lexer::get_typename(token_type tt) {
		switch (tt) {
		case tk_identifier:
			return "identifier";
		case tk_keyword:
			return "keyword";
		case tk_comment:
			return "comment";
		case tk_number_literal:
			return "number literal";
		case tk_float_literal:
			return "float literal";
		case tk_string_literal:
			return "string literal";
		case tk_boolean_literal:
			return "boolean literal";
		case tk_operator:
			return "operator";
		case tk_delimiter:
			return "delimiter";
		case tk_error:
			return "error";
		case tk_eof:
			return "eof";
		default:
			return "unknown";
		}
	}

	void lexer::visualize(std::vector<token>& tokens) {
		for (auto& tk : tokens) {
			std::cout << "token type: " << get_typename(tk.get_type()) << std::endl;
			std::cout << "lexeme: " << tk.get_lexeme() << std::endl;
			std::cout << "line: " << tk.get_line() << std::endl;
			std::cout << "column: " << tk.get_column() << std::endl;
			std::cout << std::endl;
		}
	}
} // occultlang