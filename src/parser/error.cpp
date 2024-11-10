#include "error.hpp"

namespace occult {
  const char *runtime_error::what() const noexcept {
    message = std::runtime_error::what();
    message += "\ngot type \"";
    message += token_t::get_typename(tk.tt);
    message += "\"\nlexeme: ";
    message += tk.lexeme;
    message += "\"\nposition in stream: ";
    message += std::to_string(curr_pos);
    
    return message.c_str();
  }
}
