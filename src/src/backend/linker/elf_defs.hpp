#pragma once
#include <cstdint>

namespace occult::elfdef {
    using Elf64_Half = std::uint16_t;
    using Elf64_Word = std::uint32_t;
    using Elf64_Xword = std::uint64_t;
    using Elf64_Sxword = std::int64_t;
    using Elf64_Addr = std::uint64_t;
    using Elf64_Off = std::uint64_t;
    using Elf64_Section = std::uint16_t;

    struct Elf64_Ehdr {
        unsigned char e_ident[16];
        Elf64_Half e_type;
        Elf64_Half e_machine;
        Elf64_Word e_version;
        Elf64_Addr e_entry;
        Elf64_Off e_phoff;
        Elf64_Off e_shoff;
        Elf64_Word e_flags;
        Elf64_Half e_ehsize;
        Elf64_Half e_phentsize;
        Elf64_Half e_phnum;
        Elf64_Half e_shentsize;
        Elf64_Half e_shnum;
        Elf64_Half e_shstrndx;
    };

    struct Elf64_Shdr {
        Elf64_Word sh_name;
        Elf64_Word sh_type;
        Elf64_Xword sh_flags;
        Elf64_Addr sh_addr;
        Elf64_Off sh_offset;
        Elf64_Xword sh_size;
        Elf64_Word sh_link;
        Elf64_Word sh_info;
        Elf64_Xword sh_addralign;
        Elf64_Xword sh_entsize;
    };

    struct Elf64_Phdr {
        Elf64_Word p_type;
        Elf64_Word p_flags;
        Elf64_Off p_offset;
        Elf64_Addr p_vaddr;
        Elf64_Addr p_paddr;
        Elf64_Xword p_filesz;
        Elf64_Xword p_memsz;
        Elf64_Xword p_align;
    };

    struct Elf64_Sym {
        Elf64_Word st_name;
        unsigned char st_info;
        unsigned char st_other;
        Elf64_Section st_shndx;
        Elf64_Addr st_value;
        Elf64_Xword st_size;
    };

    struct Elf64_Rela {
        Elf64_Addr r_offset;
        Elf64_Xword r_info;
        Elf64_Sxword r_addend;
    };

    struct Elf64_Dyn {
        Elf64_Sxword d_tag;
        Elf64_Xword d_val; // d_ptr too: the two share the word
    };

    static_assert(sizeof(Elf64_Ehdr) == 64);
    static_assert(sizeof(Elf64_Shdr) == 64);
    static_assert(sizeof(Elf64_Phdr) == 56);
    static_assert(sizeof(Elf64_Sym) == 24);
    static_assert(sizeof(Elf64_Rela) == 24);
    static_assert(sizeof(Elf64_Dyn) == 16);

    constexpr unsigned char ELFMAG0 = 0x7f;
    constexpr unsigned char ELFMAG1 = 'E';
    constexpr unsigned char ELFMAG2 = 'L';
    constexpr unsigned char ELFMAG3 = 'F';
    constexpr const char* ELFMAG = "\177ELF";
    constexpr int SELFMAG = 4;

    constexpr int EI_MAG0 = 0;
    constexpr int EI_MAG1 = 1;
    constexpr int EI_MAG2 = 2;
    constexpr int EI_MAG3 = 3;
    constexpr int EI_CLASS = 4;
    constexpr int EI_DATA = 5;
    constexpr int EI_VERSION = 6;
    constexpr int EI_OSABI = 7;

    constexpr unsigned char ELFCLASS64 = 2;
    constexpr unsigned char ELFDATA2LSB = 1;
    constexpr unsigned char ELFOSABI_SYSV = 0;
    constexpr Elf64_Word EV_CURRENT = 1;

    constexpr Elf64_Half ET_REL = 1;
    constexpr Elf64_Half ET_EXEC = 2;
    constexpr Elf64_Half ET_DYN = 3;
    constexpr Elf64_Half EM_X86_64 = 62;

    constexpr Elf64_Word SHT_PROGBITS = 1;
    constexpr Elf64_Word SHT_SYMTAB = 2;
    constexpr Elf64_Word SHT_STRTAB = 3;
    constexpr Elf64_Word SHT_RELA = 4;
    constexpr Elf64_Word SHT_HASH = 5;
    constexpr Elf64_Word SHT_DYNAMIC = 6;
    constexpr Elf64_Word SHT_DYNSYM = 11;
    constexpr Elf64_Word SHT_INIT_ARRAY = 14;

    constexpr Elf64_Xword SHF_WRITE = 0x1;
    constexpr Elf64_Xword SHF_ALLOC = 0x2;
    constexpr Elf64_Xword SHF_EXECINSTR = 0x4;
    constexpr Elf64_Xword SHF_INFO_LINK = 0x40;

    constexpr Elf64_Section SHN_UNDEF = 0;

    constexpr unsigned char STB_LOCAL = 0;
    constexpr unsigned char STB_GLOBAL = 1;
    constexpr unsigned char STT_NOTYPE = 0;
    constexpr unsigned char STT_OBJECT = 1;
    constexpr unsigned char STT_FUNC = 2;
    constexpr unsigned char STT_SECTION = 3;

    constexpr Elf64_Word R_X86_64_64 = 1;
    constexpr Elf64_Word R_X86_64_PC32 = 2;
    constexpr Elf64_Word R_X86_64_PLT32 = 4;
    constexpr Elf64_Word R_X86_64_GLOB_DAT = 6;
    constexpr Elf64_Word R_X86_64_RELATIVE = 8;

    constexpr Elf64_Word PT_LOAD = 1;
    constexpr Elf64_Word PT_DYNAMIC = 2;
    constexpr Elf64_Word PT_INTERP = 3;
    constexpr Elf64_Word PT_PHDR = 6;
    constexpr Elf64_Word PT_GNU_STACK = 0x6474e551;

    constexpr Elf64_Sxword DT_NULL = 0;
    constexpr Elf64_Sxword DT_NEEDED = 1;
    constexpr Elf64_Sxword DT_HASH = 4;
    constexpr Elf64_Sxword DT_STRTAB = 5;
    constexpr Elf64_Sxword DT_SYMTAB = 6;
    constexpr Elf64_Sxword DT_RELA = 7;
    constexpr Elf64_Sxword DT_RELASZ = 8;
    constexpr Elf64_Sxword DT_RELAENT = 9;
    constexpr Elf64_Sxword DT_STRSZ = 10;
    constexpr Elf64_Sxword DT_SYMENT = 11;
    constexpr Elf64_Sxword DT_SONAME = 14;
    constexpr Elf64_Sxword DT_DEBUG = 21;
    constexpr Elf64_Word PF_X = 1;
    constexpr Elf64_Word PF_W = 2;
    constexpr Elf64_Word PF_R = 4;

    constexpr unsigned char st_info(unsigned char bind, unsigned char type) {
        return static_cast<unsigned char>((bind << 4) | (type & 0xf));
    }

    constexpr Elf64_Xword r_info(Elf64_Word sym, Elf64_Word type) {
        return (static_cast<Elf64_Xword>(sym) << 32) | type;
    }
} // namespace occult::elfdef
