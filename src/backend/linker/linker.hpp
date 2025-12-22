#pragma once
#include "../codegen/x86_64_codegen.hpp"
#include "elf_header.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace occult {
class linker {
public:
    static void link_and_create_binary(
        const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map,
        const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map,
        bool debug = false, bool showtime = false);
};
} // namespace occult
