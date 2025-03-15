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
  
  std::size_t writer::push_string(const std::string& str) {
    if (!string_locations.contains(str)) {
      string_locations.insert({str, code.size()});
    }
    
    auto initial = code.size();
    
    push_bytes(string_to_bytes(str));
    
    return initial + 1;
  }
  
  jit_function writer::setup_function() {
    if (code.size() > allocated_size) {
      size_t new_size = ((code.size() / page_size) + 1) * page_size;
      void* new_memory = mmap(nullptr, new_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (new_memory == MAP_FAILED) {
        throw std::runtime_error("failed to allocate additional memory");
      }
    
      std::memcpy(new_memory, memory, allocated_size);
    
      munmap(memory, allocated_size);
    
      memory = new_memory;
      allocated_size = new_size;
    }
    
    std::memcpy(memory, code.data(), code.size());
    
    return reinterpret_cast<jit_function>(memory);
  }
  
  std::vector<std::uint8_t>& writer::get_code() {
    return code;
  }
  
  void writer::print_bytes() {
    for (auto& c : code) {
      std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(c) << " " << std::dec;
    }
    
    std::cout << "\n";
  }
  
  const std::size_t& writer::get_string_location(const std::string& str) {
    return string_locations[str];
  }
} // namespace occult
