#pragma once
#include <stdexcept>
#include "../lexer/lexer.hpp"

namespace occultlang
{
    class occ_runtime_error : public std::runtime_error
    {
    private:
        token tk;
        mutable std::string message;

    public:
        explicit occ_runtime_error(const std::string &message, token tk) : std::runtime_error(message), tk(tk) {}

        virtual const char *what() const noexcept override;
    };
} // occultlang
