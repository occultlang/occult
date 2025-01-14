#include "writer.hpp"

namespace occult {
  void writer::push_byte(const std::uint8_t& byte) {
    code.emplace_back(byte);
  }
  
  void writer::push_bytes(const std::initializer_list<std::uint8_t>& bytes) {
    code.insert(code.end(), bytes);
  }
  
  void writer::push_bytes(const std::vector<std::uint8_t> bytes) {
    code.insert(code.end(), bytes.begin(), bytes.end());
  }
  
  const std::vector<std::uint8_t> writer::string_to_bytes(const std::string& str) {
    std::vector<std::uint8_t> vectorized_data;
    vectorized_data.reserve(str.size()); // might be redundant
    
    for (const auto& byte : str) {
      vectorized_data.emplace_back(static_cast<std::uint8_t>(byte));
    }
    
    return vectorized_data;
  }
  
  void writer::push_string(const std::string& str) {
    if (!string_locations.contains(str)) {
      string_locations.insert({str, code.size()});
    }
    
    push_bytes(string_to_bytes(str));
  }
  
  writer::jit_function writer::setup_function() {
    std::cout << "machine code: ";
    for (auto byte : code) {
      std::cout << std::hex << std::setw(2)
                 << std::setfill('0') << std::uppercase
                 << static_cast<std::intptr_t>(byte) << " ";
    }
    
    std::cout << std::dec << std::endl;
    
    std::memcpy(memory, code.data(), code.size());
    
    return reinterpret_cast<jit_function>(memory);
  }
  
  std::vector<std::uint8_t>& writer::get_code() {
    return code;
  }
  
  const std::size_t& writer::get_string_location(const std::string& str) {
    return string_locations[str];
  }
} // namespace occult
