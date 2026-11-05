#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "../codegen/x86_64_writer.hpp"
#include "elf_header.hpp"
#include "pe_header.hpp"
#include "pe_header.hpp"

namespace occult {

    struct pe_import_use {
        std::string dll;
        std::string name;
        std::size_t field;
    };

    struct elf_import_use {
        std::string name;
        std::size_t field;
    };

    class linker {
    public:
        static void link_blob(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                              const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                              const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs = {}, std::int32_t globals_size = 0,
                              bool debug = false, bool showtime = false,
                              const std::vector<elf_import_use>& imports = {}, const std::vector<std::string>& needed = {});

        static void link_blob_so(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                                 const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                                 const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs,
                                 std::int32_t globals_size,
                                 const std::vector<std::pair<std::string, std::string>>& exports,
                                 const std::vector<elf_import_use>& imports,
                                 const std::vector<std::string>& needed,
                                 bool debug, bool showtime);

        static void link_blob_dll(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                                  const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                                  const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs,
                                  std::int32_t globals_size,
                                  const std::vector<std::pair<std::string, std::string>>& exports,
                                  const std::vector<pe_import_use>& import_thunks,
                                  bool debug, bool showtime);

        static void link_blob_pe(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                                 const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                                 const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs = {}, std::int32_t globals_size = 0,
                                 bool main_takes_args = false,
                                 const std::vector<pe_import_use>& import_thunks = {},
                                 bool debug = false, bool showtime = false);
    };

} // namespace occult
