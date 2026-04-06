#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "../codegen/x86_64_writer.hpp"
#include "elf_header.hpp"

namespace occult {

    class linker {
    public:
        static void link_and_create_binary(const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map, const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map,
                                           const std::unordered_map<std::uint64_t, std::string>& string_literals, bool debug = false, bool showtime = false);
    };

} // namespace occult
