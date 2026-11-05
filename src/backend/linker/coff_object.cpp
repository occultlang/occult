#include "coff_object.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include "../../util/color_defs.hpp"

namespace occult {
    namespace {
        constexpr std::uint16_t kMachineAmd64 = 0x8664;

        constexpr std::uint32_t kScnCntCode = 0x00000020;
        constexpr std::uint32_t kScnCntInitData = 0x00000040;
        constexpr std::uint32_t kScnLnkInfo = 0x00000200;
        constexpr std::uint32_t kScnLnkRemove = 0x00000800;
        constexpr std::uint32_t kScnAlign8 = 0x00400000;
        constexpr std::uint32_t kScnAlign16 = 0x00500000;
        constexpr std::uint32_t kScnMemExecute = 0x20000000;
        constexpr std::uint32_t kScnMemRead = 0x40000000;
        constexpr std::uint32_t kScnMemWrite = 0x80000000;

        constexpr std::uint16_t kRelAddr64 = 0x0001;
        constexpr std::uint16_t kRelRel32 = 0x0004;

        constexpr std::uint8_t kSymClassExternal = 2;
        constexpr std::uint8_t kSymClassStatic = 3;
        constexpr std::uint16_t kSymTypeFunction = 0x20;

        struct strtab {
            std::vector<char> bytes;
            std::uint32_t add(const std::string& s) {
                const auto at = static_cast<std::uint32_t>(bytes.size()) + 4; // past the size field
                bytes.insert(bytes.end(), s.begin(), s.end());
                bytes.push_back('\0');
                return at;
            }
        };

        template <typename T>
        void put(std::vector<std::uint8_t>& out, const T& v) {
            const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
            out.insert(out.end(), p, p + sizeof(T));
        }

        void pad_to(std::vector<std::uint8_t>& out, std::size_t align) {
            while (out.size() % align != 0) {
                out.push_back(0);
            }
        }

        // a 32-bit field, little-endian, written over whatever was there
        void poke32(std::vector<std::uint8_t>& buf, std::size_t at, std::int32_t v) {
            for (int i = 0; i < 4; ++i) {
                buf[at + i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
            }
        }

        void poke64(std::vector<std::uint8_t>& buf, std::size_t at, std::int64_t v) {
            for (int i = 0; i < 8; ++i) {
                buf[at + i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
            }
        }

        struct section {
            std::string name;
            std::uint32_t characteristics;
            std::vector<std::uint8_t> data;
            std::vector<std::uint8_t> relocs; // packed IMAGE_RELOCATION records
            std::uint32_t reloc_count{};
        };

        struct symbol {
            std::string name;
            std::uint32_t value{};
            std::int16_t section{}; // 1-based, 0 is undefined
            std::uint16_t type{};
            std::uint8_t storage_class{};
        };
    } // namespace

    void coff_object::write(const std::string& filename,
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
                            bool debug,
                            bool defaultlibs,
                            const std::vector<std::string>& libraries) {
        // the code is patched in place below, so it cannot be the caller's
        std::vector<std::uint8_t> text = code;

        // ----------------------------------------------------------- .rdata
        // the shape every other backend uses: length in eight bytes, then
        // the content, then eight zeros, so the pointer reads as a C
        // const char *. sorted by their old address so two runs agree.
        std::vector<std::uint8_t> rdata;
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
                put(rdata, len);
                string_at[addr] = rdata.size();
                rdata.insert(rdata.end(), content.begin(), content.end());
                rdata.insert(rdata.end(), 8, 0);
            }
        }

        const bool has_init = !init_fn.empty() && function_locs.count(init_fn) > 0;

        // --------------------------------------------------------- sections
        std::vector<section> sections;
        auto add_section = [&](std::string name, std::uint32_t ch, std::vector<std::uint8_t> d) {
            sections.push_back(section{std::move(name), ch, std::move(d), {}, 0});
            return static_cast<std::int16_t>(sections.size()); // 1-based
        };

        std::uint32_t text_flags = kScnCntCode | kScnMemExecute | kScnMemRead | kScnAlign16;
        if (writable_text) {
            text_flags |= kScnMemWrite;
        }
        const std::int16_t sec_text = add_section(".text", text_flags, {});
        const std::int16_t sec_rdata =
            add_section(".rdata", kScnCntInitData | kScnMemRead | kScnAlign8, rdata);
        const std::int16_t sec_data =
            add_section(".data", kScnCntInitData | kScnMemRead | kScnMemWrite | kScnAlign8,
                        std::vector<std::uint8_t>(globals_size > 0 ? globals_size : 0, 0));
        // both runtimes walk .CRT$XC* between XCA and XCZ before main
        const std::int16_t sec_init =
            has_init ? add_section(".CRT$XCU", kScnCntInitData | kScnMemRead | kScnAlign8,
                                   std::vector<std::uint8_t>(8, 0))
                     : 0;

