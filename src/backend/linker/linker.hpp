#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include "elf_header.hpp"
#include "../codegen/jit.hpp"
/*
namespace occult {
  class linker {
  public:
    static void link_and_create_binary(const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map,
                                       const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map, bool debug = false, bool showtime = false);
  };
}
*/