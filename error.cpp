#include "error.hpp"

namespace occult {
  const char *runtime_error::what() const noexcept {
    message = std::runtime_error::what();
    message += "\n Type: ";
    message += token_t::get_typename(tk.tt);
    message += "\n Lexeme: ";
    message += tk.lexeme;
    
    return message.c_str();
  }
}
