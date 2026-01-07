#include "error.hpp"

namespace occult {
const char *parsing_error::what() const noexcept {
  message = std::runtime_error::what();
  message += "Got \"";
  message += tk.lexeme;
  message += "\", ";
  message += "expected \"";
  message += expected;
  message += "\" at line ";
  message += std::to_string(tk.line);
  message += ", column ";
  message += std::to_string(tk.column);
  message += " (";
  message += std::to_string(curr_pos);
  message += ")";
  message += " in parsing function \"";
  message += func_name;
  message += "\"";

  return message.c_str();
}
} // namespace occult
