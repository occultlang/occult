#pragma once
#include <stdexcept>
#include "../lexer/lexer.hpp"

namespace occult {
    class parsing_error final : public std::runtime_error {
        mutable std::string message;
        mutable std::string expected;
        token_t tk;
        int curr_pos;
        const char *func_name;

    public:
        explicit parsing_error(const std::string &expected, const token_t &tk, const int curr_pos,
                               const char *func_name) : std::runtime_error("[PARSE ERROR] "), expected(expected),
                                                        tk(tk), curr_pos(curr_pos), func_name(func_name) {}

        const char *what() const noexcept override;

        token_t get_token() const noexcept { return tk; }
        std::string get_expected() const noexcept { return expected; }
        int get_position() const noexcept { return curr_pos; }
        std::string get_message() const noexcept { return message; }
    };
} // namespace occult
