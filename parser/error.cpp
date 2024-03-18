#include "error.hpp"

namespace occultlang
{
    const char *occ_runtime_error::what() const noexcept
    {
        message = std::runtime_error::what();
        message += "\n\n";

        message += std::string("Lexeme: ") + tk.get_lexeme() + "\n"; // Print the lexeme

        return message.c_str();
    }
} // occultlang