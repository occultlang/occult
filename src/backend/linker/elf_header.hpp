/*
 * this is only really meant to work for linux, please bare with me
 * ~nisan
 */
#pragma once
#include <cstdint>
#include <cstring>
#include "elf_defs.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "../../util/color_defs.hpp"

namespace occult { // https://wiki.osdev.org/ELF_Tutorial
    using elf_header = elfdef::Elf64_Ehdr;
    using elf_program_header = elfdef::Elf64_Phdr;
    using elf_section_header = elfdef::Elf64_Shdr;

    struct elf {
        static elf_header generate_elf_header(std::uint64_t entry_addr);

        static elf_program_header generate_program_header(std::uint64_t filesz, std::uint64_t vaddr, std::uint64_t paddr, std::uint64_t memsz = 0);

        static elf_section_header generate_section_header(std::uint32_t sh_name, std::uint32_t sh_type, std::uint64_t sh_flags, std::uint64_t sh_addr, std::uint64_t sh_offset, std::uint64_t sh_size, std::uint32_t sh_link = 0,
                                                          std::uint32_t sh_info = 0, std::uint64_t sh_addralign = 1, std::uint64_t sh_entsize = 0);

        // the loader every dynamically linked x86-64 Linux program names
        static constexpr const char* interpreter = "/lib64/ld-linux-x86-64.so.2";

        static std::size_t dynamic_header_size();

        static void generate_dynamic_binary(const std::string& binary_name,
                                            std::vector<std::uint8_t> image,
                                            std::uint64_t vaddr,
                                            std::size_t header_size,
                                            std::size_t text_size,
                                            const std::vector<std::string>& needed,
                                            const std::vector<std::pair<std::string, std::size_t>>& slot_uses);

        static std::size_t shared_header_size();
        static void generate_shared_object(const std::string& binary_name,
                                           std::vector<std::uint8_t> image,
                                           std::size_t header_size,
                                           std::size_t text_size,
                                           const std::string& soname,
                                           const std::vector<std::pair<std::string, std::size_t>>& exports,
                                           const std::vector<std::string>& needed,
                                           const std::vector<std::pair<std::string, std::size_t>>& slot_uses,
                                           const std::vector<std::size_t>& abs_relocs);

        static void generate_binary(const std::string& binary_name, const std::vector<std::uint8_t>& code, std::uint64_t entry_addr = 0x400078, std::uint64_t vaddr = 0x400000, std::uint64_t paddr = 0x400000, std::uint64_t memsz = 0,
                                    std::size_t text_size = 0);
    };
} // namespace occult
