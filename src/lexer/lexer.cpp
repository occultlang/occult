#include "lexer.hpp"
#include <sstream>
#include "lexer_maps.hpp"
#include "number_parser.hpp"

namespace occult {
    std::string token_t::get_typename(const token_type& tt) {
        if (token_typename_map.contains(tt)) {
            return token_typename_map[tt];
        }

        throw std::runtime_error("Can't find token typename");
    }

    void lexer::increment(const std::uintptr_t& _line, const std::uintptr_t& _pos, const std::uintptr_t& _column) {
        this->line += _line;
        this->pos += _pos;
        this->column += _column;
    }

    void lexer::handle_comment() {
        if (pos + 1 < source.size()) {
            if (const auto& lexeme = std::string() + source[pos] + source[pos + 1]; comment_map.contains(lexeme)) {
                increment(0, 2, 2); // beginning comment

                if (comment_map[lexeme] == comment_tt) {
                    while (pos < source.size() && source[pos] != '\n' && source[pos] != '\r') {
                        increment(0, 1, 1);
                    }
                }
                else if (comment_map[lexeme] == multiline_comment_start_tt) {
                    while (pos < source.size()) {
                        if (pos + 1 < source.size()) {
                            if (const auto& end_lexeme = std::string() + source[pos] + source[pos + 1]; comment_map[end_lexeme] == multiline_comment_end_tt) {
                                increment(0, 2, 2); // skip the closing */
                                break;
                            }
                        }
                        if (source[pos] == '\n' || source[pos] == '\r') {
                            increment(1, 0, 0);
                        }
                        increment(0, 1, 1);
                    }
                }
            }
        }
    }

    void lexer::handle_whitespace() {
        if (whitespace_map.contains(source[pos])) {
            if (source[pos] == '\n' || source[pos] == '\r') {
                increment(1, 1, 0);
                column = 1;
            }
            else {
                increment(0, 1, 1);
            }
        }
    }

    token_t lexer::handle_whitespace_with_token() {
        std::string lexeme;

        if (whitespace_map.contains(source[pos])) {
            lexeme += source[pos];

            if (source[pos] == '\n' || source[pos] == '\r') {
                increment(1, 1, 0);
                column = 1;
            }
            else {
                increment(0, 1, 1);
            }
        }

        return token_t(line, column, lexeme, whitespace_tt);
    }

    std::string lexer::handle_escape_sequences(const char& type) {
        std::string lexeme;

        while (pos < source.length() && source[pos] != type) {
            if (source[pos] == '\\') {
                increment(0, 1, 1);

                if (pos < source.length()) {
                    switch (source[pos]) {
                    case 'n':
                        lexeme += '\n';
                        break;
                    case 'r':
                        lexeme += '\r';
                        break;
                    case 't':
                        lexeme += '\t';
                        break;
                    case '\\':
                        lexeme += '\\';
                        break;
                    case '\"':
                        lexeme += '\"';
                        break;
                    default:
                        lexeme += source[pos];
                        break;
                    }
                }
            }
            else {
                lexeme += source[pos];
            }

            increment(0, 1, 1);
        }

        return lexeme;
    }

    token_t lexer::handle_string() {
        increment(0, 1, 1);

        const auto lexeme = handle_escape_sequences('\"');

        if (pos < source.length() && source[pos] == '\"') {
            increment(0, 1, 1);
        }

        return token_t(line, column, lexeme, string_literal_tt);
    }

    token_t lexer::handle_char() {
        increment(0, 1, 1);

        const auto lexeme = handle_escape_sequences('\'');

        if (pos < source.length() && source[pos] == '\'') {
            increment(0, 1, 1);
        }

        return token_t(line, column, lexeme, char_literal_tt);
    }