        if (defaultlibs) {
            static const char kDirectives[] =
                " /DEFAULTLIB:\"libcmt\" /DEFAULTLIB:\"oldnames\""
                " /DEFAULTLIB:\"legacy_stdio_definitions\" ";
            std::string directives(kDirectives);
            for (const auto& lib : libraries) {
                directives += "/DEFAULTLIB:\"" + lib + "\" ";
            }
            std::vector<std::uint8_t> d(directives.begin(), directives.end()); // no NUL
            add_section(".drectve", kScnLnkInfo | kScnLnkRemove, std::move(d));
        }

        // ---------------------------------------------------------- symbols
        std::vector<symbol> syms;
        auto add_symbol = [&](std::string name, std::uint32_t value, std::int16_t sec,
                              std::uint16_t type, std::uint8_t cls) {
            syms.push_back(symbol{std::move(name), value, sec, type, cls});
            return static_cast<std::uint32_t>(syms.size() - 1);
        };

        const std::uint32_t sym_text = add_symbol(".text", 0, sec_text, 0, kSymClassStatic);
        const std::uint32_t sym_rdata = add_symbol(".rdata", 0, sec_rdata, 0, kSymClassStatic);
        const std::uint32_t sym_data = add_symbol(".data", 0, sec_data, 0, kSymClassStatic);

        std::vector<std::pair<std::uint32_t, std::string>> by_offset;
        by_offset.reserve(function_locs.size());
        for (const auto& [name, loc] : function_locs) {
            by_offset.emplace_back(loc, name);
        }
        std::sort(by_offset.begin(), by_offset.end());

        std::unordered_map<std::string, std::uint32_t> sym_of;
        for (const auto& [loc, name] : by_offset) {
            sym_of[name] = add_symbol(name, loc, sec_text, kSymTypeFunction, kSymClassExternal);
        }

        std::vector<std::pair<std::int32_t, std::string>> globals_by_offset;
        globals_by_offset.reserve(globals.size());
        for (const auto& [name, off] : globals) {
            globals_by_offset.emplace_back(off, name);
        }
        std::sort(globals_by_offset.begin(), globals_by_offset.end());
        for (const auto& [off, name] : globals_by_offset) {
            add_symbol(name, static_cast<std::uint32_t>(off), sec_data, 0, kSymClassExternal);
        }

        for (const auto& name : undefined) {
            sym_of[name] = add_symbol(name, 0, 0, 0, kSymClassExternal);
        }

        // ----------------------------------------------------- relocations
        // COFF has no addend field: the value goes in the bytes being
        // relocated. REL32 measures from four bytes past the field, where
        // an ELF PC32 measures from the field itself, so the addend the
        // code generator computed arrives here four short.
        bool failed = false;
        auto add_reloc = [&](section& s, std::uint32_t at, std::uint32_t sym, std::uint16_t type) {
            put(s.relocs, at);
            put(s.relocs, sym);
            put(s.relocs, type);
            s.reloc_count++;
        };
        section& text_sec = sections[sec_text - 1];
        section& init_sec = has_init ? sections[sec_init - 1] : sections[sec_text - 1];

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
                sym = sym_rdata;
            }
            else if (r.section == against::data) {
                sym = sym_data;
            }

