#include "tokens.hpp"

namespace occultlang {
    token_type token::get_type() {
        return type;
    }

    std::string token::get_lexeme() {
        return lexeme;
    }

    int token::get_line() {
        return line;
    }

    int token::get_column() {
        return column;
    }

    token_type token::get_type() const {
        return type;
    }

    std::string token::get_lexeme() const {
        return lexeme;
    }

    int token::get_line() const {
        return line;
    }

    int token::get_column() const {
        return column;
    }
} // occultlang