#include "linker.hpp"
#include "../elf_header.hpp"

namespace occult {
    std::vector<std::uint8_t> linker::link(
        const std::vector<std::vector<std::uint8_t>>& code_fragments,
        const std::unordered_map<std::string, jit_function>& symbol_table,
        const std::vector<relocation_entry>& relocations) {
            std::vector<std::uint8_t> final_code;

            for (const auto& fragment : code_fragments) {
                final_code.insert(final_code.end(), fragment.begin(), fragment.end());
            }

            for (const auto& reloc : relocations) {
                if (symbol_table.find(reloc.symbol) == symbol_table.end()) {
                    throw std::runtime_error("Undefined symbol: " + reloc.symbol);
                }

                std::size_t target_address = (std::size_t)symbol_table.at(reloc.symbol);
                std::size_t instruction_end = reloc.patch_location + reloc.instruction_length;
                std::int32_t offset = static_cast<std::int32_t>(target_address) - static_cast<std::int32_t>(instruction_end);

                std::memcpy(final_code.data() + reloc.patch_location, &offset, sizeof(offset));
            }

            return final_code;
    }

    void linker::create_elf_binary(const std::string& binary_name, const std::vector<uint8_t> &final_code) {
        elf::generate_binary(binary_name, final_code.size(), final_code);
    }
};
