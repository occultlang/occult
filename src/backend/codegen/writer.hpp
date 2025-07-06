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
  using jit_function = std::int64_t(*)();
  
  class writer {
    std::vector<std::uint8_t> code;
    std::size_t page_size;
    size_t allocated_size;
    std::unordered_map<std::string, std::size_t> string_locations;
  public:
    void* memory = nullptr;
    
#ifdef __linux__
    writer() : page_size(sysconf(_SC_PAGE_SIZE)), allocated_size(page_size) {
      memory = mmap(nullptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      
      if (memory == MAP_FAILED) {
        throw std::runtime_error("Memory allocation failed");
      }
    }
    
    ~writer() {
      if (memory) {
        munmap(memory, allocated_size); 
      }
    }
#endif
#ifdef _WIN64 
    writer()
        : page_size([] {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
            }()), allocated_size(page_size) {

        // Equivalent to mmap with PROT_READ | PROT_WRITE | PROT_EXEC
        memory = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

        if (!memory) {
            throw std::runtime_error("Memory allocation failed");
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
