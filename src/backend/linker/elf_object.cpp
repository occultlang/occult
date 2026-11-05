#include "elf_object.hpp"
#include <algorithm>
#include <cstring>
#include "elf_defs.hpp"
#include <fstream>
#include <iostream>
#include "../../util/color_defs.hpp"

namespace occult {
    namespace {
        struct strtab {
            std::vector<char> bytes{'\0'};
            std::uint32_t add(const std::string& s) {
                if (s.empty()) {
                    return 0;
                }
                const auto at = static_cast<std::uint32_t>(bytes.size());
                bytes.insert(bytes.end(), s.begin(), s.end());
                bytes.push_back('\0');
                return at;
            }
        };

        void pad_to(std::vector<std::uint8_t>& out, std::size_t align) {
            while (out.size() % align != 0) {
                out.push_back(0);
            }
        }

        template <typename T>
        void put(std::vector<std::uint8_t>& out, const T& v) {
            const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
            out.insert(out.end(), p, p + sizeof(T));
        }

        std::uint32_t elf_type(elf_object::kind k) {
            switch (k) {
            case elf_object::kind::pc32:
                return elfdef::R_X86_64_PC32;
            case elf_object::kind::plt32:
                return elfdef::R_X86_64_PLT32;
            default:
                return elfdef::R_X86_64_64;
            }
        }
    } // namespace

