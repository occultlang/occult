#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "elf_object.hpp"

namespace occult {
    struct coff_object {
        // the same relocation vocabulary the code generator already speaks
        using kind = elf_object::kind;
        using against = elf_object::against;
        using reloc = elf_object::reloc;

        static void write(const std::string& filename,
                          const std::vector<std::uint8_t>& code,
                          const std::unordered_map<std::string, std::uint32_t>& function_locs,
                          const std::vector<std::string>& undefined,
                          const std::vector<reloc>& relocs,
                          const std::unordered_map<std::uint64_t, std::string>& string_literals,
                          const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs,
                          const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs,
                          const std::unordered_map<std::string, std::int32_t>& globals,
                          std::int32_t globals_size,
                          const std::string& init_fn,
                          bool writable_text = false,
                          bool debug = false,
                          bool defaultlibs = true,
                          const std::vector<std::string>& libraries = {});
    };
} // namespace occult
