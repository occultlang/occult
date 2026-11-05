#include "elf_header.hpp"
#include <algorithm>
#include <string_view>
#include <cstdint>
#include "elf_defs.hpp"

namespace occult {

    elf_header elf::generate_elf_header(std::uint64_t entry_addr) {
        elf_header h = {};

        std::memcpy(h.e_ident, elfdef::ELFMAG, elfdef::SELFMAG);
        h.e_ident[elfdef::EI_CLASS] = elfdef::ELFCLASS64;
        h.e_ident[elfdef::EI_DATA] = elfdef::ELFDATA2LSB;
        h.e_ident[elfdef::EI_VERSION] = elfdef::EV_CURRENT;

        h.e_type = elfdef::ET_EXEC;
        h.e_machine = elfdef::EM_X86_64;
        h.e_version = elfdef::EV_CURRENT;
        h.e_entry = entry_addr;
        h.e_phoff = sizeof(elf_header);
        h.e_ehsize = sizeof(elf_header);
        h.e_phentsize = sizeof(elf_program_header);
        h.e_phnum = 1;
        h.e_shentsize = sizeof(elf_section_header);

        return h;
    }

    elf_program_header elf::generate_program_header(std::uint64_t filesz, std::uint64_t vaddr, std::uint64_t paddr, std::uint64_t memsz) {
        elf_program_header ph = {};
        ph.p_type = elfdef::PT_LOAD;
        ph.p_flags = elfdef::PF_R | elfdef::PF_W | elfdef::PF_X; // W so globals (in this segment) are writable
        ph.p_offset = 0;
        ph.p_vaddr = vaddr;
        ph.p_paddr = paddr;
        ph.p_filesz = filesz;
        ph.p_memsz = memsz ? memsz : filesz;
        ph.p_align = 0x1000;

        return ph;
    }

    void elf::generate_binary(const std::string& binary_name, const std::vector<std::uint8_t>& code, std::uint64_t entry_addr, std::uint64_t vaddr, std::uint64_t paddr, std::uint64_t memsz, std::size_t text_size) {
        if (code.empty()) {
            std::cerr << RED << "[OCCULTC] No code to write\n" << RESET;
            return;
        }

        if (text_size == 0 || text_size > code.size())
            text_size = code.size();

        const std::size_t data_size = code.size() - text_size;
        const std::size_t header_size = sizeof(elf_header) + sizeof(elf_program_header);
        const std::uint64_t filesz = header_size + code.size();

        if (memsz == 0 || memsz < filesz)
            memsz = filesz;

        std::vector<std::uint8_t> shstrtab;
        shstrtab.push_back(0);

        const std::uint32_t text_name_off = static_cast<std::uint32_t>(shstrtab.size());
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

        const std::uint32_t shstrtab_name_off = static_cast<std::uint32_t>(shstrtab.size());
        for (char c : std::string_view(".shstrtab"))
            shstrtab.push_back(static_cast<std::uint8_t>(c));
        shstrtab.push_back(0);

        const std::uint16_t shnum = static_cast<std::uint16_t>(data_size > 0 ? 4 : 3);
        const std::uint16_t shstrndx = static_cast<std::uint16_t>(shnum - 1);

        const std::size_t shstrtab_off = filesz;
        const std::size_t shtab_off_raw = shstrtab_off + shstrtab.size();
        const std::size_t shtab_off = (shtab_off_raw + 7) & ~static_cast<std::size_t>(7);
        const std::size_t padding_size = shtab_off - shtab_off_raw;

        std::vector<elf_section_header> sections(shnum, elf_section_header{});

        sections[1].sh_name = text_name_off;
        sections[1].sh_type = elfdef::SHT_PROGBITS;
        sections[1].sh_flags = elfdef::SHF_ALLOC | elfdef::SHF_EXECINSTR;
        sections[1].sh_addr = vaddr + header_size;
        sections[1].sh_offset = header_size;
        sections[1].sh_size = text_size;
        sections[1].sh_addralign = 16;

        if (data_size > 0) {
            sections[2].sh_name = rodata_name_off;
            sections[2].sh_type = elfdef::SHT_PROGBITS;
            sections[2].sh_flags = elfdef::SHF_ALLOC;
            sections[2].sh_addr = vaddr + header_size + text_size;
            sections[2].sh_offset = header_size + text_size;
            sections[2].sh_size = data_size;
            sections[2].sh_addralign = 1;
        }

        sections[shstrndx].sh_name = shstrtab_name_off;
        sections[shstrndx].sh_type = elfdef::SHT_STRTAB;
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

        file.write(reinterpret_cast<const char*>(&elf_h), sizeof(elf_h));
        file.write(reinterpret_cast<const char*>(&ph), sizeof(ph));
        file.write(reinterpret_cast<const char*>(code.data()), code.size());
        file.write(reinterpret_cast<const char*>(shstrtab.data()), shstrtab.size());

        if (padding_size > 0) {
            static constexpr std::uint8_t zeros[8] = {};
            file.write(reinterpret_cast<const char*>(zeros), static_cast<std::streamsize>(padding_size));
        }

        for (const auto& sh : sections)
            file.write(reinterpret_cast<const char*>(&sh), sizeof(sh));

        file.close();

        const std::size_t total_size = shtab_off + shnum * sizeof(elf_section_header);
        std::cout << GREEN << "[OCCULTC] ELF binary created: " << binary_name << " (" << total_size << " bytes, " << shnum << " sections)\n" << RESET;
    }

