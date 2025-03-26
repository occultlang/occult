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
#ifdef __linux
  jit_function writer::setup_function() {
    std::size_t required_size = code.size();
  
    if (required_size > allocated_size) {
      std::size_t new_size = ((required_size / page_size) + 1) * page_size;
  

      void* new_memory = mremap(memory, allocated_size, new_size, MREMAP_MAYMOVE);
      if (new_memory == MAP_FAILED) {
        throw std::runtime_error("failed to reallocate memory");
      }
  
      memory = new_memory;
      allocated_size = new_size;
    }
  
    std::memcpy(memory, code.data(), required_size);
  
    return reinterpret_cast<jit_function>(memory);
  }
#endif
#ifdef _WIN64
  jit_function writer::setup_function() {
      if (code.size() > allocated_size) {
          size_t new_size = ((code.size() / page_size) + 1) * page_size;
          DWORD old_protect = 0;
          PVOID new_memory = VirtualAlloc(nullptr, new_size, PAGE_EXECUTE_READWRITE | MEM_COMMIT | MEM_RESERVE, old_protect);

          if (new_memory == nullptr) {
              throw std::runtime_error("failed to allocate additional memory");
          }

          std::memcpy(new_memory, memory, allocated_size);

          VirtualFree(new_memory, allocated_size, MEM_RELEASE);

          memory = new_memory;
          allocated_size = new_size;
      }

      std::memcpy(memory, code.data(), code.size());
      return reinterpret_cast<jit_function>(memory);
  }
#endif
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
