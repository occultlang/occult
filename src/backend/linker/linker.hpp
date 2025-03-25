#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "../writer.hpp"

namespace occult {
    struct relocation_entry {
        std::string symbol;
        std::size_t patch_location;
        std::size_t instruction_length;
    };

    class linker {
    public:
        std::vector<std::uint8_t> link(const std::vector<std::vector<std::uint8_t>>& code_fragments,
                                       const std::unordered_map<std::string, jit_function>& symbol_table,
                                       const std::vector<relocation_entry>& relocations);

        void create_elf_binary(const std::string& binary_name, const std::vector<std::uint8_t>& final_code);
    };
};