    namespace {
        std::size_t align_to(std::size_t v, std::size_t a) { return (v + a - 1) & ~(a - 1); }

        template <typename T>
        void append(std::vector<std::uint8_t>& out, const T& v) {
            const auto* b = reinterpret_cast<const std::uint8_t*>(&v);
            out.insert(out.end(), b, b + sizeof(T));
        }
    } // namespace

    std::size_t elf::dynamic_header_size() {
        return align_to(sizeof(elf_header) + 5 * sizeof(elf_program_header) + std::string_view(interpreter).size() + 1, 16);
    }

    void elf::generate_dynamic_binary(const std::string& binary_name,
                                      std::vector<std::uint8_t> image,
                                      std::uint64_t vaddr,
                                      std::size_t header_size,
                                      std::size_t text_size,
                                      const std::vector<std::string>& needed,
                                      const std::vector<std::pair<std::string, std::size_t>>& slot_uses) {
        using namespace elfdef;
        auto addr_of = [&](std::size_t image_off) { return vaddr + header_size + image_off; };
        auto pad_to = [&](std::size_t a) { image.resize(align_to(image.size(), a), 0); };

        // one symbol and one slot per name, however many places call it
        std::vector<std::string> names;
        for (const auto& [name, field] : slot_uses) {
            if (std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
            }
        }
        const std::size_t data_end = image.size();

        std::vector<std::uint8_t> dynstr{0};
        auto add_str = [&](const std::string& v) {
            const auto at = static_cast<Elf64_Word>(dynstr.size());
            dynstr.insert(dynstr.end(), v.begin(), v.end());
            dynstr.push_back(0);
            return at;
        };
        std::vector<Elf64_Word> needed_at;
        for (const auto& n : needed) {
            needed_at.push_back(add_str(n));
        }

        pad_to(8);
        const std::size_t dynsym_off = image.size();
        append(image, Elf64_Sym{});
        for (const auto& n : names) {
            Elf64_Sym sym{};
            sym.st_name = add_str(n);
            sym.st_info = st_info(STB_GLOBAL, STT_FUNC);
            sym.st_shndx = SHN_UNDEF;
            append(image, sym);
        }
        const std::size_t dynsym_size = image.size() - dynsym_off;

        const std::size_t dynstr_off = image.size();
        image.insert(image.end(), dynstr.begin(), dynstr.end());

        // nothing here is looked up by another object, so one empty bucket
        pad_to(8);
        const std::size_t hash_off = image.size();
        const auto nsyms = static_cast<std::uint32_t>(names.size() + 1);
        append(image, std::uint32_t{1});
        append(image, nsyms);
        append(image, std::uint32_t{0});
        for (std::uint32_t i = 0; i < nsyms; ++i) {
            append(image, std::uint32_t{0});
        }
        const std::size_t hash_size = image.size() - hash_off;

        pad_to(8);
        const std::size_t got_off = image.size();
        image.resize(image.size() + 8 * names.size(), 0);

        const std::size_t rela_off = image.size();
        for (std::size_t i = 0; i < names.size(); ++i) {
            Elf64_Rela r{};
            r.r_offset = addr_of(got_off + 8 * i);
            r.r_info = r_info(static_cast<Elf64_Word>(i + 1), R_X86_64_GLOB_DAT);
            append(image, r);
        }
        const std::size_t rela_size = image.size() - rela_off;

        const std::size_t dynamic_off = image.size();
        auto dyn = [&](Elf64_Sxword tag, Elf64_Xword val) { append(image, Elf64_Dyn{tag, val}); };
        for (const auto at : needed_at) {
            dyn(DT_NEEDED, at);
        }
        dyn(DT_HASH, addr_of(hash_off));
        dyn(DT_STRTAB, addr_of(dynstr_off));
        dyn(DT_SYMTAB, addr_of(dynsym_off));
        dyn(DT_STRSZ, dynstr.size());
        dyn(DT_SYMENT, sizeof(Elf64_Sym));
        dyn(DT_RELA, addr_of(rela_off));
        dyn(DT_RELASZ, rela_size);
        dyn(DT_RELAENT, sizeof(Elf64_Rela));
        dyn(DT_DEBUG, 0);
        dyn(DT_NULL, 0);
        const std::size_t dynamic_size = image.size() - dynamic_off;

        // every call site reaches its name's slot; rip is the end of the field
        for (const auto& [name, field] : slot_uses) {
            const auto slot = std::find(names.begin(), names.end(), name) - names.begin();
            const auto target = static_cast<std::int64_t>(addr_of(got_off + 8 * static_cast<std::size_t>(slot)));
            const auto disp = static_cast<std::int32_t>(target - static_cast<std::int64_t>(addr_of(field + 4)));
            for (int j = 0; j < 4; ++j) image[field + j] = static_cast<std::uint8_t>((disp >> (j * 8)) & 0xFF);
        }

        // the headers, then the image, then what only tools read
        const std::string_view interp(interpreter);
        const std::size_t phoff = sizeof(elf_header);
        const std::size_t interp_off = phoff + 5 * sizeof(elf_program_header);
        const std::size_t loaded = header_size + image.size();

        std::vector<std::uint8_t> shstrtab{0};
        auto add_sh = [&](std::string_view v) {
            const auto at = static_cast<Elf64_Word>(shstrtab.size());
            shstrtab.insert(shstrtab.end(), v.begin(), v.end());
            shstrtab.push_back(0);
            return at;
        };
        std::vector<elf_section_header> sh(1);
        auto section = [&](std::string_view name, Elf64_Word type, Elf64_Xword flags, std::size_t off, std::size_t size,
                           Elf64_Word link = 0, Elf64_Word info = 0, Elf64_Xword align = 8, Elf64_Xword entsize = 0) {
            elf_section_header h{};
            h.sh_name = add_sh(name);
            h.sh_type = type;
            h.sh_flags = flags;
            h.sh_addr = vaddr + off;
            h.sh_offset = off;
            h.sh_size = size;
            h.sh_link = link;
            h.sh_info = info;
            h.sh_addralign = align;
            h.sh_entsize = entsize;
            sh.push_back(h);
            return static_cast<Elf64_Word>(sh.size() - 1);
        };
        section(".interp", SHT_PROGBITS, SHF_ALLOC, interp_off, interp.size() + 1, 0, 0, 1);
        section(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, header_size, text_size, 0, 0, 16);
        if (data_end > text_size) {
            section(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, header_size + text_size, data_end - text_size, 0, 0, 1);
        }
        const Elf64_Word dynstr_ix = static_cast<Elf64_Word>(sh.size() + 1);
        const Elf64_Word dynsym_ix = section(".dynsym", SHT_DYNSYM, SHF_ALLOC, header_size + dynsym_off, dynsym_size,
                                             dynstr_ix, 1, 8, sizeof(Elf64_Sym));
        section(".dynstr", SHT_STRTAB, SHF_ALLOC, header_size + dynstr_off, dynstr.size(), 0, 0, 1);
        section(".hash", SHT_HASH, SHF_ALLOC, header_size + hash_off, hash_size, dynsym_ix, 0, 8, 4);
        section(".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, header_size + got_off, 8 * names.size(), 0, 0, 8, 8);
        section(".rela.dyn", SHT_RELA, SHF_ALLOC, header_size + rela_off, rela_size, dynsym_ix, 0, 8, sizeof(Elf64_Rela));
        section(".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, header_size + dynamic_off, dynamic_size, dynstr_ix, 0, 8,
                sizeof(Elf64_Dyn));
        // not loaded: it sits after the segment, and only tools read it
        const Elf64_Word shstrndx = section(".shstrtab", SHT_STRTAB, 0, 0, 0, 0, 0, 1);
        sh[shstrndx].sh_addr = 0;
        sh[shstrndx].sh_offset = loaded;
        sh[shstrndx].sh_size = shstrtab.size();
        const std::size_t shoff = align_to(loaded + shstrtab.size(), 8);

        elf_header eh = generate_elf_header(vaddr + header_size);
        eh.e_phnum = 5;
        eh.e_shoff = shoff;
        eh.e_shnum = static_cast<Elf64_Half>(sh.size());
        eh.e_shstrndx = static_cast<Elf64_Half>(shstrndx);

        auto phdr = [](Elf64_Word type, Elf64_Word flags, std::size_t off, std::uint64_t addr, std::size_t size, std::size_t align) {
            elf_program_header p{};
            p.p_type = type;
            p.p_flags = flags;
            p.p_offset = off;
            p.p_vaddr = addr;
            p.p_paddr = addr;
            p.p_filesz = size;
            p.p_memsz = size;
            p.p_align = align;
            return p;
        };

        std::vector<std::uint8_t> file;
        file.reserve(shoff + sh.size() * sizeof(elf_section_header));
        append(file, eh);
        // PHDR and INTERP have to come before the LOAD that maps them
        append(file, phdr(PT_PHDR, PF_R, phoff, vaddr + phoff, 5 * sizeof(elf_program_header), 8));
        append(file, phdr(PT_INTERP, PF_R, interp_off, vaddr + interp_off, interp.size() + 1, 1));
        append(file, phdr(PT_LOAD, PF_R | PF_W | PF_X, 0, vaddr, loaded, 0x1000));
        append(file, phdr(PT_DYNAMIC, PF_R | PF_W, header_size + dynamic_off, addr_of(dynamic_off), dynamic_size, 8));
        append(file, phdr(PT_GNU_STACK, PF_R | PF_W, 0, 0, 0, 16));
        file.insert(file.end(), interp.begin(), interp.end());
        file.push_back(0);
        file.resize(header_size, 0);
        file.insert(file.end(), image.begin(), image.end());
        file.insert(file.end(), shstrtab.begin(), shstrtab.end());
        file.resize(shoff, 0);
        for (const auto& h : sh) {
            append(file, h);
        }

        std::ofstream out(binary_name, std::ios::binary);
        if (!out) {
            std::cerr << RED << "[OCCULTC] Failed to create binary\n" << RESET;
            return;
        }
        out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
        out.close();

        std::string libs;
        for (const auto& n : needed) {
            libs += " " + n;
        }
        std::cout << GREEN << "[OCCULTC] ELF binary created: " << binary_name << " (" << file.size()
                  << " bytes, dynamic, needs" << libs << ")\n" << RESET;
    }

    std::size_t elf::shared_header_size() {
        return align_to(sizeof(elf_header) + 4 * sizeof(elf_program_header), 16);
    }

    void elf::generate_shared_object(const std::string& binary_name,
                                     std::vector<std::uint8_t> image,
                                     std::size_t header_size,
                                     std::size_t text_size,
                                     const std::string& soname,
                                     const std::vector<std::pair<std::string, std::size_t>>& exports,
                                     const std::vector<std::string>& needed,
                                     const std::vector<std::pair<std::string, std::size_t>>& slot_uses,
                                     const std::vector<std::size_t>& abs_relocs) {
        using namespace elfdef;
        constexpr std::uint64_t vaddr = 0; // ET_DYN; loader picks the base
        auto addr_of = [&](std::size_t image_off) { return vaddr + header_size + image_off; };
        auto pad_to = [&](std::size_t a) { image.resize(align_to(image.size(), a), 0); };

        std::vector<std::string> import_names;
        for (const auto& [name, field] : slot_uses) {
            if (std::find(import_names.begin(), import_names.end(), name) == import_names.end()) {
                import_names.push_back(name);
            }
        }
        const std::size_t data_end = image.size();

        std::vector<std::uint8_t> dynstr{0};
        auto add_str = [&](const std::string& v) {
            const auto at = static_cast<Elf64_Word>(dynstr.size());
            dynstr.insert(dynstr.end(), v.begin(), v.end());
            dynstr.push_back(0);
            return at;
        };
        const Elf64_Word soname_at = add_str(soname);
        std::vector<Elf64_Word> needed_at;
        for (const auto& n : needed) {
            needed_at.push_back(add_str(n));
        }

        pad_to(8);
        const std::size_t dynsym_off = image.size();
        // sym 0: SHN_UNDEF
        append(image, Elf64_Sym{});
        std::vector<Elf64_Word> import_name_offs;
        for (const auto& n : import_names) {
            Elf64_Sym sym{};
            sym.st_name = add_str(n);
            sym.st_info = st_info(STB_GLOBAL, STT_FUNC);
            sym.st_shndx = SHN_UNDEF;
            import_name_offs.push_back(sym.st_name);
            append(image, sym);
        }
        std::vector<std::size_t> export_sym_image_offsets;
        for (const auto& [name, text_off] : exports) {
            Elf64_Sym sym{};
            sym.st_name = add_str(name);
            sym.st_info = st_info(STB_GLOBAL, STT_FUNC);
            sym.st_shndx = 0; // filled in below
            sym.st_value = addr_of(text_off);
            sym.st_size = 0;
            export_sym_image_offsets.push_back(image.size());
            append(image, sym);
        }
        const std::size_t dynsym_size = image.size() - dynsym_off;

        const std::size_t dynstr_off = image.size();
        image.insert(image.end(), dynstr.begin(), dynstr.end());

        pad_to(8);
        const std::size_t hash_off = image.size();
        const auto nsyms = static_cast<std::uint32_t>(1 + import_names.size() + exports.size());
        append(image, std::uint32_t{1});       // nbucket
        append(image, nsyms);                   // nchain
        append(image, std::uint32_t{nsyms > 1 ? 1u : 0u}); // bucket[0]: first real sym, or 0
        append(image, std::uint32_t{0});        // chain[0]: UNDEF sentinel
        for (std::uint32_t i = 1; i < nsyms; ++i) {
            // every symbol in bucket 0, chain i -> i+1, last -> 0
            append(image, std::uint32_t{i + 1 < nsyms ? i + 1 : 0});
        }
        const std::size_t hash_size = image.size() - hash_off;

        pad_to(8);
        const std::size_t got_off = image.size();
        image.resize(image.size() + 8 * import_names.size(), 0);

        const std::size_t rela_off = image.size();
        for (std::size_t i = 0; i < import_names.size(); ++i) {
            Elf64_Rela r{};
            r.r_offset = addr_of(got_off + 8 * i);
            // Import sym indices run 1..import_names.size(); symbol 0 is undef.
            r.r_info = r_info(static_cast<Elf64_Word>(i + 1), R_X86_64_GLOB_DAT);
            append(image, r);
        }
        for (const auto image_off : abs_relocs) {
            std::int64_t addend = 0;
            for (int j = 0; j < 8; ++j) {
                addend |= static_cast<std::int64_t>(image[image_off + j]) << (j * 8);
            }
            Elf64_Rela r{};
            r.r_offset = addr_of(image_off);
            r.r_info = r_info(0, R_X86_64_RELATIVE);
            r.r_addend = addend;
            append(image, r);
        }
        const std::size_t rela_size = image.size() - rela_off;
        const bool have_relocs = !import_names.empty() || !abs_relocs.empty();

        const std::size_t dynamic_off = image.size();
        auto dyn = [&](Elf64_Sxword tag, Elf64_Xword val) { append(image, Elf64_Dyn{tag, val}); };
        dyn(DT_SONAME, soname_at);
        for (const auto at : needed_at) {
            dyn(DT_NEEDED, at);
        }
        dyn(DT_HASH, addr_of(hash_off));
        dyn(DT_STRTAB, addr_of(dynstr_off));
        dyn(DT_SYMTAB, addr_of(dynsym_off));
        dyn(DT_STRSZ, dynstr.size());
        dyn(DT_SYMENT, sizeof(Elf64_Sym));
        if (have_relocs) {
            dyn(DT_RELA, addr_of(rela_off));
            dyn(DT_RELASZ, rela_size);
            dyn(DT_RELAENT, sizeof(Elf64_Rela));
        }
        dyn(DT_DEBUG, 0);
        dyn(DT_NULL, 0);
        const std::size_t dynamic_size = image.size() - dynamic_off;

        for (const auto& [name, field] : slot_uses) {
            const auto slot = std::find(import_names.begin(), import_names.end(), name) - import_names.begin();
            const auto target = static_cast<std::int64_t>(addr_of(got_off + 8 * static_cast<std::size_t>(slot)));
            const auto disp = static_cast<std::int32_t>(target - static_cast<std::int64_t>(addr_of(field + 4)));
            for (int j = 0; j < 4; ++j) image[field + j] = static_cast<std::uint8_t>((disp >> (j * 8)) & 0xFF);
        }

        const std::size_t phoff = sizeof(elf_header);
        const std::size_t loaded = header_size + image.size();

        std::vector<std::uint8_t> shstrtab{0};
        auto add_sh = [&](std::string_view v) {
            const auto at = static_cast<Elf64_Word>(shstrtab.size());
            shstrtab.insert(shstrtab.end(), v.begin(), v.end());
            shstrtab.push_back(0);
            return at;
        };
        std::vector<elf_section_header> sh(1);
        auto section = [&](std::string_view name, Elf64_Word type, Elf64_Xword flags, std::size_t off, std::size_t size,
                           Elf64_Word link = 0, Elf64_Word info = 0, Elf64_Xword align = 8, Elf64_Xword entsize = 0) {
            elf_section_header h{};
            h.sh_name = add_sh(name);
            h.sh_type = type;
            h.sh_flags = flags;
            h.sh_addr = vaddr + off;
            h.sh_offset = off;
            h.sh_size = size;
            h.sh_link = link;
            h.sh_info = info;
            h.sh_addralign = align;
            h.sh_entsize = entsize;
            sh.push_back(h);
            return static_cast<Elf64_Word>(sh.size() - 1);
        };
        const Elf64_Word text_ix = section(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, header_size, text_size, 0, 0, 16);
        if (data_end > text_size) {
            section(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, header_size + text_size, data_end - text_size, 0, 0, 1);
        }
        const Elf64_Word dynstr_ix = static_cast<Elf64_Word>(sh.size() + 1);
        const Elf64_Word dynsym_ix = section(".dynsym", SHT_DYNSYM, SHF_ALLOC, header_size + dynsym_off, dynsym_size,
                                             dynstr_ix, 1, 8, sizeof(Elf64_Sym));
        section(".dynstr", SHT_STRTAB, SHF_ALLOC, header_size + dynstr_off, dynstr.size(), 0, 0, 1);
        section(".hash", SHT_HASH, SHF_ALLOC, header_size + hash_off, hash_size, dynsym_ix, 0, 8, 4);
        if (!import_names.empty()) {
            section(".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, header_size + got_off, 8 * import_names.size(), 0, 0, 8, 8);
        }
        if (have_relocs) {
            section(".rela.dyn", SHT_RELA, SHF_ALLOC, header_size + rela_off, rela_size, dynsym_ix, 0, 8, sizeof(Elf64_Rela));
        }
        section(".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, header_size + dynamic_off, dynamic_size, dynstr_ix, 0, 8,
                sizeof(Elf64_Dyn));
        const Elf64_Word shstrndx = section(".shstrtab", SHT_STRTAB, 0, 0, 0, 0, 0, 1);
        sh[shstrndx].sh_addr = 0;
        sh[shstrndx].sh_offset = loaded;
        sh[shstrndx].sh_size = shstrtab.size();
        const std::size_t shoff = align_to(loaded + shstrtab.size(), 8);

        // Backfill st_shndx on every export sym. They all point into .text.
        for (const auto image_off : export_sym_image_offsets) {
            auto* sym = reinterpret_cast<Elf64_Sym*>(image.data() + image_off);
            sym->st_shndx = static_cast<Elf64_Half>(text_ix);
        }

        elf_header eh = generate_elf_header(0);
        eh.e_type = ET_DYN;
        eh.e_entry = 0;
        eh.e_phnum = 4;
        eh.e_shoff = shoff;
        eh.e_shnum = static_cast<Elf64_Half>(sh.size());
        eh.e_shstrndx = static_cast<Elf64_Half>(shstrndx);

        auto phdr = [](Elf64_Word type, Elf64_Word flags, std::size_t off, std::uint64_t addr, std::size_t size, std::size_t align) {
            elf_program_header p{};
            p.p_type = type;
            p.p_flags = flags;
            p.p_offset = off;
            p.p_vaddr = addr;
            p.p_paddr = addr;
            p.p_filesz = size;
            p.p_memsz = size;
            p.p_align = align;
            return p;
        };

        std::vector<std::uint8_t> file;
        file.reserve(shoff + sh.size() * sizeof(elf_section_header));
        append(file, eh);
        append(file, phdr(PT_PHDR, PF_R, phoff, vaddr + phoff, 4 * sizeof(elf_program_header), 8));
        append(file, phdr(PT_LOAD, PF_R | PF_W | PF_X, 0, vaddr, loaded, 0x1000));
        append(file, phdr(PT_DYNAMIC, PF_R | PF_W, header_size + dynamic_off, addr_of(dynamic_off), dynamic_size, 8));
        append(file, phdr(PT_GNU_STACK, PF_R | PF_W, 0, 0, 0, 16));
        file.resize(header_size, 0);
        file.insert(file.end(), image.begin(), image.end());
        file.insert(file.end(), shstrtab.begin(), shstrtab.end());
        file.resize(shoff, 0);
        for (const auto& h : sh) {
            append(file, h);
        }

        std::ofstream out(binary_name, std::ios::binary);
        if (!out) {
            std::cerr << RED << "[OCCULTC] Failed to create shared object\n" << RESET;
            return;
        }
        out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
        out.close();

        std::cout << GREEN << "[OCCULTC] ELF shared object created: " << binary_name << " ("
                  << file.size() << " bytes, soname " << soname << ", "
                  << exports.size() << " export" << (exports.size() == 1 ? "" : "s") << ")\n"
                  << RESET;
    }

} // namespace occult
