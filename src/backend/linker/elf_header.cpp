#include "elf_header.hpp"
#include <cstdint>

namespace occult {
    elf_header elf::generate_elf_header(std::uint64_t entry_addr) {
        // make a macro for different architectures later please
        elf_header elf_header = {};
        // std::memset(elf_header.e_ident, 0, sizeof(elf_header.e_ident));
        std::memcpy(elf_header.e_ident,
                    "\x7f"
                    "ELF",
                    4);
        elf_header.e_ident[4] = 2; // 64bit
        elf_header.e_ident[5] = 1; // little-endian
        elf_header.e_ident[6] = 1; // original elf version

        elf_header.e_type = 2;                               // exe
        elf_header.e_machine = 0x3E;                         // x86_64
        elf_header.e_version = 1;                            // original version
        elf_header.e_entry = entry_addr;                     // entry point address
        elf_header.e_phoff = sizeof(elf_header);             // program header offset
        elf_header.e_ehsize = sizeof(elf_header);            // elf header size
        elf_header.e_phentsize = sizeof(elf_program_header); // program header size
        elf_header.e_phnum = 1;                              // number of program headers

        return elf_header;
    }

    elf_program_header elf::generate_program_header(std::uint64_t filesz, std::uint64_t vaddr, std::uint64_t paddr, std::uint64_t memsz) {
        // make a macro later for different architecture
        elf_program_header program_header = {};
        program_header.p_type = 1;                       // Loadable segment
        program_header.p_flags = 5;                      // Read + Execute
        program_header.p_offset = 0;                     // Offset in file
        program_header.p_vaddr = vaddr;                  // Virtual address in memory
        program_header.p_paddr = paddr;                  // Physical address
        program_header.p_filesz = filesz;                // File size, TODO: add argument for file size in the future
        program_header.p_memsz = memsz ? memsz : filesz; // Memory size
        program_header.p_align = 0x1000;                 // Alignment

        return program_header;
    }

    void elf::generate_binary(const std::string& binary_name, const std::vector<std::uint8_t>& code, std::uint64_t entry_addr, std::uint64_t vaddr, std::uint64_t paddr, std::uint64_t memsz) {
        if (code.empty()) {
            std::cerr << RED << "[OCCULTC] No code to write\n" << RESET;
            return;
        }

        const std::size_t header_size = sizeof(elf_header) + sizeof(elf_program_header);
        const std::uint64_t filesz = header_size + code.size(); // exact bytes that will be in the file

        // if caller didn't specify a larger memsz (e.g. for future .bss), use filesz
        if (memsz == 0 || memsz < filesz) {
            memsz = filesz;
        }

        std::ofstream file(binary_name, std::ios::binary);
        if (!file) {
            std::cerr << RED << "[OCCULTC] Failed to create binary\n" << RESET;
            return;
        }

        elf_header elf_h = generate_elf_header(entry_addr);
        elf_program_header ph = generate_program_header(filesz, vaddr, paddr, memsz);

        file.write(reinterpret_cast<const char*>(&elf_h), sizeof(elf_h));
        file.write(reinterpret_cast<const char*>(&ph), sizeof(ph));
        file.write(reinterpret_cast<const char*>(code.data()), code.size());

        file.close();
        std::cout << GREEN << "[OCCULTC] ELF binary created: " << binary_name << " (" << filesz << " bytes, full code included)\n" << RESET;
    }
}; // namespace occult
