#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <iomanip>

#ifdef __linux__ 
#include <unistd.h>
#include <sys/mman.h>
#endif
#ifdef _WIN64
#include <Windows.h>
#endif

namespace occult {
  using jit_function = int(*)();
  
  class writer {
    std::vector<std::uint8_t> code;
    void* memory = nullptr;
    std::size_t page_size;
    std::size_t size;
    std::unordered_map<std::string, std::size_t> string_locations;
  public:
#ifdef __linux__
    writer(const std::size_t& size) : page_size(sysconf(_SC_PAGE_SIZE)), size(((size + page_size - 1) / page_size) * page_size) {
      memory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      
      if (memory == MAP_FAILED) {
        throw std::runtime_error("memory allocation failed");
      }
    }
    
    ~writer() {
      if (memory) {
        munmap(memory, size);
      }
    }
#endif
#ifdef _WIN64 
    writer(const std::size_t& size) {
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

    ~writer() {
      if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
      }
    }
#endif
    void push_byte(const std::uint8_t& byte);
    void push_bytes(const std::initializer_list<std::uint8_t>& bytes);
    void push_bytes(const std::vector<std::uint8_t> bytes);
    const std::vector<std::uint8_t> string_to_bytes(const std::string& str);
    std::size_t push_string(const std::string &str);
    
    jit_function setup_function();
    
    std::vector<std::uint8_t>& get_code();
    void print_bytes();
    const std::size_t& get_string_location(const std::string& str);
  };
} // namespace occult
