#include "linker.hpp"
#include <chrono>

namespace occult {
    void linker::link_and_create_binary(const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map, const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map,
                                        const std::unordered_map<std::uint64_t, std::string>& string_literals, bool debug, bool showtime) {
        auto start = std::chrono::high_resolution_clock::now();

        std::unordered_map<std::uint64_t, std::string> func_addr_map;

        for (const auto& pair : function_map) {
            // Copy address of functions to map the address itself to the name of the
            // function
            func_addr_map[reinterpret_cast<std::uint64_t>(pair.second)] = pair.first;
        }

        std::vector<std::uint8_t> final_code;
        std::unordered_map<std::string, std::uint64_t> locations;

        std::size_t offset = 0;
        for (const auto& m : function_raw_code_map) {
            locations[m.first] = offset; // Store the correct function start offset
            final_code.insert(final_code.end(), m.second.begin(), m.second.end());

            if (debug)
                std::cout << BLUE << "[LINKER INFO] Added function: " << YELLOW << "\"" << m.first << "\"" << RESET << std::endl;

            offset += m.second.size(); // Move offset forward
        }

        const std::uint64_t base_addr = 0x400000;
        const std::uint64_t header_size = sizeof(elf_header) + sizeof(elf_program_header);
        std::uint64_t entry_addr = base_addr + locations["main"] + header_size;

        if (debug) {
            std::cout << BLUE << "[LINKER INFO] Base address: " << RED << "0x" << std::hex << base_addr << std::dec << RESET << std::endl;
            std::cout << BLUE << "[LINKER INFO] Entry address: " << RED << "0x" << std::hex << entry_addr << std::dec << RESET << std::endl;
        }


        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map; // old JIT addr to final vaddr

        for (const auto& [old_host_addr, content] : string_literals) {
            std::uint64_t str_offset = final_code.size();

            // write length prefix (exactly as codegen does)
            std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) {
                final_code.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            }

            final_code.insert(final_code.end(), content.begin(), content.end());
            final_code.push_back(0);

            // pointer must point to DATA, not the length header
            std::uint64_t new_data_addr = base_addr + header_size + str_offset + 8;

            relocation_map[old_host_addr] = new_data_addr;

            if (debug) {
                std::cout << BLUE << "[LINKER INFO] Placed string \"" << content << "\" (len=" << len << ") -> data at 0x" << std::hex << new_data_addr << std::dec << RESET << std::endl;
            }
        }

        for (std::size_t i = 0; i < final_code.size() - 12; ++i) {
            uint8_t b1 = final_code[i];
            uint8_t b2 = final_code[i + 1];

            // Function call: 48 B8 xx xx xx xx xx xx xx xx 48 FF D0
            if (b1 == 0x48 && b2 == 0xB8 && i + 12 < final_code.size() && final_code[i + 10] == 0x48 && final_code[i + 11] == 0xFF && final_code[i + 12] == 0xD0) {

                std::uint64_t old_addr = 0;
                for (int j = 0; j < 8; ++j)
                    old_addr |= static_cast<std::uint64_t>(final_code[i + 2 + j]) << (j * 8);

                if (auto it = func_addr_map.find(old_addr); it != func_addr_map.end()) {
                    std::string name = it->second;
                    std::uint64_t new_addr = base_addr + header_size + locations[name];

                    for (int j = 0; j < 8; ++j)
                        final_code[i + 2 + j] = (new_addr >> (j * 8)) & 0xFF;

                    if (debug) {
                        std::cout << BLUE << "[LINKER INFO] Patched call to \"" << name << "\" → 0x" << std::hex << new_addr << std::dec << RESET << "\n";
                    }
                }
                continue;
            }

            // Any data pointer: 48 Bx xx xx xx xx xx xx xx xx  (mov r64, imm64)
            if (b1 == 0x48 && b2 >= 0xB8 && b2 <= 0xBF) {
                std::uint64_t old_addr = 0;
                for (int j = 0; j < 8; ++j)
                    old_addr |= static_cast<std::uint64_t>(final_code[i + 2 + j]) << (j * 8);

                if (auto it = relocation_map.find(old_addr); it != relocation_map.end()) {
                    std::uint64_t new_addr = it->second;
                    for (int j = 0; j < 8; ++j)
                        final_code[i + 2 + j] = (new_addr >> (j * 8)) & 0xFF;

                    if (debug)
                        std::cout << BLUE << "[LINKER INFO] Patched string pointer -> 0x" << std::hex << new_addr << std::dec << RESET << "\n";
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;

        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking functions \033[0m" << duration.count() << "ms\n";
        }

        // Generate the final binary
        elf::generate_binary(binary_name, final_code, entry_addr);
    }
} // namespace occult
