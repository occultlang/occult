#include "error.hpp"

namespace occultlang
{
    const char *occ_runtime_error::what() const noexcept
    {
        message = std::runtime_error::what();
        message += "\n\n";

        message += std::string("Lexeme: ") + tk.get_lexeme() + "\n"; // Print the lexeme
        message += std::string("Line: ") + std::to_string(tk.get_line()) + "\n";
        message += std::string("Column: ") + std::to_string(tk.get_column()) + "\n";
        message += std::string("Type: ") + lexer::get_typename(tk.get_type()) + "\n";


        return message.c_str();
    }
} // occultlang