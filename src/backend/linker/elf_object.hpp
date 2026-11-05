#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace occult {
    struct elf_object {
        // mirrors x86_64::codegen_v2::reloc_kind / reloc_against
        enum class kind { pc32, plt32, abs64 };
        enum class against { text, rodata, data };

        struct reloc {
            std::string symbol; // empty => against the section below
            against section;
            std::size_t at; // offset in .text of the field to fill in
            kind k;
            std::int64_t addend;
        };

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
                          bool debug = false);
    };
} // namespace occult
