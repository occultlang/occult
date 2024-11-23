#pragma once
#include "fast_float.hpp"

namespace occult {
  bool isoctal(char c) {
    return (c >= '0' && c <= '7');
  }
  
  bool ishex(char c) {
    return (std::isdigit(c) || (std::tolower(c) >= 'a' && std::tolower(c) <= 'f'));
  }

  bool isbinary(char c) {
    return (c == '0' || c == '1');
  }
  
  std::string octal_to_dec(std::string number) {
    
  }
  
  std::string hex_to_dec(std::string number) {
    
  }
  
  std::string binary_to_dec(std::string number) {
    
  }
  
  std::string scientific_to_float(std::string number) {
    
  }
}
