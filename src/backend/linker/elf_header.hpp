/*
 * this is only really meant to work for linux, please bare with me
 * ~nisan
 */

#pragma once
#include "../../util/color_defs.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace occult { // https://wiki.osdev.org/ELF_Tutorial
struct elf_header {
  unsigned char e_ident[16]; // Magic number and other info
  uint16_t e_type;           // Object file type
  uint16_t e_machine;        // Architecture
  uint32_t e_version;        // Object file version
  uint64_t e_entry;          // Entry point virtual address
  uint64_t e_phoff;          // Program header table file offset
  uint64_t e_shoff;          // Section header table file offset
  uint32_t e_flags;          // Processor-specific flags
  uint16_t e_ehsize;         // ELF header size in bytes
  uint16_t e_phentsize;      // Program header table entry size
  uint16_t e_phnum;          // Program header table entry count
  uint16_t e_shentsize;      // Section header table entry size
  uint16_t e_shnum;          // Section header table entry count
  uint16_t e_shstrndx;       // Section header string table index
};

struct elf_program_header {
  uint32_t p_type;   // Segment type
  uint32_t p_flags;  // Segment flags
  uint64_t p_offset; // Segment file offset
  uint64_t p_vaddr;  // Segment virtual address
  uint64_t p_paddr;  // Segment physical address
  uint64_t p_filesz; // Segment size in file
  uint64_t p_memsz;  // Segment size in memory
  uint64_t p_align;  // Segment alignment
};

struct elf {
  static elf_header generate_elf_header(std::uint64_t entry_addr);

  static elf_program_header generate_program_header(std::uint64_t program_size,
                                                    std::uint64_t vaddr,
                                                    std::uint64_t paddr,
                                                    std::uint64_t memsz);

  static void generate_binary(const std::string &binary_name,
                              const std::vector<std::uint8_t> &code,
                              std::uint64_t program_size,
                              std::uint64_t entry_addr = 0x400078,
                              std::uint64_t vaddr = 0x400000,
                              std::uint64_t paddr = 0x400000,
                              std::uint64_t memsz = 0x1000);
};
} // namespace occult
