#pragma once
#include <stdexcept>
#include "lexer.hpp"

namespace occult {
  class runtime_error : public std::runtime_error {
      mutable std::string message;
      token_t tk;
  public:
      explicit runtime_error(const std::string &message, token_t tk) : std::runtime_error(message), tk(tk) {}
  
      virtual const char *what() const noexcept override;
  };
}
