#pragma once
#include <string>
#include "../lexer/lexer.hpp"
#include <optional>

namespace occultlang 
{
    class static_analyzer 
    {
        std::vector<token> tokens;
        bool match(token_type type, std::optional<std::string> value = std::nullopt);
        bool match_and_consume(token_type type, std::optional<std::string> value = std::nullopt);
        std::string error_list;
    public:
        static_analyzer(std::string source) : tokens(lexer{source}.lex()) {}

        ~static_analyzer() = default; // HAVE DESTRUCTOR PRINT ERRORS?

        std::string get_current_line_number();
        std::string get_current_col_number();

        std::string& get_error_list();

        std::string append_error(std::string msg);

        void analyze();
    };
} // occultlang