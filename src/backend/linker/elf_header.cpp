#include "elf_header.hpp"
#include <cstdint>
#include <elf.h>

namespace occult {

elf_header elf::generate_elf_header(std::uint64_t entry_addr) {
  elf_header h = {};

  std::memcpy(h.e_ident, ELFMAG, SELFMAG);
  h.e_ident[EI_CLASS] = ELFCLASS64;
  h.e_ident[EI_DATA] = ELFDATA2LSB;
  h.e_ident[EI_VERSION] = EV_CURRENT;

  h.e_type = ET_EXEC;
  h.e_machine = EM_X86_64;
  h.e_version = EV_CURRENT;
  h.e_entry = entry_addr;
  h.e_phoff = sizeof(elf_header);
  h.e_ehsize = sizeof(elf_header);
  h.e_phentsize = sizeof(elf_program_header);
  h.e_phnum = 1;
  h.e_shentsize = sizeof(elf_section_header);

  return h;
}

elf_program_header elf::generate_program_header(std::uint64_t filesz,
                                                std::uint64_t vaddr,
                                                std::uint64_t paddr,
                                                std::uint64_t memsz) {
  elf_program_header ph = {};
  ph.p_type = PT_LOAD;
  ph.p_flags = PF_R | PF_X;
  ph.p_offset = 0;
  ph.p_vaddr = vaddr;
  ph.p_paddr = paddr;
  ph.p_filesz = filesz;
  ph.p_memsz = memsz ? memsz : filesz;
  ph.p_align = 0x1000;

  return ph;
}

void elf::generate_binary(const std::string &binary_name,
                          const std::vector<std::uint8_t> &code,
                          std::uint64_t entry_addr, std::uint64_t vaddr,
                          std::uint64_t paddr, std::uint64_t memsz,
                          std::size_t text_size) {
  if (code.empty()) {
    std::cerr << RED << "[OCCULTC] No code to write\n" << RESET;
    return;
  }

  if (text_size == 0 || text_size > code.size())
    text_size = code.size();

  const std::size_t data_size = code.size() - text_size;
  const std::size_t header_size =
      sizeof(elf_header) + sizeof(elf_program_header);
  const std::uint64_t filesz = header_size + code.size();

  if (memsz == 0 || memsz < filesz)
    memsz = filesz;

  std::vector<std::uint8_t> shstrtab;
  shstrtab.push_back(0);

  const std::uint32_t text_name_off =
      static_cast<std::uint32_t>(shstrtab.size());
  for (char c : std::string_view(".text"))
    shstrtab.push_back(static_cast<std::uint8_t>(c));
  shstrtab.push_back(0);

  std::uint32_t rodata_name_off = 0;
  if (data_size > 0) {
    rodata_name_off = static_cast<std::uint32_t>(shstrtab.size());
    for (char c : std::string_view(".rodata"))
      shstrtab.push_back(static_cast<std::uint8_t>(c));
    shstrtab.push_back(0);
  }

  const std::uint32_t shstrtab_name_off =
      static_cast<std::uint32_t>(shstrtab.size());
  for (char c : std::string_view(".shstrtab"))
    shstrtab.push_back(static_cast<std::uint8_t>(c));
  shstrtab.push_back(0);

  const std::uint16_t shnum = static_cast<std::uint16_t>(data_size > 0 ? 4 : 3);
  const std::uint16_t shstrndx = static_cast<std::uint16_t>(shnum - 1);

  const std::size_t shstrtab_off = filesz;
  const std::size_t shtab_off_raw = shstrtab_off + shstrtab.size();
  const std::size_t shtab_off =
      (shtab_off_raw + 7) & ~static_cast<std::size_t>(7);
  const std::size_t padding_size = shtab_off - shtab_off_raw;

  std::vector<elf_section_header> sections(shnum, elf_section_header{});

  sections[1].sh_name = text_name_off;
  sections[1].sh_type = SHT_PROGBITS;
  sections[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
  sections[1].sh_addr = vaddr + header_size;
  sections[1].sh_offset = header_size;
  sections[1].sh_size = text_size;
  sections[1].sh_addralign = 16;

  if (data_size > 0) {
    sections[2].sh_name = rodata_name_off;
    sections[2].sh_type = SHT_PROGBITS;
    sections[2].sh_flags = SHF_ALLOC;
    sections[2].sh_addr = vaddr + header_size + text_size;
    sections[2].sh_offset = header_size + text_size;
    sections[2].sh_size = data_size;
    sections[2].sh_addralign = 1;
  }

  sections[shstrndx].sh_name = shstrtab_name_off;
  sections[shstrndx].sh_type = SHT_STRTAB;
  sections[shstrndx].sh_offset = shstrtab_off;
  sections[shstrndx].sh_size = shstrtab.size();
  sections[shstrndx].sh_addralign = 1;

  elf_header elf_h = generate_elf_header(entry_addr);
  elf_h.e_shoff = shtab_off;
  elf_h.e_shnum = shnum;
  elf_h.e_shstrndx = shstrndx;

  elf_program_header ph = generate_program_header(filesz, vaddr, paddr, memsz);

  std::ofstream file(binary_name, std::ios::binary);
  if (!file) {
    std::cerr << RED << "[OCCULTC] Failed to create binary\n" << RESET;
    return;
  }

  file.write(reinterpret_cast<const char *>(&elf_h), sizeof(elf_h));
  file.write(reinterpret_cast<const char *>(&ph), sizeof(ph));
  file.write(reinterpret_cast<const char *>(code.data()), code.size());
  file.write(reinterpret_cast<const char *>(shstrtab.data()), shstrtab.size());

  if (padding_size > 0) {
    static constexpr std::uint8_t zeros[8] = {};
    file.write(reinterpret_cast<const char *>(zeros),
               static_cast<std::streamsize>(padding_size));
  }

  for (const auto &sh : sections)
    file.write(reinterpret_cast<const char *>(&sh), sizeof(sh));

  file.close();

  const std::size_t total_size = shtab_off + shnum * sizeof(elf_section_header);
  std::cout << GREEN << "[OCCULTC] ELF binary created: " << binary_name << " ("
            << total_size << " bytes, " << shnum << " sections)\n"
            << RESET;
}

} // namespace occult