    token_t lexer::handle_numeric() {
        if (source[pos] == '0') {
            increment(0, 1, 1);

            if (source[pos] == 'x' || source[pos] == 'X') {
                increment(0, 1, 1);

                std::string hex_number;
                while (is_hex(source[pos])) {
                    hex_number += source[pos];
                    increment(0, 1, 1);
                }

                hex_number = to_parsable_type<std::uintptr_t>(hex_number, HEX_BASE);
                return token_t(line, column, hex_number, number_literal_tt);
            }

            if (source[pos] == 'b' || source[pos] == 'B') {
                increment(0, 1, 1);

                std::string binary_number;
                while (is_binary(source[pos])) {
                    binary_number += source[pos];
                    increment(0, 1, 1);
                }

                binary_number = to_parsable_type<std::uintptr_t>(binary_number, BINARY_BASE);
                return token_t(line, column, binary_number, number_literal_tt);
            }

            if (source[pos] == '.') {
                pos--;
                column--;
            }
            else {
                std::uintptr_t octal_value = 0;
                while (is_octal(source[pos])) {
                    octal_value = octal_value * OCTAL_BASE + (source[pos] - '0');
                    increment(0, 1, 1);
                }

                return token_t(line, column, std::to_string(octal_value), number_literal_tt);
            }
        }

        std::string normal_number;
        bool has_decimal_point = false, has_exponent = false;

        while (numeric_set.contains(source[pos]) || source[pos] == '.' || source[pos] == 'e' || source[pos] == 'E' || source[pos] == '-' || source[pos] == '+') {
            normal_number += source[pos];
            increment(0, 1, 1);
        }

        if (normal_number.contains('.')) {
            has_decimal_point = true;
        }

        if (normal_number.contains('e') || normal_number.contains('E')) {
            has_exponent = true;
        }

        if (has_decimal_point || has_exponent) {
            normal_number = to_parsable_type<double>(normal_number);
            return token_t(line, column, normal_number, float_literal_tt);
        }

        normal_number = to_parsable_type<std::uintptr_t>(normal_number, DECIMAL_BASE);
        return token_t(line, column, normal_number, number_literal_tt);
    }

    token_t lexer::handle_delimiter() {
        increment(0, 1, 1);

        return token_t(line, column, std::string(1, source[pos - 1]), delimiter_map[source[pos - 1]]);
    }

    token_t lexer::handle_operator(const operator_type& op_type) {
        if (pos + 1 < source.size() && source[pos] == '/' && (source[pos + 1] == '*' || source[pos + 1] == '/')) {
            handle_comment();
            return get_next_token();
        }

        if (op_type == operator_type::three) {
            if (pos + 2 < source.size()) {
                increment(0, 3, 3);
                auto lexeme = std::string() + source[pos - 3] + source[pos - 2] + source[pos - 1];
                return token_t(line, column, lexeme, operator_map_triple[lexeme]);
            }
        }
        else if (op_type == operator_type::two) {
            if (pos + 1 < source.size()) {
                increment(0, 2, 2);
                auto lexeme = std::string() + source[pos - 2] + source[pos - 1];
                return token_t(line, column, lexeme, operator_map_double[lexeme]);
            }
        }
        else {
            increment(0, 1, 1);
            return token_t(line, column, std::string(1, source[pos - 1]), operator_map_single[source[pos - 1]]);
        }

        return token_t(line, column, "unknown operator", unkown_tt);
    }

    token_t lexer::handle_symbol() {
        const auto start_pos = pos;
        const auto start_column = column;

        while (pos < source.length() && alnumeric_set.contains(source[pos])) {
            increment(0, 1, 1);
        }

        auto lexeme = source.substr(start_pos, pos - start_pos);

        if (lexeme == "else" && pos < source.length() && source[pos] == ' ') {
            auto next_pos = pos + 1;
            while (next_pos < source.length() && source[next_pos] == ' ') {
                ++next_pos;
            }

            if (next_pos < source.length() && source.substr(next_pos, 2) == "if") {
                pos = next_pos + 2;
                column += (pos - start_pos);
                lexeme = "elseif";
            }
        }

        if (keyword_map.contains(lexeme)) {
            return token_t(line, start_column, lexeme, keyword_map[lexeme]);
        }

        return token_t(line, start_column, lexeme, identifier_tt);
    }

    token_t lexer::get_next_token() {
        if (pos >= source.length()) {
            return token_t(line, column, "end of file", end_of_file_tt);
        }

        if (use_whitespace && whitespace_map.contains(source[pos])) {
            return handle_whitespace_with_token();
        }

        handle_whitespace(); // else

        if (pos < source.length() && source[pos] == '\"') {
            return handle_string();
        }

        if (pos < source.length() && source[pos] == '\'') {
            return handle_char();
        }

        if (pos < source.length() && numeric_set.contains(source[pos])) {
            return handle_numeric();
        }

        if (pos < source.length() && delimiter_map.contains(source[pos])) {
            // check for variadic "..." before treating '.' as a delimiter
            if (source[pos] == '.' && pos + 2 < source.length() && source[pos + 1] == '.' && source[pos + 2] == '.') {
                increment(0, 3, 3);
                return token_t(line, column, "...", variadic_tt);
            }
            return handle_delimiter();
        }

        if (pos + 2 < source.length() && operator_map_triple.contains(std::string() + source[pos] + source[pos + 1] + source[pos + 2])) {
            return handle_operator(operator_type::three);
        }

        if (pos + 1 < source.length() && operator_map_double.contains(std::string() + source[pos] + source[pos + 1])) {
            return handle_operator(operator_type::two);
        }

        if (pos < source.length() && operator_map_single.contains(source[pos])) {
            return handle_operator(operator_type::one);
        }

        if (pos < source.length() && alnumeric_set.contains(source[pos])) {
            return handle_symbol();
        }

        return token_t(line, column, "unknown token", unkown_tt);
    }

