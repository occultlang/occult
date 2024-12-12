#pragma once
#include "../libs/fast_float.hpp"

#define BINARY_BASE 2
#define DECIMAL_BASE 10
#define OCTAL_BASE 8
#define HEX_BASE 16

namespace occult {
  bool is_octal(const char& c) {
    return (c >= '0' && c <= '7');
  }
  
  bool is_hex(const char& c) {
    return (std::isdigit(c) || (std::tolower(c) >= 'a' && std::tolower(c) <= 'f'));
  }

  bool is_binary(const char& c) {
    return (c == '0' || c == '1');
  }
  
  template<typename ValueType>
  std::string to_parsable_type(const std::string& number, std::optional<std::uintptr_t> base = std::nullopt) {
    ValueType value;
    
    if constexpr (std::is_floating_point_v<ValueType>) {
      auto result = fast_float::from_chars(number.data(), number.data() + number.size(), value);
      
      if (result.ec != std::errc()) {
        throw std::runtime_error("failed parsing number");
      }
    }
    else if constexpr (std::is_integral_v<ValueType>) {
      if (!base.has_value()) {
        throw std::runtime_error("base is required parsing integers");
      }
      
      auto result = fast_float::from_chars(number.data(), number.data() + number.size(), value, static_cast<int>(base.value()));
      
      if (result.ec != std::errc()) {
        throw std::runtime_error("failed parsing number");
      }
    }
    
    return std::to_string(value);
  }
} // namespace occult
