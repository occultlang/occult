#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <sstream>
#include <cstdint>

#ifdef __linux__ // only supports linux for now
#include <unistd.h>
#include <sys/mman.h>
#endif
#ifdef _WIN64
#include <Windows.h>
#endif

namespace occult {
  class x86_writer {
    std::vector<std::uint8_t> code;
    void* memory = nullptr;
    std::size_t page_size;
    std::size_t size;
  public:
#ifdef __linux__
    x86_writer(const std::size_t& size) : page_size(sysconf(_SC_PAGE_SIZE)), size(((size + page_size - 1) / page_size) * page_size) {
      memory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      
      if (memory == MAP_FAILED) {
        throw std::runtime_error("memory allocation failed");
      }
    }
    
    ~x86_writer() {
      if (memory) {
        munmap(memory, size);
      }
    }
#endif
#ifdef _WIN64 
    x86_writer(const std::size_t& size) {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        page_size = sys_info.dwPageSize;
        this->size = ((size + page_size - 1) / page_size) * page_size;

        DWORD old_protect = 0;
        memory = VirtualAlloc(nullptr, size, PAGE_EXECUTE_READWRITE | MEM_COMMIT | MEM_RESERVE, old_protect);

        if (!VirtualProtect(memory, size, PAGE_READWRITE, &old_protect)) {
            throw std::runtime_error("memory allocation failed");
        }
    }

    ~x86_writer() {
        if (memory) {
            VirtualFree(memory, 0, MEM_RELEASE);
        }
    }
#endif
    void push_byte(const std::uint8_t& byte) {
      code.emplace_back(byte);
    }
    
    void push_bytes(const std::initializer_list<std::uint8_t>& bytes) {
      code.insert(code.end(), bytes);
    }
    
    void push_bytes(const std::vector<std::uint8_t> bytes) {
      code.insert(code.end(), bytes.begin(), bytes.end());
    }
    
    const std::vector<std::uint8_t> string_to_bytes(const std::string& str) {
      std::vector<std::uint8_t> vectorized_data;
      vectorized_data.reserve(str.size()); // might be redundant
      
      for (const auto& byte : str) {
        vectorized_data.emplace_back(static_cast<std::uint8_t>(byte));
      }
      
      return vectorized_data;
    }
    
    using jit_function = void(*)();
    
    jit_function setup_function() {
      std::memcpy(memory, code.data(), code.size());
      
      return reinterpret_cast<jit_function>(memory);
    }
    
    const std::vector<std::uint8_t>& get_code() {
      return code;
    }
  };
} // namespace occult
