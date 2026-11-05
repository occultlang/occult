#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include "../lexer/lexer.hpp"

namespace occult {
    class parsing_error final : public std::runtime_error {
        mutable std::string message;
        mutable std::string expected;
        token_t tk;
        std::size_t curr_pos;
        const char* func_name;

        mutable std::string source_file;

    public:
        explicit parsing_error(std::string expected, token_t tk, const std::size_t& curr_pos, const char* func_name) :
            std::runtime_error("[PARSE ERROR] "), expected(std::move(expected)), tk(std::move(tk)), curr_pos(curr_pos), func_name(func_name) {}

        void set_context(std::string file) const { source_file = std::move(file); }

        const char* what() const noexcept override;

        token_t get_token() const noexcept { return tk; }
        std::string get_expected() const noexcept { return expected; }
        std::size_t get_position() const noexcept { return curr_pos; }
    };
} // namespace occult
