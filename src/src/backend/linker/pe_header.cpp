#include "pe_header.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "../../util/color_defs.hpp"

namespace occult {
    namespace {
        std::uint32_t align_up(std::uint32_t v, std::uint32_t a) {
            return (v + a - 1) & ~(a - 1);
        }

        void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {
            for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>((x >> (i * 8)) & 0xFF));
        }

        void poke32(std::vector<std::uint8_t>& v, std::size_t at, std::uint32_t x) {
            for (int i = 0; i < 4; ++i) v[at + i] = static_cast<std::uint8_t>((x >> (i * 8)) & 0xFF);
        }
    } // namespace

    std::vector<std::uint8_t> pe::build_imports(const std::vector<import>& imports,
                                                std::uint32_t at_rva,
                                                iat_map& iat,
                                                pedef::data_directory& import_dir,
                                                pedef::data_directory& iat_dir) {
        std::vector<std::uint8_t> blob;
        if (imports.empty()) {
            import_dir = {0, 0};
            iat_dir = {0, 0};
            return blob;
        }

        const std::uint32_t descriptors_size =
            static_cast<std::uint32_t>((imports.size() + 1) * sizeof(pedef::import_descriptor));

        std::uint32_t thunks_size = 0;
        for (const auto& im : imports) thunks_size += static_cast<std::uint32_t>((im.names.size() + 1) * 8 * 2);

        const std::uint32_t descriptors_at = 0;
        const std::uint32_t thunks_at = descriptors_size;
        const std::uint32_t names_at = thunks_at + thunks_size;

        blob.resize(names_at, 0);

        // the name blobs, which the tables above point into
        std::unordered_map<std::string, std::uint32_t> dll_name_rva;
        std::unordered_map<std::string, std::uint32_t> hint_name_rva;
        auto add_string = [&](const std::string& s) {
            const std::uint32_t rva = at_rva + static_cast<std::uint32_t>(blob.size());
            blob.insert(blob.end(), s.begin(), s.end());
            blob.push_back(0);
            if (blob.size() & 1) blob.push_back(0); // a hint/name entry is even-aligned
            return rva;
        };

        for (const auto& im : imports) {
            dll_name_rva[im.dll] = add_string(im.dll);
            for (const auto& n : im.names) {
                const std::uint32_t rva = at_rva + static_cast<std::uint32_t>(blob.size());
                blob.push_back(0);
                blob.push_back(0);
                blob.insert(blob.end(), n.begin(), n.end());
                blob.push_back(0);
                if (blob.size() & 1) blob.push_back(0);
                hint_name_rva[im.dll + "!" + n] = rva;
            }
        }

        // now the tables, written back over the space left for them
        std::size_t desc_off = descriptors_at;
        std::size_t thunk_off = thunks_at;
        for (const auto& im : imports) {
            const std::uint32_t ilt_rva = at_rva + static_cast<std::uint32_t>(thunk_off);
            std::size_t p = thunk_off;
            for (const auto& n : im.names) {
                const std::uint64_t entry = hint_name_rva[im.dll + "!" + n];
                for (int i = 0; i < 8; ++i) blob[p + i] = static_cast<std::uint8_t>((entry >> (i * 8)) & 0xFF);
                p += 8;
            }
            p += 8; // the null that ends the lookup table

            const std::uint32_t iat_rva_here = at_rva + static_cast<std::uint32_t>(p);
            for (const auto& n : im.names) {
                const std::uint64_t entry = hint_name_rva[im.dll + "!" + n];
                for (int i = 0; i < 8; ++i) blob[p + i] = static_cast<std::uint8_t>((entry >> (i * 8)) & 0xFF);
                iat[n] = at_rva + static_cast<std::uint32_t>(p);
                p += 8;
            }
            p += 8;

            poke32(blob, desc_off + 0, ilt_rva);
            poke32(blob, desc_off + 12, dll_name_rva[im.dll]);
            poke32(blob, desc_off + 16, iat_rva_here);
            desc_off += sizeof(pedef::import_descriptor);
            thunk_off = p;
        }

        import_dir = {at_rva + descriptors_at, descriptors_size};
        iat_dir = {at_rva + thunks_at, thunks_size};
        return blob;
    }

    void pe::generate_binary(const std::string& binary_name,
                             const std::vector<std::uint8_t>& image,
                             std::uint32_t entry_rva,
                             const pedef::data_directory& import_dir,
                             const pedef::data_directory& iat_dir) {
        if (image.empty()) {
            std::cerr << RED << "[OCCULTC] No code to write\n" << RESET;
            return;
        }

        std::vector<std::uint8_t> head;
        head.reserve(file_align);

        pedef::dos_header dos = {};
        dos.e_magic = pedef::DOS_SIGNATURE;
        dos.e_cblp = 0x90;
        dos.e_cp = 3;
        dos.e_cparhdr = 4;
        dos.e_maxalloc = 0xFFFF;
        dos.e_sp = 0xB8;
        dos.e_lfarlc = 0x40;
        dos.e_lfanew = 0x80;
        const auto* dos_bytes = reinterpret_cast<const std::uint8_t*>(&dos);
        head.insert(head.end(), dos_bytes, dos_bytes + sizeof(dos));

        static const std::uint8_t dos_stub[] = {
            0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
            0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21,
            'T', 'h', 'i', 's', ' ', 'p', 'r', 'o', 'g', 'r', 'a', 'm', ' ',
            'c', 'a', 'n', 'n', 'o', 't', ' ', 'b', 'e', ' ', 'r', 'u', 'n', ' ',
            'i', 'n', ' ', 'D', 'O', 'S', ' ', 'm', 'o', 'd', 'e', '.', '\r', '\r', '\n', '$'};
        head.insert(head.end(), std::begin(dos_stub), std::end(dos_stub));
        head.resize(0x80, 0);

        put32(head, pedef::NT_SIGNATURE);

        const std::uint32_t headers_raw =
            0x80 + 4 + static_cast<std::uint32_t>(sizeof(pedef::file_header) + sizeof(pedef::optional_header64) + sizeof(pedef::section_header));
        const std::uint32_t size_of_headers = align_up(headers_raw, file_align);
        const std::uint32_t raw_size = align_up(static_cast<std::uint32_t>(image.size()), file_align);

        pedef::file_header fh = {};
        fh.Machine = pedef::FILE_MACHINE_AMD64;
        fh.NumberOfSections = 1;
        fh.SizeOfOptionalHeader = sizeof(pedef::optional_header64);
        fh.Characteristics = pedef::FILE_RELOCS_STRIPPED | pedef::FILE_EXECUTABLE_IMAGE |
                             pedef::FILE_LARGE_ADDRESS_AWARE;
        const auto* fh_bytes = reinterpret_cast<const std::uint8_t*>(&fh);
        head.insert(head.end(), fh_bytes, fh_bytes + sizeof(fh));

        pedef::optional_header64 oh = {};
        oh.Magic = pedef::NT_OPTIONAL_HDR64_MAGIC;
        oh.MajorLinkerVersion = 1;
        oh.SizeOfCode = raw_size;
        oh.AddressOfEntryPoint = entry_rva;
        oh.BaseOfCode = text_rva;
        oh.ImageBase = image_base;
        oh.SectionAlignment = section_align;
        oh.FileAlignment = file_align;
        oh.MajorOperatingSystemVersion = 6;
        oh.MajorSubsystemVersion = 6;
        oh.SizeOfImage = align_up(text_rva + static_cast<std::uint32_t>(image.size()), section_align);
        oh.SizeOfHeaders = size_of_headers;
        oh.Subsystem = pedef::SUBSYSTEM_WINDOWS_CUI;
        // no DYNAMIC_BASE: see RELOCS_STRIPPED above
        oh.DllCharacteristics = 0;
        oh.SizeOfStackReserve = 0x100000;
        oh.SizeOfStackCommit = 0x1000;
        oh.SizeOfHeapReserve = 0x100000;
        oh.SizeOfHeapCommit = 0x1000;
        oh.NumberOfRvaAndSizes = pedef::NUMBEROF_DIRECTORY_ENTRIES;
        oh.DataDirectory[pedef::DIRECTORY_ENTRY_IMPORT] = import_dir;
        oh.DataDirectory[pedef::DIRECTORY_ENTRY_IAT] = iat_dir;
        const auto* oh_bytes = reinterpret_cast<const std::uint8_t*>(&oh);
        head.insert(head.end(), oh_bytes, oh_bytes + sizeof(oh));

        pedef::section_header sh = {};
        std::memcpy(sh.Name, ".text", 5);
        sh.VirtualSize = static_cast<std::uint32_t>(image.size());
        sh.VirtualAddress = text_rva;
        sh.SizeOfRawData = raw_size;
        sh.PointerToRawData = size_of_headers;
        sh.Characteristics = pedef::SCN_CNT_CODE | pedef::SCN_MEM_EXECUTE |
                             pedef::SCN_MEM_READ | pedef::SCN_MEM_WRITE;
        const auto* sh_bytes = reinterpret_cast<const std::uint8_t*>(&sh);
        head.insert(head.end(), sh_bytes, sh_bytes + sizeof(sh));

        head.resize(size_of_headers, 0);

        std::ofstream file(binary_name, std::ios::binary);
        if (!file) {
            std::cerr << RED << "[OCCULTC] Failed to create binary\n" << RESET;
            return;
        }

        file.write(reinterpret_cast<const char*>(head.data()), static_cast<std::streamsize>(head.size()));
        file.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        // the loader reads SizeOfRawData, so the file has to hold it
        if (raw_size > image.size()) {
            const std::vector<std::uint8_t> pad(raw_size - image.size(), 0);
            file.write(reinterpret_cast<const char*>(pad.data()), static_cast<std::streamsize>(pad.size()));
        }
        file.close();

        std::cout << GREEN << "[OCCULTC] PE binary created: " << RESET << binary_name << " ("
                  << (size_of_headers + raw_size) << " bytes, 1 section)\n";
    }

    std::vector<std::uint8_t> pe::build_exports(const std::vector<export_entry>& exports,
                                                 const std::string& dll_name,
                                                 std::uint32_t at_rva,
                                                 pedef::data_directory& export_dir) {
        std::vector<std::uint8_t> blob;
        if (exports.empty()) {
            export_dir = {0, 0};
            return blob;
        }

        // Sort by name for the ENPT. GetProcAddress binary-searches it.
        std::vector<std::size_t> order(exports.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return exports[a].name < exports[b].name;
        });

        const std::uint32_t dir_size = 40;
        const std::uint32_t eat_size = static_cast<std::uint32_t>(exports.size() * 4);
        const std::uint32_t enpt_size = static_cast<std::uint32_t>(exports.size() * 4);
        const std::uint32_t ord_size = static_cast<std::uint32_t>(exports.size() * 2);

        const std::uint32_t dir_at = 0;
        const std::uint32_t eat_at = dir_at + dir_size;
        const std::uint32_t enpt_at = eat_at + eat_size;
        const std::uint32_t ord_at = enpt_at + enpt_size;
        const std::uint32_t strings_at = ord_at + ord_size;

        blob.resize(strings_at, 0);

        // Append a string, return its RVA.
        auto add_string = [&](const std::string& s) {
            const std::uint32_t rva = at_rva + static_cast<std::uint32_t>(blob.size());
            blob.insert(blob.end(), s.begin(), s.end());
            blob.push_back(0);
            return rva;
        };

        const std::uint32_t dll_name_rva = add_string(dll_name);
        std::vector<std::uint32_t> name_rvas(exports.size());
        for (std::size_t ix : order) {
            name_rvas[ix] = add_string(exports[ix].name);
        }

        // IMAGE_EXPORT_DIRECTORY
        poke32(blob, dir_at + 0, 0);                               // Characteristics
        poke32(blob, dir_at + 4, 0);                               // TimeDateStamp
        poke32(blob, dir_at + 8, 0);                               // Version
        poke32(blob, dir_at + 12, dll_name_rva);                   // Name
        poke32(blob, dir_at + 16, 1);                              // Base
        poke32(blob, dir_at + 20, static_cast<std::uint32_t>(exports.size())); // NumberOfFunctions
        poke32(blob, dir_at + 24, static_cast<std::uint32_t>(exports.size())); // NumberOfNames
        poke32(blob, dir_at + 28, at_rva + eat_at);                // AddressOfFunctions
        poke32(blob, dir_at + 32, at_rva + enpt_at);               // AddressOfNames
        poke32(blob, dir_at + 36, at_rva + ord_at);                // AddressOfNameOrdinals

        for (std::size_t i = 0; i < exports.size(); ++i) {
            poke32(blob, eat_at + 4 * i, exports[i].rva);
        }

        // ENPT. Name RVAs in name-sorted order.
        for (std::size_t j = 0; j < order.size(); ++j) {
            poke32(blob, enpt_at + 4 * j, name_rvas[order[j]]);
        }

        for (std::size_t j = 0; j < order.size(); ++j) {
            const std::uint16_t ord = static_cast<std::uint16_t>(order[j]);
            blob[ord_at + 2 * j]     = static_cast<std::uint8_t>(ord & 0xFF);
            blob[ord_at + 2 * j + 1] = static_cast<std::uint8_t>((ord >> 8) & 0xFF);
        }

        export_dir.VirtualAddress = at_rva;
        export_dir.Size = static_cast<std::uint32_t>(blob.size());
        return blob;
    }

    void pe::generate_dll(const std::string& binary_name,
                          const std::vector<std::uint8_t>& image,
                          std::uint32_t entry_rva,
                          const pedef::data_directory& import_dir,
                          const pedef::data_directory& iat_dir,
                          const pedef::data_directory& export_dir,
                          const std::vector<std::size_t>& abs_relocs) {
        if (image.empty()) {
            std::cerr << RED << "[OCCULTC] No code to write\n" << RESET;
            return;
        }

        std::vector<std::uint8_t> reloc_bytes;
        if (!abs_relocs.empty()) {
            std::map<std::uint32_t, std::vector<std::uint16_t>> blocks;
            for (const auto image_off : abs_relocs) {
                const std::uint32_t rva = text_rva + static_cast<std::uint32_t>(image_off);
                const std::uint32_t page = rva & ~0xFFFu;
                const std::uint16_t within = static_cast<std::uint16_t>(rva - page);
                const std::uint16_t entry = static_cast<std::uint16_t>((10u << 12) | (within & 0xFFFu));
                blocks[page].push_back(entry);
            }
            for (auto& [page, entries] : blocks) {
                if (entries.size() & 1u) entries.push_back(0); // ABSOLUTE pad
                const std::uint32_t block_size = 8 + static_cast<std::uint32_t>(entries.size() * 2);
                put32(reloc_bytes, page);
                put32(reloc_bytes, block_size);
                for (auto e : entries) {
                    reloc_bytes.push_back(static_cast<std::uint8_t>(e & 0xFF));
                    reloc_bytes.push_back(static_cast<std::uint8_t>((e >> 8) & 0xFF));
                }
            }
        }

        const bool have_relocs = !reloc_bytes.empty();
        const std::uint16_t num_sections = have_relocs ? 2 : 1;

        std::vector<std::uint8_t> head;
        head.reserve(file_align);

        pedef::dos_header dos = {};
        dos.e_magic = pedef::DOS_SIGNATURE;
        dos.e_cblp = 0x90;
        dos.e_cp = 3;
        dos.e_cparhdr = 4;
        dos.e_maxalloc = 0xFFFF;
        dos.e_sp = 0xB8;
        dos.e_lfarlc = 0x40;
        dos.e_lfanew = 0x80;
        const auto* dos_bytes = reinterpret_cast<const std::uint8_t*>(&dos);
        head.insert(head.end(), dos_bytes, dos_bytes + sizeof(dos));

        static const std::uint8_t dos_stub[] = {
            0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
            0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21,
            'T', 'h', 'i', 's', ' ', 'p', 'r', 'o', 'g', 'r', 'a', 'm', ' ',
            'c', 'a', 'n', 'n', 'o', 't', ' ', 'b', 'e', ' ', 'r', 'u', 'n', ' ',
            'i', 'n', ' ', 'D', 'O', 'S', ' ', 'm', 'o', 'd', 'e', '.', '\r', '\r', '\n', '$'};
        head.insert(head.end(), std::begin(dos_stub), std::end(dos_stub));
        head.resize(0x80, 0);

        put32(head, pedef::NT_SIGNATURE);

        const std::uint32_t headers_raw =
            0x80 + 4 + static_cast<std::uint32_t>(sizeof(pedef::file_header) + sizeof(pedef::optional_header64) +
                                                   num_sections * sizeof(pedef::section_header));
        const std::uint32_t size_of_headers = align_up(headers_raw, file_align);
        const std::uint32_t text_raw = align_up(static_cast<std::uint32_t>(image.size()), file_align);
        const std::uint32_t text_vsize = static_cast<std::uint32_t>(image.size());
        const std::uint32_t reloc_rva = align_up(text_rva + text_vsize, section_align);
        const std::uint32_t reloc_raw = align_up(static_cast<std::uint32_t>(reloc_bytes.size()), file_align);
        const std::uint32_t reloc_vsize = static_cast<std::uint32_t>(reloc_bytes.size());

        pedef::file_header fh = {};
        fh.Machine = pedef::FILE_MACHINE_AMD64;
        fh.NumberOfSections = num_sections;
        fh.SizeOfOptionalHeader = sizeof(pedef::optional_header64);
        fh.Characteristics = pedef::FILE_EXECUTABLE_IMAGE |
                             pedef::FILE_LARGE_ADDRESS_AWARE | pedef::FILE_DLL;
        const auto* fh_bytes = reinterpret_cast<const std::uint8_t*>(&fh);
        head.insert(head.end(), fh_bytes, fh_bytes + sizeof(fh));

        pedef::optional_header64 oh = {};
        oh.Magic = pedef::NT_OPTIONAL_HDR64_MAGIC;
        oh.MajorLinkerVersion = 1;
        oh.SizeOfCode = text_raw;
        oh.AddressOfEntryPoint = entry_rva;
        oh.BaseOfCode = text_rva;
        oh.ImageBase = image_base;
        oh.SectionAlignment = section_align;
        oh.FileAlignment = file_align;
        oh.MajorOperatingSystemVersion = 6;
        oh.MajorSubsystemVersion = 6;
        oh.SizeOfImage = align_up(have_relocs ? (reloc_rva + reloc_vsize) : (text_rva + text_vsize), section_align);
        oh.SizeOfHeaders = size_of_headers;
        oh.Subsystem = pedef::SUBSYSTEM_WINDOWS_CUI;
        oh.DllCharacteristics = 0;
        oh.SizeOfStackReserve = 0x100000;
        oh.SizeOfStackCommit = 0x1000;
        oh.SizeOfHeapReserve = 0x100000;
        oh.SizeOfHeapCommit = 0x1000;
        oh.NumberOfRvaAndSizes = pedef::NUMBEROF_DIRECTORY_ENTRIES;
        oh.DataDirectory[pedef::DIRECTORY_ENTRY_EXPORT] = export_dir;
        oh.DataDirectory[pedef::DIRECTORY_ENTRY_IMPORT] = import_dir;
        oh.DataDirectory[pedef::DIRECTORY_ENTRY_IAT] = iat_dir;
        if (have_relocs) {
            oh.DataDirectory[pedef::DIRECTORY_ENTRY_BASERELOC] = {reloc_rva, reloc_vsize};
        }
        const auto* oh_bytes = reinterpret_cast<const std::uint8_t*>(&oh);
        head.insert(head.end(), oh_bytes, oh_bytes + sizeof(oh));

        pedef::section_header sh_text = {};
        std::memcpy(sh_text.Name, ".text", 5);
        sh_text.VirtualSize = text_vsize;
        sh_text.VirtualAddress = text_rva;
        sh_text.SizeOfRawData = text_raw;
        sh_text.PointerToRawData = size_of_headers;
        sh_text.Characteristics = pedef::SCN_CNT_CODE | pedef::SCN_MEM_EXECUTE |
                                  pedef::SCN_MEM_READ | pedef::SCN_MEM_WRITE;
        const auto* sh_text_bytes = reinterpret_cast<const std::uint8_t*>(&sh_text);
        head.insert(head.end(), sh_text_bytes, sh_text_bytes + sizeof(sh_text));

        if (have_relocs) {
            pedef::section_header sh_reloc = {};
            std::memcpy(sh_reloc.Name, ".reloc", 6);
            sh_reloc.VirtualSize = reloc_vsize;
            sh_reloc.VirtualAddress = reloc_rva;
            sh_reloc.SizeOfRawData = reloc_raw;
            sh_reloc.PointerToRawData = size_of_headers + text_raw;
            sh_reloc.Characteristics = pedef::SCN_CNT_INITIALIZED_DATA |
                                       pedef::SCN_MEM_DISCARDABLE | pedef::SCN_MEM_READ;
            const auto* sh_reloc_bytes = reinterpret_cast<const std::uint8_t*>(&sh_reloc);
            head.insert(head.end(), sh_reloc_bytes, sh_reloc_bytes + sizeof(sh_reloc));
        }

        head.resize(size_of_headers, 0);

        std::ofstream file(binary_name, std::ios::binary);
        if (!file) {
            std::cerr << RED << "[OCCULTC] Failed to create DLL\n" << RESET;
            return;
        }
        file.write(reinterpret_cast<const char*>(head.data()), static_cast<std::streamsize>(head.size()));
        file.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        if (text_raw > image.size()) {
            const std::vector<std::uint8_t> pad(text_raw - image.size(), 0);
            file.write(reinterpret_cast<const char*>(pad.data()), static_cast<std::streamsize>(pad.size()));
        }
        if (have_relocs) {
            file.write(reinterpret_cast<const char*>(reloc_bytes.data()), static_cast<std::streamsize>(reloc_bytes.size()));
            if (reloc_raw > reloc_bytes.size()) {
                const std::vector<std::uint8_t> pad(reloc_raw - reloc_bytes.size(), 0);
                file.write(reinterpret_cast<const char*>(pad.data()), static_cast<std::streamsize>(pad.size()));
            }
        }
        file.close();

        std::cout << GREEN << "[OCCULTC] PE DLL created: " << RESET << binary_name << " ("
                  << (size_of_headers + text_raw + (have_relocs ? reloc_raw : 0u)) << " bytes, "
                  << num_sections << " sections, " << abs_relocs.size() << " reloc entries)\n";
    }
} // namespace occult
