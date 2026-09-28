#include "linker.hpp"
#include <chrono>

namespace occult {
    void linker::link_blob(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                           const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                           const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs, std::int32_t globals_size,
                           bool debug, bool showtime) {
        auto start = std::chrono::high_resolution_clock::now();

        auto main_it = function_locs.find("main");
        if (main_it == function_locs.end()) {
            std::cerr << RED << "[LINKER ERROR] No 'main' function found\n" << RESET;
            return;
        }

        constexpr std::uint64_t base_addr = 0x400000;
        constexpr std::uint64_t header_size = sizeof(elf_header) + sizeof(elf_program_header);
        const std::uint64_t entry_addr = base_addr + header_size;

        auto init_it = function_locs.find("__global_init");
        const bool has_init = (init_it != function_locs.end());
        const std::size_t stub_size = has_init ? 20 : 15;

        std::vector<std::uint8_t> final_code;
        final_code.reserve(stub_size + code.size());

        if (has_init) {
            const std::int32_t rel_init = static_cast<std::int32_t>(stub_size + init_it->second) - 5;
            final_code.push_back(0xE8);
            for (int j = 0; j < 4; ++j) final_code.push_back(static_cast<std::uint8_t>((rel_init >> (j * 8)) & 0xFF));
        }
        const std::int32_t rel = static_cast<std::int32_t>(stub_size + main_it->second) - static_cast<std::int32_t>(final_code.size() + 5);
        final_code.push_back(0xE8);
        for (int j = 0; j < 4; ++j) final_code.push_back(static_cast<std::uint8_t>((rel >> (j * 8)) & 0xFF));
        final_code.insert(final_code.end(), {0x48, 0x89, 0xC7, 0xB8, 0x3C, 0x00, 0x00, 0x00, 0x0F, 0x05});

        final_code.insert(final_code.end(), code.begin(), code.end());
        const std::size_t text_size = final_code.size();

        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;
        for (const auto& [old_addr, content] : string_literals) {
            const std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) final_code.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            const std::uint64_t data_off = final_code.size();
            final_code.insert(final_code.end(), content.begin(), content.end());
            final_code.insert(final_code.end(), 8, 0);
            relocation_map[old_addr] = base_addr + header_size + data_off;
        }

        for (const auto& [code_off, old_addr] : string_relocs) {
            auto it = relocation_map.find(old_addr);
            if (it == relocation_map.end()) {
                continue;
            }
            const std::size_t at = stub_size + code_off;
            for (int j = 0; j < 8; ++j) final_code[at + j] = static_cast<std::uint8_t>((it->second >> (j * 8)) & 0xFF);

            if (debug) {
                std::cout << BLUE << "[LINKER INFO] Patched string pointer @0x" << std::hex << at << " -> 0x" << it->second << std::dec << RESET << "\n";
            }
        }

        if (globals_size > 0) {
            const std::uint64_t globals_base = base_addr + header_size + final_code.size();
            final_code.insert(final_code.end(), static_cast<std::size_t>(globals_size), 0);

            for (const auto& [code_off, global_off] : global_relocs) {
                const std::uint64_t addr = globals_base + static_cast<std::uint64_t>(global_off);
                const std::size_t at = stub_size + code_off;
                for (int j = 0; j < 8; ++j) final_code[at + j] = static_cast<std::uint8_t>((addr >> (j * 8)) & 0xFF);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking \033[0m" << duration.count() << "ms\n";
        }

        elf::generate_binary(binary_name, final_code, entry_addr, base_addr, base_addr, 0, text_size);
    }
} // namespace occult