    void elf_object::write(const std::string& filename,
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
                           bool writable_text,
                           bool debug) {
        // ---------------------------------------------------------- .rodata
        // the same shape the linked blob uses: length in eight bytes, then
        // the content, then eight zeros, so the pointer reads as a C
        // const char *. sorted by their old address so two runs agree.
        std::vector<std::uint8_t> rodata;
        std::unordered_map<std::uint64_t, std::uint64_t> string_at;
        {
            std::vector<std::uint64_t> keys;
            keys.reserve(string_literals.size());
            for (const auto& [addr, _] : string_literals) {
                keys.push_back(addr);
            }
            std::sort(keys.begin(), keys.end());
            for (std::uint64_t addr : keys) {
                const std::string& content = string_literals.at(addr);
                const std::uint64_t len = content.size();
                put(rodata, len);
                string_at[addr] = rodata.size();
                rodata.insert(rodata.end(), content.begin(), content.end());
                rodata.insert(rodata.end(), 8, 0);
            }
        }

        const bool has_init = !init_fn.empty() && function_locs.count(init_fn) > 0;

        // --------------------------------------------------------- sections
        strtab shstr;
        std::uint32_t idx = 0;
        idx++; // the null section
        const std::uint32_t sec_text = idx++;
        const std::uint32_t sec_rela_text = idx++;
        const std::uint32_t sec_rodata = idx++;
        const std::uint32_t sec_data = idx++;
        const std::uint32_t sec_init = has_init ? idx++ : 0;
        const std::uint32_t sec_rela_init = has_init ? idx++ : 0;
        const std::uint32_t sec_note_stack = idx++;
        const std::uint32_t sec_symtab = idx++;
        const std::uint32_t sec_strtab = idx++;
        const std::uint32_t sec_shstrtab = idx++;
        const std::uint32_t section_count = idx;

        // ---------------------------------------------------------- symbols
        // locals first; sh_info says where the globals start
        strtab str;
        std::vector<elfdef::Elf64_Sym> syms;
        syms.push_back(elfdef::Elf64_Sym{}); // the null symbol

        auto section_symbol = [&](std::uint32_t shndx) {
            elfdef::Elf64_Sym s{};
            s.st_name = 0;
            s.st_info = elfdef::st_info(elfdef::STB_LOCAL, elfdef::STT_SECTION);
            s.st_shndx = static_cast<elfdef::Elf64_Section>(shndx);
            syms.push_back(s);
            return static_cast<std::uint32_t>(syms.size() - 1);
        };
        const std::uint32_t sym_text = section_symbol(sec_text);
        const std::uint32_t sym_rodata = section_symbol(sec_rodata);
        const std::uint32_t sym_data = section_symbol(sec_data);
        const std::uint32_t first_global = static_cast<std::uint32_t>(syms.size());

        // each function's size is the gap to the next, for nm and debuggers
        std::vector<std::pair<std::uint32_t, std::string>> by_offset;
        by_offset.reserve(function_locs.size());
        for (const auto& [name, loc] : function_locs) {
            by_offset.emplace_back(loc, name);
        }
        std::sort(by_offset.begin(), by_offset.end());

        std::unordered_map<std::string, std::uint32_t> sym_of;
        for (std::size_t i = 0; i < by_offset.size(); ++i) {
            const auto& [loc, name] = by_offset[i];
            const std::uint64_t end =
                (i + 1 < by_offset.size()) ? by_offset[i + 1].first : code.size();
            elfdef::Elf64_Sym s{};
            s.st_name = str.add(name);
            s.st_info = elfdef::st_info(elfdef::STB_GLOBAL, elfdef::STT_FUNC);
            s.st_shndx = static_cast<elfdef::Elf64_Section>(sec_text);
            s.st_value = loc;
            s.st_size = end > loc ? end - loc : 0;
            sym_of[name] = static_cast<std::uint32_t>(syms.size());
            syms.push_back(s);
        }
        std::vector<std::pair<std::int32_t, std::string>> globals_by_offset;
        globals_by_offset.reserve(globals.size());
        for (const auto& [name, off] : globals) {
            globals_by_offset.emplace_back(off, name);
        }
        std::sort(globals_by_offset.begin(), globals_by_offset.end());
        for (const auto& [off, name] : globals_by_offset) {
            elfdef::Elf64_Sym s{};
            s.st_name = str.add(name);
            s.st_info = elfdef::st_info(elfdef::STB_GLOBAL, elfdef::STT_OBJECT);
            s.st_shndx = static_cast<elfdef::Elf64_Section>(sec_data);
            s.st_value = static_cast<elfdef::Elf64_Addr>(off);
            s.st_size = 8;
            syms.push_back(s);
        }

        for (const auto& name : undefined) {
            elfdef::Elf64_Sym s{};
            s.st_name = str.add(name);
            s.st_info = elfdef::st_info(elfdef::STB_GLOBAL, elfdef::STT_NOTYPE);
            s.st_shndx = elfdef::SHN_UNDEF;
            sym_of[name] = static_cast<std::uint32_t>(syms.size());
            syms.push_back(s);
        }

        // ----------------------------------------------------- relocations
        std::vector<elfdef::Elf64_Rela> rela_text;
        auto add_rela = [&](std::uint64_t at, std::uint32_t sym, std::uint32_t type,
                            std::int64_t addend) {
            elfdef::Elf64_Rela r{};
            r.r_offset = at;
            r.r_info = elfdef::r_info(sym, type);
            r.r_addend = addend;
            rela_text.push_back(r);
        };

        for (const auto& r : relocs) {
            std::uint32_t sym = sym_text;
            if (!r.symbol.empty()) {
                auto it = sym_of.find(r.symbol);
                if (it == sym_of.end()) {
                    std::cerr << RED << "[OBJECT ERROR] no symbol for \"" << r.symbol << "\"\n"
                              << RESET;
                    return;
                }
                sym = it->second;
            }
            else if (r.section == against::rodata) {
                sym = sym_rodata;
            }
            else if (r.section == against::data) {
                sym = sym_data;
            }
            add_rela(r.at, sym, elf_type(r.k), r.addend);
        }
        // A string pointer is an absolute eight bytes into .rodata.
        for (const auto& [at, old_addr] : string_relocs) {
            auto it = string_at.find(old_addr);
            if (it == string_at.end()) {
                continue;
            }
            add_rela(at, sym_rodata, elfdef::R_X86_64_64, static_cast<std::int64_t>(it->second));
        }
        // A global's address likewise, into .data.
        for (const auto& [at, off] : global_relocs) {
            add_rela(at, sym_data, elfdef::R_X86_64_64, off);
        }

        // no entry stub of our own to call it from, so the C runtime does
        std::vector<elfdef::Elf64_Rela> rela_init;
        if (has_init) {
            elfdef::Elf64_Rela r{};
            r.r_offset = 0;
            r.r_info = elfdef::r_info(sym_of.at(init_fn), elfdef::R_X86_64_64);
            r.r_addend = 0;
            rela_init.push_back(r);
        }

        // -------------------------------------------------------- the file
        // a writable code section cannot be called .text: the default linker
        // script folds that name into the read-only segment. an orphan
        // section is placed by its flags instead.
        const std::uint32_t name_text = shstr.add(writable_text ? ".occtext" : ".text");
        const std::uint32_t name_rela_text =
            shstr.add(writable_text ? ".rela.occtext" : ".rela.text");
        const std::uint32_t name_rodata = shstr.add(".rodata");
        const std::uint32_t name_data = shstr.add(".data");
        const std::uint32_t name_init = has_init ? shstr.add(".init_array") : 0;
        const std::uint32_t name_rela_init = has_init ? shstr.add(".rela.init_array") : 0;
        const std::uint32_t name_note_stack = shstr.add(".note.GNU-stack");
        const std::uint32_t name_symtab = shstr.add(".symtab");
        const std::uint32_t name_strtab = shstr.add(".strtab");
        const std::uint32_t name_shstrtab = shstr.add(".shstrtab");

        std::vector<std::uint8_t> out(sizeof(elfdef::Elf64_Ehdr), 0);
        std::vector<elfdef::Elf64_Shdr> shdrs(section_count);

        auto place = [&](std::uint32_t sec, std::uint32_t name, std::uint32_t type,
                         std::uint64_t flags, std::size_t align, std::uint64_t entsize,
                         const void* data, std::size_t size) {
            pad_to(out, align);
            shdrs[sec].sh_name = name;
            shdrs[sec].sh_type = type;
            shdrs[sec].sh_flags = flags;
            shdrs[sec].sh_offset = out.size();
            shdrs[sec].sh_size = size;
            shdrs[sec].sh_addralign = align;
            shdrs[sec].sh_entsize = entsize;
            if (data != nullptr && size > 0) {
                const auto* p = static_cast<const std::uint8_t*>(data);
                out.insert(out.end(), p, p + size);
            }
        };

        const std::uint64_t text_flags =
            elfdef::SHF_ALLOC | elfdef::SHF_EXECINSTR | (writable_text ? elfdef::SHF_WRITE : 0u);
        place(sec_text, name_text, elfdef::SHT_PROGBITS, text_flags, 16, 0, code.data(), code.size());
        place(sec_rodata, name_rodata, elfdef::SHT_PROGBITS, elfdef::SHF_ALLOC, 8, 0, rodata.data(),
              rodata.size());
        // .data is zeroed rather than left to .bss, as the linked blob does
        {
            std::vector<std::uint8_t> zeros(static_cast<std::size_t>(globals_size), 0);
            place(sec_data, name_data, elfdef::SHT_PROGBITS, elfdef::SHF_ALLOC | elfdef::SHF_WRITE, 8, 0, zeros.data(),
                  zeros.size());
        }
        if (has_init) {
            const std::uint64_t slot = 0;
            place(sec_init, name_init, elfdef::SHT_INIT_ARRAY, elfdef::SHF_ALLOC | elfdef::SHF_WRITE, 8, 8, &slot,
                  sizeof(slot));
        }
        place(sec_rela_text, name_rela_text, elfdef::SHT_RELA, elfdef::SHF_INFO_LINK, 8, sizeof(elfdef::Elf64_Rela),
              rela_text.data(), rela_text.size() * sizeof(elfdef::Elf64_Rela));
        shdrs[sec_rela_text].sh_link = sec_symtab;
        shdrs[sec_rela_text].sh_info = sec_text;
        if (has_init) {
            place(sec_rela_init, name_rela_init, elfdef::SHT_RELA, elfdef::SHF_INFO_LINK, 8, sizeof(elfdef::Elf64_Rela),
                  rela_init.data(), rela_init.size() * sizeof(elfdef::Elf64_Rela));
            shdrs[sec_rela_init].sh_link = sec_symtab;
            shdrs[sec_rela_init].sh_info = sec_init;
        }
        // empty and unflagged: without the section ld assumes an exec stack
        place(sec_note_stack, name_note_stack, elfdef::SHT_PROGBITS, 0, 1, 0, nullptr, 0);
        place(sec_symtab, name_symtab, elfdef::SHT_SYMTAB, 0, 8, sizeof(elfdef::Elf64_Sym), syms.data(),
              syms.size() * sizeof(elfdef::Elf64_Sym));
        shdrs[sec_symtab].sh_link = sec_strtab;
        shdrs[sec_symtab].sh_info = first_global;
        place(sec_strtab, name_strtab, elfdef::SHT_STRTAB, 0, 1, 0, str.bytes.data(), str.bytes.size());
        place(sec_shstrtab, name_shstrtab, elfdef::SHT_STRTAB, 0, 1, 0, shstr.bytes.data(),
              shstr.bytes.size());

        pad_to(out, 8);
        const std::uint64_t shoff = out.size();
        for (const auto& sh : shdrs) {
            put(out, sh);
        }

        elfdef::Elf64_Ehdr eh{};
        eh.e_ident[elfdef::EI_MAG0] = elfdef::ELFMAG0;
        eh.e_ident[elfdef::EI_MAG1] = elfdef::ELFMAG1;
        eh.e_ident[elfdef::EI_MAG2] = elfdef::ELFMAG2;
        eh.e_ident[elfdef::EI_MAG3] = elfdef::ELFMAG3;
        eh.e_ident[elfdef::EI_CLASS] = elfdef::ELFCLASS64;
        eh.e_ident[elfdef::EI_DATA] = elfdef::ELFDATA2LSB;
        eh.e_ident[elfdef::EI_VERSION] = elfdef::EV_CURRENT;
        eh.e_ident[elfdef::EI_OSABI] = elfdef::ELFOSABI_SYSV;
        eh.e_type = elfdef::ET_REL;
        eh.e_machine = elfdef::EM_X86_64;
        eh.e_version = elfdef::EV_CURRENT;
        eh.e_shoff = shoff;
        eh.e_ehsize = sizeof(elfdef::Elf64_Ehdr);
        eh.e_shentsize = sizeof(elfdef::Elf64_Shdr);
        eh.e_shnum = static_cast<elfdef::Elf64_Half>(section_count);
        eh.e_shstrndx = static_cast<elfdef::Elf64_Half>(sec_shstrtab);
        std::memcpy(out.data(), &eh, sizeof(eh));

        std::ofstream f(filename, std::ios::binary);
        if (!f) {
            std::cerr << RED << "[OBJECT ERROR] cannot write " << filename << "\n" << RESET;
            return;
        }
        f.write(reinterpret_cast<const char*>(out.data()),
                static_cast<std::streamsize>(out.size()));

        if (debug) {
            std::cout << BLUE << "[OBJECT INFO] " << RESET << syms.size() - 1 << " symbol(s), "
                      << undefined.size() << " undefined, " << rela_text.size()
                      << " relocation(s)\n";
        }
        std::cout << GREEN << "[OCCULTC] Object file created: " << RESET << filename << " ("
                  << out.size() << " bytes, " << by_offset.size() << " function(s), "
                  << undefined.size() << " undefined)\n";
    }
} // namespace occult