    std::vector<token_t> lexer::analyze() {
        std::vector<token_t> token_stream;
        token_t token = get_next_token();
        token_type previous_token_type = end_of_file_tt;

        auto is_unary_context = [](token_type prev_type) {
            return prev_type == left_paren_tt                        // (
                   || prev_type == comma_tt                          // ,
                   || prev_type == left_curly_bracket_tt             // {
                   || prev_type == semicolon_tt                      // ;
                   || prev_type == end_of_file_tt                    // (start of file)
                   || prev_type == return_keyword_tt                 // return
                   || prev_type == not_equals_operator_tt            // !=
                   || prev_type == equals_operator_tt                // ==
                   || prev_type == greater_than_operator_tt          // >
                   || prev_type == less_than_operator_tt             // <
                   || prev_type == greater_than_or_equal_operator_tt // >=
                   || prev_type == less_than_or_equal_operator_tt    // <=
                   || prev_type == assignment_tt                     // =
                   || prev_type == add_assign_tt                     // +=
                   || prev_type == sub_assign_tt                     // -=
                   || prev_type == mul_assign_tt                     // *=
                   || prev_type == div_assign_tt                     // /=
                   || prev_type == mod_assign_tt                     // %=
                   || prev_type == and_assign_tt                     // &=
                   || prev_type == or_assign_tt                      // |=
                   || prev_type == xor_assign_tt                     // ^=
                   || prev_type == lshift_assign_tt                  // <<=
                   || prev_type == rshift_assign_tt;                 // >>=
        };

        while (token.tt != end_of_file_tt) {
            if (token.tt != unkown_tt) { // comments and whitespaces are just unknown
                if (token.tt == add_operator_tt || token.tt == subtract_operator_tt) {
                    if (is_unary_context(previous_token_type)) {
                        token.tt = (token.tt == add_operator_tt) ? unary_plus_operator_tt : unary_minus_operator_tt;
                    }

                    if (token.tt == unary_plus_operator_tt) {
                        token.lexeme = "unary_plus";
                    }
                    else if (token.tt == unary_minus_operator_tt) {
                        token.lexeme = "unary_minus";
                    }
                }
                /*else if (token.tt == bitwise_and_tt || token.tt == multiply_operator_tt)
                { if (is_unary_context(previous_token_type)) { if (token.tt ==
                bitwise_and_tt) { token.tt = reference_operator_tt; token.lexeme =
                "reference_operator"; // ptr reference
                    }
                    else if (token.tt == multiply_operator_tt) {
                      token.tt = dereference_operator_tt;
                      token.lexeme = "dereference_operator"; // ptr dereference
                    }
                  }
                }*/

                token_stream.push_back(token);
                previous_token_type = token.tt;
            }

            token = get_next_token();
        }

        token_stream.push_back(token);

        stream = token_stream;

        return token_stream;
    }

    void lexer::visualize(const std::optional<std::vector<token_t>>& o_s) const {
        if (!o_s.has_value()) {
            for (size_t i = 0; i < stream.size(); ++i) {
                auto s = stream.at(i);

                std::cout << "Lexeme: " << s.lexeme << "\n"
                          << "Type: " << occult::token_t::get_typename(s.tt) << "\n"
                          << "Position in stream: " << i << "\n"
                          << "(Line, Column): " << s.line << ", " << s.column << "\n\n";
            }
        }
        else {
            for (size_t i = 0; i < o_s.value().size(); ++i) {
                auto s = o_s.value().at(i);

                std::cout << "Lexeme: " << s.lexeme << "\n"
                          << "Type: " << occult::token_t::get_typename(s.tt) << "\n"
                          << "Position in stream: " << i << "\n"
                          << "(Line, Column): " << s.line << ", " << s.column << "\n\n";
            }
        }
    }
} // namespace occult
