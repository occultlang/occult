#include "linker.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace occult {
    void linker::link_blob(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                           const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                           const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs, std::int32_t globals_size,
                           bool debug, bool showtime,
                           const std::vector<elf_import_use>& imports, const std::vector<std::string>& needed) {
        auto start = std::chrono::high_resolution_clock::now();

        auto main_it = function_locs.find("main");
        if (main_it == function_locs.end()) {
            std::cerr << RED << "[LINKER ERROR] No 'main' function found\n" << RESET;
            return;
        }

        const bool dynamic = !imports.empty();
        constexpr std::uint64_t base_addr = 0x400000;
        const std::uint64_t header_size = dynamic ? elf::dynamic_header_size()
                                                  : sizeof(elf_header) + sizeof(elf_program_header);
        const std::uint64_t entry_addr = base_addr + header_size;

        auto init_it = function_locs.find("__global_init");
        const bool has_init = (init_it != function_locs.end());
        const std::size_t stub_size = has_init ? 29 : 24;

        std::vector<std::uint8_t> final_code;
        final_code.reserve(stub_size + code.size());

        if (has_init) {
            const std::int32_t rel_init = static_cast<std::int32_t>(stub_size + init_it->second) - 5;
            final_code.push_back(0xE8);
            for (int j = 0; j < 4; ++j) final_code.push_back(static_cast<std::uint8_t>((rel_init >> (j * 8)) & 0xFF));
        }
        final_code.insert(final_code.end(), {0x48, 0x8B, 0x3C, 0x24,
                                             0x48, 0x8D, 0x74, 0x24, 0x08});
        const std::int32_t rel = static_cast<std::int32_t>(stub_size + main_it->second) - static_cast<std::int32_t>(final_code.size() + 5);
        final_code.push_back(0xE8);
        for (int j = 0; j < 4; ++j) final_code.push_back(static_cast<std::uint8_t>((rel >> (j * 8)) & 0xFF));
        std::size_t exit_field = 0;
        if (dynamic) {
            final_code.insert(final_code.end(), {0x48, 0x89, 0xC7, 0xFF, 0x15}); // mov rdi, rax; call [rip+disp32]
            exit_field = final_code.size();
            final_code.insert(final_code.end(), {0x00, 0x00, 0x00, 0x00, 0xF4}); // disp32; hlt
        }
        else {
            final_code.insert(final_code.end(), {0x48, 0x89, 0xC7, 0xB8, 0x3C, 0x00, 0x00, 0x00, 0x0F, 0x05});
        }

        final_code.insert(final_code.end(), code.begin(), code.end());
        const std::size_t text_size = final_code.size();

        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;
        for (const auto& [old_addr, content] : string_literals) {
            const std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) final_code.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            const std::uint64_t data_off = final_code.size();
            final_code.insert(final_code.end(), content.begin(), content.end());
            final_code.insert(final_code.end(), 8, 0);
            relocation_map[old_addr] = base_addr + header_size + data_off;
        }

        for (const auto& [code_off, old_addr] : string_relocs) {
            auto it = relocation_map.find(old_addr);
            if (it == relocation_map.end()) {
                continue;
            }
            const std::size_t at = stub_size + code_off;
            for (int j = 0; j < 8; ++j) final_code[at + j] = static_cast<std::uint8_t>((it->second >> (j * 8)) & 0xFF);

            if (debug) {
                std::cout << BLUE << "[LINKER INFO] Patched string pointer @0x" << std::hex << at << " -> 0x" << it->second << std::dec << RESET << "\n";
            }
        }

        if (globals_size > 0) {
            const std::uint64_t globals_base = base_addr + header_size + final_code.size();
            final_code.insert(final_code.end(), static_cast<std::size_t>(globals_size), 0);

            for (const auto& [code_off, global_off] : global_relocs) {
                const std::uint64_t addr = globals_base + static_cast<std::uint64_t>(global_off);
                const std::size_t at = stub_size + code_off;
                for (int j = 0; j < 8; ++j) final_code[at + j] = static_cast<std::uint8_t>((addr >> (j * 8)) & 0xFF);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking \033[0m" << duration.count() << "ms\n";
        }

        if (dynamic) {
            std::vector<std::pair<std::string, std::size_t>> slot_uses{{"exit", exit_field}};
            for (const auto& use : imports) {
                slot_uses.emplace_back(use.name, stub_size + use.field);
            }
            elf::generate_dynamic_binary(binary_name, final_code, base_addr, header_size, text_size, needed, slot_uses);
            return;
        }

        elf::generate_binary(binary_name, final_code, entry_addr, base_addr, base_addr, 0, text_size);
    }

    void linker::link_blob_so(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                              const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                              const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs,
                              std::int32_t globals_size,
                              const std::vector<std::pair<std::string, std::string>>& exports,
                              const std::vector<elf_import_use>& imports,
                              const std::vector<std::string>& needed,
                              bool debug, bool showtime) {
        auto start = std::chrono::high_resolution_clock::now();

        const std::size_t header_size = elf::shared_header_size();

        std::vector<std::uint8_t> image(code.begin(), code.end());
        const std::size_t text_size = image.size();

        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;
        for (const auto& [old_addr, content] : string_literals) {
            const std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) image.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            const std::uint64_t data_off = image.size();
            image.insert(image.end(), content.begin(), content.end());
            image.insert(image.end(), 8, 0);
            relocation_map[old_addr] = header_size + data_off;
        }

        std::vector<std::size_t> abs_relocs;
        for (const auto& [code_off, old_addr] : string_relocs) {
            auto it = relocation_map.find(old_addr);
            if (it == relocation_map.end()) continue;
            for (int j = 0; j < 8; ++j) image[code_off + j] = static_cast<std::uint8_t>((it->second >> (j * 8)) & 0xFF);
            abs_relocs.push_back(code_off);
        }

        if (globals_size > 0) {
            const std::uint64_t globals_base = header_size + image.size();
            image.insert(image.end(), static_cast<std::size_t>(globals_size), 0);
            for (const auto& [code_off, global_off] : global_relocs) {
                const std::uint64_t addr = globals_base + static_cast<std::uint64_t>(global_off);
                for (int j = 0; j < 8; ++j) image[code_off + j] = static_cast<std::uint8_t>((addr >> (j * 8)) & 0xFF);
                abs_relocs.push_back(code_off);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking \033[0m" << duration.count() << "ms\n";
        }

        std::vector<std::pair<std::string, std::size_t>> slot_uses;
        for (const auto& use : imports) {
            slot_uses.emplace_back(use.name, use.field);
        }

        std::vector<std::pair<std::string, std::size_t>> resolved_exports;
        for (const auto& [internal, exported] : exports) {
            auto it = function_locs.find(internal);
            if (it == function_locs.end()) {
                std::cerr << RED << "[LINKER ERROR] exported function '" << internal << "' has no code\n" << RESET;
                continue;
            }
            resolved_exports.emplace_back(exported, static_cast<std::size_t>(it->second));
        }

        std::filesystem::path p(binary_name);
        std::string soname = p.filename().string();

        elf::generate_shared_object(binary_name, image, header_size, text_size, soname,
                                    resolved_exports, needed, slot_uses, abs_relocs);
    }

    void linker::link_blob_pe(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                              const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                              const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs, std::int32_t globals_size,
                              bool main_takes_args,
                              const std::vector<pe_import_use>& import_thunks,
                              bool debug, bool showtime) {
        auto start = std::chrono::high_resolution_clock::now();

        auto lower = [](std::string v) {
            for (auto& ch : v) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return v;
        };
        const bool uses_crt = main_takes_args || std::any_of(import_thunks.begin(), import_thunks.end(), [&](const pe_import_use& use) {
            return lower(use.dll) == "msvcrt.dll";
        });

        auto main_it = function_locs.find("main");
        if (main_it == function_locs.end()) {
            std::cerr << RED << "[LINKER ERROR] No 'main' function found\n" << RESET;
            return;
        }

        auto init_it = function_locs.find("__global_init");
        const bool has_init = (init_it != function_locs.end());

        const std::size_t stub_size = (main_takes_args ? 68u : 17u) + (has_init ? 5u : 0u);

        std::vector<std::uint8_t> image;
        image.reserve(stub_size + code.size());

        auto emit = [&](std::initializer_list<std::uint8_t> bytes) {
            image.insert(image.end(), bytes);
        };
        auto emit32 = [&](std::int32_t x) {
            for (int i = 0; i < 4; ++i) image.push_back(static_cast<std::uint8_t>((x >> (i * 8)) & 0xFF));
        };
        // a call whose target is an offset into the image
        auto call_rel = [&](std::size_t target) {
            image.push_back(0xE8);
            emit32(static_cast<std::int32_t>(target) - static_cast<std::int32_t>(image.size() + 4));
        };
        std::vector<std::pair<std::string, std::size_t>> iat_calls;
        auto call_import = [&](const char* name) {
            emit({0xFF, 0x15});
            iat_calls.emplace_back(name, image.size());
            emit32(0);
        };

        if (main_takes_args) {
            emit({0x48, 0x83, 0xEC, 0x58});             // sub rsp, 0x58
        }
        else {
            emit({0x48, 0x83, 0xEC, 0x28});             // sub rsp, 0x28
        }

        if (has_init) {
            call_rel(stub_size + init_it->second);
        }

        if (main_takes_args) {
            emit({0xC7, 0x44, 0x24, 0x48, 0x00, 0x00, 0x00, 0x00}); // mov dword [rsp+0x48], 0
            emit({0x48, 0x8D, 0x44, 0x24, 0x48});                   // lea rax, [rsp+0x48]
            emit({0x48, 0x89, 0x44, 0x24, 0x20});                   // mov [rsp+0x20], rax
            emit({0x48, 0x8D, 0x4C, 0x24, 0x30});                   // lea rcx, [rsp+0x30]
            emit({0x48, 0x8D, 0x54, 0x24, 0x38});                   // lea rdx, [rsp+0x38]
            emit({0x4C, 0x8D, 0x44, 0x24, 0x40});                   // lea r8,  [rsp+0x40]
            emit({0x45, 0x31, 0xC9});                               // xor r9d, r9d
            call_import("__getmainargs");
            emit({0x8B, 0x4C, 0x24, 0x30});                         // mov ecx, [rsp+0x30]
            emit({0x48, 0x8B, 0x54, 0x24, 0x38});                   // mov rdx, [rsp+0x38]
        }

        call_rel(stub_size + main_it->second);
        emit({0x89, 0xC1});                                         // mov ecx, eax
        call_import(uses_crt ? "exit" : "ExitProcess");

        if (image.size() != stub_size) {
            std::cerr << RED << "[LINKER ERROR] entry stub is " << image.size()
                      << " bytes, laid out for " << stub_size << "\n" << RESET;
            return;
        }

        image.insert(image.end(), code.begin(), code.end());

        const std::uint64_t data_base = pe::image_base + pe::text_rva;

        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;
        for (const auto& [old_addr, content] : string_literals) {
            const std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) image.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            const std::uint64_t data_off = image.size();
            image.insert(image.end(), content.begin(), content.end());
            image.insert(image.end(), 8, 0);
            relocation_map[old_addr] = data_base + data_off;
        }

        for (const auto& [code_off, old_addr] : string_relocs) {
            auto it = relocation_map.find(old_addr);
            if (it == relocation_map.end()) {
                continue;
            }
            const std::size_t at = stub_size + code_off;
            for (int j = 0; j < 8; ++j) image[at + j] = static_cast<std::uint8_t>((it->second >> (j * 8)) & 0xFF);

            if (debug) {
                std::cout << BLUE << "[LINKER INFO] Patched string pointer @0x" << std::hex << at << " -> 0x" << it->second << std::dec << RESET << "\n";
            }
        }

        if (globals_size > 0) {
            const std::uint64_t globals_base = data_base + image.size();
            image.insert(image.end(), static_cast<std::size_t>(globals_size), 0);

            for (const auto& [code_off, global_off] : global_relocs) {
                const std::uint64_t addr = globals_base + static_cast<std::uint64_t>(global_off);
                const std::size_t at = stub_size + code_off;
                for (int j = 0; j < 8; ++j) image[at + j] = static_cast<std::uint8_t>((addr >> (j * 8)) & 0xFF);
            }
        }

        // the import directory goes last, so its address is known only now
        while (image.size() % 8) image.push_back(0);
        const std::uint32_t import_rva = pe::text_rva + static_cast<std::uint32_t>(image.size());

        std::vector<pe::import> imports;
        imports.push_back({"KERNEL32.dll", {"ExitProcess"}});
        if (main_takes_args) {
            imports.push_back({"msvcrt.dll", {"__getmainargs"}});
        }
        if (uses_crt) {
            auto crt = std::find_if(imports.begin(), imports.end(), [&](const pe::import& im) { return lower(im.dll) == "msvcrt.dll"; });
            if (crt == imports.end()) {
                imports.push_back({"msvcrt.dll", {}});
                crt = std::prev(imports.end());
            }
            crt->names.push_back("exit");
        }

        std::unordered_map<std::string, std::string> dll_of = {{"ExitProcess", "kernel32.dll"}};
        if (main_takes_args) {
            dll_of["__getmainargs"] = "msvcrt.dll";
        }
        if (uses_crt) {
            dll_of["exit"] = "msvcrt.dll";
        }
        for (const auto& use : import_thunks) {
            const std::string dll = lower(use.dll);
            if (auto seen = dll_of.find(use.name); seen != dll_of.end()) {
                if (seen->second != dll) {
                    std::cerr << RED << "[LINKER ERROR] \"" << use.name << "\" is imported from both "
                              << seen->second << " and " << dll << "\n" << RESET;
                    return;
                }
                continue;
            }
            dll_of[use.name] = dll;
            auto it = std::find_if(imports.begin(), imports.end(), [&](const pe::import& im) { return lower(im.dll) == dll; });
            if (it == imports.end()) {
                imports.push_back({use.dll, {}});
                it = std::prev(imports.end());
            }
            it->names.push_back(use.name);
        }

        pe::iat_map iat;
        pedef::data_directory import_dir{}, iat_dir{};
        const std::vector<std::uint8_t> import_blob = pe::build_imports(imports, import_rva, iat, import_dir, iat_dir);
        image.insert(image.end(), import_blob.begin(), import_blob.end());

        for (const auto& [name, field] : iat_calls) {
            auto slot = iat.find(name);
            if (slot == iat.end()) {
                std::cerr << RED << "[LINKER ERROR] no import slot for " << name << "\n" << RESET;
                return;
            }
            // rip is the end of the instruction, which is the end of the field
            const std::int32_t disp = static_cast<std::int32_t>(slot->second) -
                                      static_cast<std::int32_t>(pe::text_rva + field + 4);
            for (int j = 0; j < 4; ++j) image[field + j] = static_cast<std::uint8_t>((disp >> (j * 8)) & 0xFF);
        }
        // a thunk's field is an offset into the code, which sits after the stub
        for (const auto& use : import_thunks) {
            const std::size_t field = stub_size + use.field;
            const std::int32_t disp = static_cast<std::int32_t>(iat.at(use.name)) -
                                      static_cast<std::int32_t>(pe::text_rva + field + 4);
            for (int j = 0; j < 4; ++j) image[field + j] = static_cast<std::uint8_t>((disp >> (j * 8)) & 0xFF);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking \033[0m" << duration.count() << "ms\n";
        }

        pe::generate_binary(binary_name, image, pe::text_rva, import_dir, iat_dir);
    }

    void linker::link_blob_dll(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                               const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                               const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs,
                               std::int32_t globals_size,
                               const std::vector<std::pair<std::string, std::string>>& exports,
                               const std::vector<pe_import_use>& import_thunks,
                               bool debug, bool showtime) {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::uint8_t> image(code.begin(), code.end());

        std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;
        for (const auto& [old_addr, content] : string_literals) {
            const std::uint64_t len = content.size();
            for (int j = 0; j < 8; ++j) image.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));
            const std::uint64_t data_off = image.size();
            image.insert(image.end(), content.begin(), content.end());
            image.insert(image.end(), 8, 0);
            relocation_map[old_addr] = pe::image_base + pe::text_rva + data_off;
        }
        std::vector<std::size_t> abs_relocs;
        for (const auto& [code_off, old_addr] : string_relocs) {
            auto it = relocation_map.find(old_addr);
            if (it == relocation_map.end()) continue;
            for (int j = 0; j < 8; ++j) image[code_off + j] = static_cast<std::uint8_t>((it->second >> (j * 8)) & 0xFF);
            abs_relocs.push_back(code_off);
        }
        if (globals_size > 0) {
            const std::uint64_t globals_base = pe::image_base + pe::text_rva + image.size();
            image.insert(image.end(), static_cast<std::size_t>(globals_size), 0);
            for (const auto& [code_off, global_off] : global_relocs) {
                const std::uint64_t addr = globals_base + static_cast<std::uint64_t>(global_off);
                for (int j = 0; j < 8; ++j) image[code_off + j] = static_cast<std::uint8_t>((addr >> (j * 8)) & 0xFF);
                abs_relocs.push_back(code_off);
            }
        }

        // Imports (if any). Build in a temp blob so the RVA is known.
        std::vector<pe::import> imports;
        std::unordered_map<std::string, std::vector<std::string>> by_dll;
        std::vector<std::string> dll_order;
        for (const auto& use : import_thunks) {
            if (by_dll.find(use.dll) == by_dll.end()) dll_order.push_back(use.dll);
            by_dll[use.dll].push_back(use.name);
        }
        for (const auto& dll : dll_order) {
            auto& names = by_dll[dll];
            std::sort(names.begin(), names.end());
            names.erase(std::unique(names.begin(), names.end()), names.end());
            imports.push_back({dll, names});
        }
        const std::uint32_t imports_rva = pe::text_rva + static_cast<std::uint32_t>(image.size());
        pe::iat_map iat;
        pedef::data_directory import_dir{}, iat_dir{};
        auto imports_blob = pe::build_imports(imports, imports_rva, iat, import_dir, iat_dir);
        image.insert(image.end(), imports_blob.begin(), imports_blob.end());

        // Patch each `call [rip+disp32]` import site now that the IAT is laid out.
        for (const auto& use : import_thunks) {
            if (iat.find(use.name) == iat.end()) continue;
            const std::size_t field = use.field;
            const std::int32_t disp = static_cast<std::int32_t>(iat.at(use.name)) -
                                      static_cast<std::int32_t>(pe::text_rva + field + 4);
            for (int j = 0; j < 4; ++j) image[field + j] = static_cast<std::uint8_t>((disp >> (j * 8)) & 0xFF);
        }

        // Exports. Resolve each function's RVA, then build the directory.
        std::vector<pe::export_entry> export_entries;
        for (const auto& [internal, exported] : exports) {
            auto it = function_locs.find(internal);
            if (it == function_locs.end()) {
                std::cerr << RED << "[LINKER ERROR] exported function '" << internal << "' has no code\n" << RESET;
                continue;
            }
            export_entries.push_back({exported, pe::text_rva + it->second});
        }

        std::filesystem::path p(binary_name);
        std::string dll_name = p.filename().string();

        const std::uint32_t exports_rva = pe::text_rva + static_cast<std::uint32_t>(image.size());
        pedef::data_directory export_dir{};
        auto exports_blob = pe::build_exports(export_entries, dll_name, exports_rva, export_dir);
        image.insert(image.end(), exports_blob.begin(), exports_blob.end());

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        if (showtime) {
            std::cout << GREEN << "[OCCULTC] Completed linking \033[0m" << duration.count() << "ms\n";
        }

        pe::generate_dll(binary_name, image, 0, import_dir, iat_dir, export_dir, abs_relocs);
    }

} // namespace occult
