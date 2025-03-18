#pragma once
#include <stdexcept>
#include "../lexer/lexer.hpp"

namespace occult {
  class runtime_error : public std::runtime_error {
      mutable std::string message;
      token_t tk;
      int curr_pos;
  public:
      explicit runtime_error(const std::string &message, token_t tk, int curr_pos) : std::runtime_error(message), tk(tk), curr_pos(curr_pos) {}
  
      virtual const char *what() const noexcept override;
  };
} // namespace occult