            if (r.k == kind::abs64) {
                if (r.at + 8 > text.size()) {
                    failed = true;
                    break;
                }
                poke64(text, r.at, r.addend);
                add_reloc(text_sec, static_cast<std::uint32_t>(r.at), sym, kRelAddr64);
            }
            else {
                if (r.at + 4 > text.size()) {
                    failed = true;
                    break;
                }
                poke32(text, r.at, static_cast<std::int32_t>(r.addend + 4));
                add_reloc(text_sec, static_cast<std::uint32_t>(r.at), sym, kRelRel32);
            }
        }
        if (failed) {
            std::cerr << RED << "[OBJECT ERROR] a relocation points past the end of .text\n"
                      << RESET;
            return;
        }

        // a string pointer is an absolute eight bytes into .rdata
        for (const auto& [at, old_addr] : string_relocs) {
            auto it = string_at.find(old_addr);
            if (it == string_at.end()) {
                continue;
            }
            poke64(text, at, static_cast<std::int64_t>(it->second));
            add_reloc(text_sec, static_cast<std::uint32_t>(at), sym_rdata, kRelAddr64);
        }
        // a global's address likewise, into .data
        for (const auto& [at, off] : global_relocs) {
            poke64(text, at, off);
            add_reloc(text_sec, static_cast<std::uint32_t>(at), sym_data, kRelAddr64);
        }
        if (has_init) {
            add_reloc(init_sec, 0, sym_of.at(init_fn), kRelAddr64);
        }

        text_sec.data = std::move(text);

        for (const auto& s : sections) {
            if (s.reloc_count > 0xFFFF) {
                std::cerr << RED << "[OBJECT ERROR] " << s.name << " has " << s.reloc_count
                          << " relocations; COFF holds 65535 without an overflow record\n"
                          << RESET;
                return;
            }
        }

        // -------------------------------------------------------- the file
        strtab str;
        const std::uint32_t header_size = 20;
        const std::uint32_t headers_end =
            header_size + static_cast<std::uint32_t>(sections.size()) * 40;

        std::uint32_t cursor = headers_end;
        std::vector<std::uint32_t> raw_at(sections.size(), 0);
        std::vector<std::uint32_t> reloc_at(sections.size(), 0);
        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (!sections[i].data.empty()) {
                cursor = (cursor + 3) & ~3u;
                raw_at[i] = cursor;
                cursor += static_cast<std::uint32_t>(sections[i].data.size());
            }
        }
        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (sections[i].reloc_count > 0) {
                reloc_at[i] = cursor;
                cursor += static_cast<std::uint32_t>(sections[i].relocs.size());
            }
        }
        const std::uint32_t symtab_at = cursor;

        std::vector<std::uint8_t> out;

        put<std::uint16_t>(out, kMachineAmd64);
        put<std::uint16_t>(out, static_cast<std::uint16_t>(sections.size()));
        put<std::uint32_t>(out, 0); // no timestamp, so two runs agree byte for byte
        put<std::uint32_t>(out, symtab_at);
        put<std::uint32_t>(out, static_cast<std::uint32_t>(syms.size()));
        put<std::uint16_t>(out, 0); // an object has no optional header
        put<std::uint16_t>(out, 0);

        for (std::size_t i = 0; i < sections.size(); ++i) {
            const auto& s = sections[i];
            char name[8] = {};
            if (s.name.size() <= 8) {
                std::memcpy(name, s.name.data(), s.name.size());
            }
            else {
                const std::string at = "/" + std::to_string(str.add(s.name));
                std::memcpy(name, at.data(), std::min<std::size_t>(at.size(), 8));
            }
            out.insert(out.end(), name, name + 8);
            put<std::uint32_t>(out, 0); // VirtualSize is zero in an object
            put<std::uint32_t>(out, 0); // and so is VirtualAddress
            put<std::uint32_t>(out, static_cast<std::uint32_t>(s.data.size()));
            put<std::uint32_t>(out, raw_at[i]);
            put<std::uint32_t>(out, reloc_at[i]);
            put<std::uint32_t>(out, 0); // no line numbers
            put<std::uint16_t>(out, static_cast<std::uint16_t>(s.reloc_count));
            put<std::uint16_t>(out, 0);
            put<std::uint32_t>(out, s.characteristics);
        }

        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (!sections[i].data.empty()) {
                pad_to(out, 4);
                out.insert(out.end(), sections[i].data.begin(), sections[i].data.end());
            }
        }
        for (const auto& s : sections) {
            out.insert(out.end(), s.relocs.begin(), s.relocs.end());
        }

        for (const auto& s : syms) {
            if (s.name.size() <= 8) {
                char name[8] = {};
                std::memcpy(name, s.name.data(), s.name.size());
                out.insert(out.end(), name, name + 8);
            }
            else {
                put<std::uint32_t>(out, 0); // zero here means "look in the string table"
                put<std::uint32_t>(out, str.add(s.name));
            }
            put<std::uint32_t>(out, s.value);
            put<std::int16_t>(out, s.section);
            put<std::uint16_t>(out, s.type);
            put<std::uint8_t>(out, s.storage_class);
            put<std::uint8_t>(out, 0); // no auxiliary records
        }

        put<std::uint32_t>(out, static_cast<std::uint32_t>(str.bytes.size()) + 4);
        out.insert(out.end(), str.bytes.begin(), str.bytes.end());

        std::ofstream f(filename, std::ios::binary);
        if (!f) {
            std::cerr << RED << "[OBJECT ERROR] cannot write " << filename << "\n" << RESET;
            return;
        }
        f.write(reinterpret_cast<const char*>(out.data()),
                static_cast<std::streamsize>(out.size()));

        if (debug) {
            std::cout << GREEN << "[OBJECT] " << RESET << filename << ": " << sections.size()
                      << " sections, " << syms.size() << " symbols\n";
        }
        std::cout << GREEN << "[OCCULTC] COFF object created: " << RESET << filename << " ("
                  << out.size() << " bytes, " << function_locs.size() << " function(s), "
                  << undefined.size() << " undefined)" << std::endl;
    }
} // namespace occult
