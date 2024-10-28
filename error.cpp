#include "error.hpp"

namespace occult {
  const char *runtime_error::what() const noexcept {
    message = std::runtime_error::what();
    message += "\ngot type \"";
    message += token_t::get_typename(tk.tt);
    message += "\"\nlexeme: ";
    message += tk.lexeme;
    
    return message.c_str();
  }
}
