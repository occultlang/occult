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
} // occultlang